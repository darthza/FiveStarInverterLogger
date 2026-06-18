#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#ifndef FIVESTAR_CONNECTED_DIAGNOSTIC
#define FIVESTAR_CONNECTED_DIAGNOSTIC 0
#endif

#ifndef FIVESTAR_PIN_DIAGNOSTIC
#define FIVESTAR_PIN_DIAGNOSTIC 0
#endif

#ifndef FIVESTAR_SERIAL_DEBUG
#define FIVESTAR_SERIAL_DEBUG 1
#endif

#ifndef FIVESTAR_LOW_POWER
#define FIVESTAR_LOW_POWER 0
#endif

#ifndef FIVESTAR_ULTRA_LOW_POWER
#define FIVESTAR_ULTRA_LOW_POWER 0
#endif

#ifndef FIVESTAR_FAST_POWER_MONITOR
#define FIVESTAR_FAST_POWER_MONITOR 0
#endif

#if !FIVESTAR_RECOVERY_ONLY
#include <PubSubClient.h>
#endif

#include <config.h>

namespace {
constexpr uint32_t DebugBaud = 115200;
#if FIVESTAR_PIN_DIAGNOSTIC
constexpr uint32_t WifiConnectTimeoutMs = 12000;
#else
constexpr uint32_t WifiConnectTimeoutMs = 20000;
#endif
constexpr uint32_t OtaWarmupMs = 60000;
constexpr uint32_t HeartbeatIntervalMs = 5000;
constexpr uint32_t StatusIntervalMs = 30000;
#if FIVESTAR_ULTRA_LOW_POWER
constexpr float WifiOutputPowerDbm = -1.0f;
#elif FIVESTAR_LOW_POWER
constexpr float WifiOutputPowerDbm = 0.0f;
#else
constexpr float WifiOutputPowerDbm = 10.5f;
#endif

#if FIVESTAR_RECOVERY_ONLY
constexpr const char *FirmwareVersion = "recovery-0.4.0";
#elif FIVESTAR_PIN_DIAGNOSTIC && FIVESTAR_ULTRA_LOW_POWER
constexpr const char *FirmwareVersion = "pin-diag-ultra-low-power-0.1.0";
#elif FIVESTAR_PIN_DIAGNOSTIC && FIVESTAR_LOW_POWER
constexpr const char *FirmwareVersion = "pin-diag-low-power-0.1.0";
#elif FIVESTAR_PIN_DIAGNOSTIC
constexpr const char *FirmwareVersion = "pin-diag-0.1.0";
#elif FIVESTAR_CONNECTED_DIAGNOSTIC
constexpr const char *FirmwareVersion = "connected-diag-0.1.0";
#elif FIVESTAR_FAST_POWER_MONITOR
constexpr const char *FirmwareVersion = "fast-power-monitor-0.1.0";
#else
#endif

#if !FIVESTAR_RECOVERY_ONLY && !FIVESTAR_PIN_DIAGNOSTIC && !FIVESTAR_CONNECTED_DIAGNOSTIC
constexpr uint32_t InverterBaud = 2400;
#if FIVESTAR_FAST_POWER_MONITOR
constexpr uint32_t FirstPollDelayMs = 0;
constexpr uint32_t PollIntervalMs = 5000;
constexpr uint32_t CommandTimeoutMs = 1200;
#else
constexpr uint32_t FirstPollDelayMs = 5000;
constexpr uint32_t PollIntervalMs = 30000;
constexpr uint32_t CommandTimeoutMs = 1500;
#endif
#endif

#if !FIVESTAR_RECOVERY_ONLY
#if FIVESTAR_PIN_DIAGNOSTIC || FIVESTAR_FAST_POWER_MONITOR
constexpr uint32_t MqttReconnectIntervalMs = 1000;
#else
constexpr uint32_t MqttReconnectIntervalMs = 10000;
#endif
#if !FIVESTAR_CONNECTED_DIAGNOSTIC && !FIVESTAR_PIN_DIAGNOSTIC && !FIVESTAR_FAST_POWER_MONITOR
constexpr const char *FirmwareVersion = "0.3.0";
#endif

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

#if !FIVESTAR_CONNECTED_DIAGNOSTIC && !FIVESTAR_PIN_DIAGNOSTIC
uint32_t nextPollMs = FirstPollDelayMs;
uint32_t inverterTimeoutCount = 0;
uint32_t successfulPollCount = 0;
#endif
uint32_t lastMqttConnectAttemptMs = 0;
#endif

uint32_t lastHeartbeatMs = 0;
#if !FIVESTAR_RECOVERY_ONLY
uint32_t lastStatusMs = 0;
#endif
#if FIVESTAR_PIN_DIAGNOSTIC
#if FIVESTAR_ULTRA_LOW_POWER
constexpr uint32_t PinSampleIntervalMs = 5000;
constexpr uint32_t PinBurstIntervalMs = 1000;
constexpr uint8_t PinBurstSamples = 1;
#elif FIVESTAR_LOW_POWER
constexpr uint32_t PinSampleIntervalMs = 2000;
constexpr uint32_t PinBurstIntervalMs = 500;
constexpr uint8_t PinBurstSamples = 6;
#else
constexpr uint32_t PinSampleIntervalMs = 500;
constexpr uint32_t PinBurstIntervalMs = 200;
constexpr uint8_t PinBurstSamples = 20;
#endif
uint32_t lastPinSampleMs = 0;
String bootPinSnapshot;
uint32_t pinSequence = 0;
#endif
bool wifiConnected = false;

void debugPrintln(const __FlashStringHelper *message) {
#if FIVESTAR_SERIAL_DEBUG
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(message);
#else
  Serial1.println(message);
#endif
#else
  (void)message;
#endif
}

void debugPrint(const __FlashStringHelper *message) {
#if FIVESTAR_SERIAL_DEBUG
#if FIVESTAR_RECOVERY_ONLY
  Serial.print(message);
#else
  Serial1.print(message);
#endif
#else
  (void)message;
#endif
}

void debugPrintlnString(const String &message) {
#if FIVESTAR_SERIAL_DEBUG
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(message);
#else
  Serial1.println(message);
#endif
#else
  (void)message;
#endif
}

void debugPrintlnNumber(int32_t value) {
#if FIVESTAR_SERIAL_DEBUG
#if FIVESTAR_RECOVERY_ONLY
  Serial.println(value);
#else
  Serial1.println(value);
#endif
#else
  (void)value;
#endif
}

void serviceBackground() {
#if !FIVESTAR_ULTRA_LOW_POWER
  ArduinoOTA.handle();
#endif
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
#if FIVESTAR_LOW_POWER || FIVESTAR_ULTRA_LOW_POWER
  WiFi.setSleepMode(WIFI_MODEM_SLEEP);
#else
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif
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

#if FIVESTAR_PIN_DIAGNOSTIC
constexpr uint8_t MonitoredPins[] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};

String readPinSnapshot() {
  String snapshot;
  snapshot.reserve(96);
  for (uint8_t i = 0; i < sizeof(MonitoredPins); i++) {
    uint8_t pin = MonitoredPins[i];
    if (i > 0) {
      snapshot += ' ';
    }
    snapshot += "GPIO";
    snapshot += String(pin);
    snapshot += '=';
    snapshot += String(digitalRead(pin));
  }
  return snapshot;
}

void configurePinDiagnostics() {
  for (uint8_t i = 0; i < sizeof(MonitoredPins); i++) {
    pinMode(MonitoredPins[i], INPUT);
  }
  bootPinSnapshot = readPinSnapshot();
}

void publishPinDiagnostics(bool retained) {
  String snapshot = readPinSnapshot();
  publishText("diag/pins", snapshot, retained);
  publishText("diag/boot_pins", bootPinSnapshot, true);
  publishText("diag/sequence", String(pinSequence++), false);
  publishText("diag/sample_ms", String(millis()), false);
  publishText("diag/gpio0", String(digitalRead(0)), retained);
  publishText("diag/gpio1_tx0", String(digitalRead(1)), retained);
  publishText("diag/gpio2_tx1_boot", String(digitalRead(2)), retained);
  publishText("diag/gpio3_rx0", String(digitalRead(3)), retained);
  publishText("diag/gpio15_boot", String(digitalRead(15)), retained);
}

void publishPinBurst() {
  for (uint8_t i = 0; i < PinBurstSamples; i++) {
    serviceBackground();
    publishPinDiagnostics(i == 0);
    delay(PinBurstIntervalMs);
  }
}
#endif

void publishStatus() {
  publishText("status/version", FirmwareVersion);
#if FIVESTAR_PIN_DIAGNOSTIC
  publishText("status/mode", "pin-diagnostic");
#elif FIVESTAR_CONNECTED_DIAGNOSTIC
  publishText("status/mode", "connected-diagnostic");
#else
  publishText("status/mode", "logger");
#endif
  publishText("status/reset_reason", ESP.getResetReason());
  publishText("status/ip", WiFi.localIP().toString());
  publishText("status/rssi", String(WiFi.RSSI()));
  publishText("status/uptime_ms", String(millis()), false);
  publishText("status/free_heap", String(ESP.getFreeHeap()), false);
#if !FIVESTAR_CONNECTED_DIAGNOSTIC && !FIVESTAR_PIN_DIAGNOSTIC
  publishText("status/inverter_timeouts", String(inverterTimeoutCount), false);
  publishText("status/successful_polls", String(successfulPollCount), false);
#else
  publishText("status/inverter_uart", "disabled", false);
#endif
#if FIVESTAR_PIN_DIAGNOSTIC
  publishPinDiagnostics(false);
#endif
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
  if (lastMqttConnectAttemptMs != 0 && now - lastMqttConnectAttemptMs < MqttReconnectIntervalMs) {
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
#if FIVESTAR_PIN_DIAGNOSTIC
    publishPinBurst();
#endif
  }
#endif
}

#if !FIVESTAR_CONNECTED_DIAGNOSTIC && !FIVESTAR_PIN_DIAGNOSTIC
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
  publishText("raw/q1", rawQ1.length() > 0 ? rawQ1 : String("<timeout>"), false);
  Q1Snapshot snapshot;
  if (parseQ1(rawQ1, snapshot)) {
    successfulPollCount++;
    publishSnapshot(snapshot);
    publishText("status/last_poll_ms", String(millis()), false);
    publishText("status/successful_polls", String(successfulPollCount), false);
  } else {
    publishText("raw/q1_parse_error", rawQ1.length() > 0 ? rawQ1 : String("<timeout>"), false);
  }
#if FIVESTAR_FAST_POWER_MONITOR
  String rawF = sendCommand("F");
  publishText("raw/f", rawF.length() > 0 ? rawF : String("<timeout>"), false);
  String rawI = sendCommand("I");
  publishText("raw/i", rawI.length() > 0 ? rawI : String("<timeout>"), false);
#endif
}
#endif
#endif
} // namespace

