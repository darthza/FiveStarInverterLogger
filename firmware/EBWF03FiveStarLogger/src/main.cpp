#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#if !FIVESTAR_RECOVERY_ONLY
#include <PubSubClient.h>
#endif

#include <config.h>

namespace {
constexpr uint32_t DebugBaud = 115200;
constexpr uint32_t WifiConnectTimeoutMs = 20000;
constexpr uint32_t OtaWarmupMs = 60000;
constexpr uint32_t HeartbeatIntervalMs = 5000;
constexpr uint32_t StatusIntervalMs = 30000;
constexpr float WifiOutputPowerDbm = 10.5f;

#if FIVESTAR_RECOVERY_ONLY
constexpr const char *FirmwareVersion = "recovery-0.4.0";
#else
constexpr uint32_t InverterBaud = 2400;
constexpr uint32_t FirstPollDelayMs = 65000;
constexpr uint32_t PollIntervalMs = 30000;
constexpr uint32_t CommandTimeoutMs = 1500;
constexpr uint32_t MqttReconnectIntervalMs = 10000;
constexpr const char *FirmwareVersion = "0.3.0";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

uint32_t nextPollMs = FirstPollDelayMs;
uint32_t lastMqttConnectAttemptMs = 0;
uint32_t inverterTimeoutCount = 0;
uint32_t successfulPollCount = 0;
#endif

uint32_t lastHeartbeatMs = 0;
#if !FIVESTAR_RECOVERY_ONLY
uint32_t lastStatusMs = 0;
#endif
bool wifiConnected = false;

void debugPrintln(const __FlashStringHelper *message) {
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(message);
#else
  Serial1.println(message);
#endif
}

void debugPrint(const __FlashStringHelper *message) {
#if FIVESTAR_RECOVERY_ONLY
  Serial.print(message);
#else
  Serial1.print(message);
#endif
}

void debugPrintlnString(const String &message) {
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(message);
#else
  Serial1.println(message);
#endif
}

void debugPrintlnNumber(int32_t value) {
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(value);
#else
  Serial1.println(value);
#endif
}

void serviceBackground() {
  ArduinoOTA.handle();
#if !FIVESTAR_RECOVERY_ONLY
#if MQTT_ENABLED
  mqtt.loop();
#endif
#endif
  yield();
}

void startFallbackAp() {
  String apName = String("fivestar-recovery-") + String(ESP.getChipId(), HEX);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str());
  debugPrint(F("Fallback AP started: "));
  debugPrintlnString(apName);
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(OTA_HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(WifiOutputPowerDbm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  debugPrint(F("Connecting WiFi"));
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WifiConnectTimeoutMs) {
    delay(250);
    debugPrint(F("."));
    serviceBackground();
  }

  if (WiFi.status() == WL_CONNECTED) {
    debugPrintln(F(""));
    debugPrint(F("WiFi connected: "));
    debugPrintlnString(WiFi.localIP().toString());
    wifiConnected = true;
    return true;
  }

  debugPrintln(F(""));
  debugPrintln(F("WiFi connect timeout"));
  wifiConnected = false;
  startFallbackAp();
  return false;
}

void setupOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    debugPrintln(F("OTA start"));
  });
  ArduinoOTA.onEnd([]() {
    debugPrintln(F("OTA end"));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    debugPrint(F("OTA error "));
    debugPrintlnNumber(static_cast<int32_t>(error));
  });
  ArduinoOTA.begin();
  debugPrintln(F("OTA ready"));
}

#if !FIVESTAR_RECOVERY_ONLY
struct Q1Snapshot {
  bool valid = false;
  float inputVoltage = NAN;
  float bypassVoltage = NAN;
  float outputVoltage = NAN;
  int loadPercent = -1;
  float outputFrequency = NAN;
  float outputCurrent = NAN;
  float batteryVoltage = NAN;
  String statusBits;
  String raw;
};

void publishText(const char *suffix, const String &value, bool retained = true) {
#if MQTT_ENABLED
  if (!mqtt.connected()) {
    return;
  }
  String topic = String(MQTT_TOPIC_PREFIX) + "/" + suffix;
  mqtt.publish(topic.c_str(), value.c_str(), retained);
#else
  (void)suffix;
  (void)value;
  (void)retained;
#endif
}

void publishNumber(const char *suffix, float value, uint8_t decimals = 1) {
  if (!isnan(value)) {
    publishText(suffix, String(value, decimals));
  }
}

void publishInteger(const char *suffix, int value) {
  if (value >= 0) {
    publishText(suffix, String(value));
  }
}

void publishStatus() {
  publishText("status/version", FirmwareVersion);
  publishText("status/reset_reason", ESP.getResetReason());
  publishText("status/ip", WiFi.localIP().toString());
  publishText("status/rssi", String(WiFi.RSSI()));
  publishText("status/uptime_ms", String(millis()), false);
  publishText("status/free_heap", String(ESP.getFreeHeap()), false);
  publishText("status/inverter_timeouts", String(inverterTimeoutCount), false);
  publishText("status/successful_polls", String(successfulPollCount), false);
}