void setup() {
#if FIVESTAR_RECOVERY_ONLY
  Serial.begin(DebugBaud);
#else
#if FIVESTAR_SERIAL_DEBUG
  Serial1.begin(DebugBaud);
#endif
#if FIVESTAR_PIN_DIAGNOSTIC
  configurePinDiagnostics();
#elif !FIVESTAR_CONNECTED_DIAGNOSTIC
  Serial.begin(InverterBaud, SERIAL_8N1);
  Serial.setTimeout(CommandTimeoutMs);
#else
  pinMode(1, INPUT);
  pinMode(3, INPUT);
#endif
#endif

  debugPrint(F("FiveStar firmware "));
  debugPrintlnString(FirmwareVersion);
  debugPrint(F("Reset reason: "));
  debugPrintlnString(ESP.getResetReason());

  connectWifi();
#if !FIVESTAR_ULTRA_LOW_POWER
  setupOta();
#endif

#if !FIVESTAR_RECOVERY_ONLY
  connectMqtt();
  debugPrintln(F("OTA warmup before inverter polling"));
#if FIVESTAR_PIN_DIAGNOSTIC
  publishPinBurst();
#endif
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

#if FIVESTAR_PIN_DIAGNOSTIC
  if (now - lastPinSampleMs >= PinSampleIntervalMs) {
    lastPinSampleMs = now;
    publishPinDiagnostics(false);
  }
#elif !FIVESTAR_CONNECTED_DIAGNOSTIC
  if (static_cast<int32_t>(now - nextPollMs) >= 0) {
    nextPollMs = now + PollIntervalMs;
    pollInverter();
  }
#endif
#endif
}