void connectMqtt() {
#if MQTT_ENABLED
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(256);

  if (mqtt.connected()) {
    return;
  }

  uint32_t now = millis();
  if (now - lastMqttConnectAttemptMs < MqttReconnectIntervalMs) {
    return;
  }
  lastMqttConnectAttemptMs = now;

  String clientId = String(OTA_HOSTNAME) + "-" + String(ESP.getChipId(), HEX);
  bool connected = strlen(MQTT_USER) > 0
                       ? mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)
                       : mqtt.connect(clientId.c_str());

  debugPrintln(connected ? F("MQTT connected") : F("MQTT connect failed"));
  if (connected) {
    publishStatus();
  }
#endif
}

String readLineFromInverter(uint32_t timeoutMs) {
  String response;
  response.reserve(96);
  uint32_t started = millis();

  while (millis() - started < timeoutMs) {
    serviceBackground();
    while (Serial.available() > 0) {
      char c = static_cast<char>(Serial.read());
      if (c == '\r' || c == '\n') {
        if (response.length() > 0) {
          return response;
        }
        continue;
      }
      if (isPrintable(c)) {
        response += c;
      }
    }
    delay(1);
  }

  return response;
}

String sendCommand(const char *command) {
  while (Serial.available() > 0) {
    Serial.read();
  }

  Serial.print(command);
  Serial.print('\r');
  String response = readLineFromInverter(CommandTimeoutMs);

  if (response.length() == 0) {
    inverterTimeoutCount++;
    publishText("status/last_timeout_command", command, false);
    publishText("status/inverter_timeouts", String(inverterTimeoutCount), false);
  }

  return response;
}

bool parseQ1(const String &raw, Q1Snapshot &snapshot) {
  snapshot = Q1Snapshot{};
  snapshot.raw = raw;
  if (!raw.startsWith("(")) {
    return false;
  }

  String body = raw.substring(1);
  body.trim();

  char buffer[128];
  body.toCharArray(buffer, sizeof(buffer));

  char *fields[8] = {};
  uint8_t count = 0;
  char *token = strtok(buffer, " ");
  while (token != nullptr && count < 8) {
    fields[count++] = token;
    token = strtok(nullptr, " ");
  }

  if (count < 8) {
    return false;
  }

  snapshot.inputVoltage = atof(fields[0]);
  snapshot.bypassVoltage = atof(fields[1]);
  snapshot.outputVoltage = atof(fields[2]);
  snapshot.loadPercent = atoi(fields[3]);
  snapshot.outputFrequency = atof(fields[4]);
  snapshot.outputCurrent = atof(fields[5]);
  snapshot.batteryVoltage = atof(fields[6]);
  snapshot.statusBits = fields[7];
  snapshot.valid = true;
  return true;
}

void publishSnapshot(const Q1Snapshot &snapshot) {
  publishText("raw/q1", snapshot.raw);
  publishNumber("input_voltage", snapshot.inputVoltage);
  publishNumber("bypass_voltage", snapshot.bypassVoltage);
  publishNumber("output_voltage", snapshot.outputVoltage);
  publishInteger("load_percent", snapshot.loadPercent);
  publishNumber("output_frequency", snapshot.outputFrequency);
  publishNumber("output_current", snapshot.outputCurrent, 2);
  publishNumber("battery_voltage", snapshot.batteryVoltage);
  publishText("status_bits", snapshot.statusBits);
}

void pollInverter() {
  publishText("status/poll_started_ms", String(millis()), false);
  String rawQ1 = sendCommand("Q1");
  Q1Snapshot snapshot;
  if (parseQ1(rawQ1, snapshot)) {
    successfulPollCount++;
    publishSnapshot(snapshot);
    publishText("status/last_poll_ms", String(millis()), false);
    publishText("status/successful_polls", String(successfulPollCount), false);
  } else {
    publishText("raw/q1_parse_error", rawQ1.length() > 0 ? rawQ1 : String("<timeout>"), false);
  }
}
#endif
} // namespace

void setup() {
#if FIVESTAR_RECOVERY_ONLY
  Serial.begin(DebugBaud);
#else
  Serial1.begin(DebugBaud);
  Serial.begin(InverterBaud, SERIAL_8N1);
  Serial.setTimeout(CommandTimeoutMs);
#endif

  debugPrint(F("FiveStar firmware "));
  debugPrintlnString(FirmwareVersion);
  debugPrint(F("Reset reason: "));
  debugPrintlnString(ESP.getResetReason());

  connectWifi();
  setupOta();

#if !FIVESTAR_RECOVERY_ONLY
  connectMqtt();
  debugPrintln(F("OTA warmup before inverter polling"));
#endif
}

void loop() {
  serviceBackground();

#if !FIVESTAR_RECOVERY_ONLY
#if MQTT_ENABLED
  if (!mqtt.connected()) {
    connectMqtt();
  }
#endif
#endif

  uint32_t now = millis();

  if (now - lastHeartbeatMs >= HeartbeatIntervalMs) {
    lastHeartbeatMs = now;
    debugPrint(F("heartbeat uptime_ms="));
    debugPrintlnNumber(static_cast<int32_t>(now));
  }

#if !FIVESTAR_RECOVERY_ONLY
  if (now - lastStatusMs >= StatusIntervalMs) {
    lastStatusMs = now;
    publishStatus();
  }

  if (static_cast<int32_t>(now - nextPollMs) >= 0) {
    nextPollMs = now + PollIntervalMs;
    pollInverter();
  }
#endif
}
