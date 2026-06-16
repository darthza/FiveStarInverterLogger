#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <config.h>

namespace {
constexpr uint32_t DebugBaud = 115200;
constexpr uint32_t InverterBaud = 2400;
constexpr uint32_t PollIntervalMs = 15000;
constexpr uint32_t StatusIntervalMs = 30000;
constexpr uint32_t CommandTimeoutMs = 1500;
constexpr uint32_t MqttReconnectIntervalMs = 10000;
constexpr const char *FirmwareVersion = "0.2.7";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

uint32_t lastPollMs = 0;
uint32_t pollCount = 0;
uint32_t lastStatusMs = 0;
uint32_t lastMqttConnectAttemptMs = 0;
uint32_t inverterTimeoutCount = 0;

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
  publishText("status/ip", WiFi.localIP().toString());
  publishText("status/rssi", String(WiFi.RSSI()));
  publishText("status/uptime_ms", String(millis()), false);
  publishText("status/free_heap", String(ESP.getFreeHeap()), false);
  publishText("status/inverter_timeouts", String(inverterTimeoutCount), false);
}

String readLineFromInverter(uint32_t timeoutMs) {
  String response;
  response.reserve(96);
  uint32_t started = millis();

  while (millis() - started < timeoutMs) {
    ArduinoOTA.handle();
#if MQTT_ENABLED
    mqtt.loop();
#endif

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

  Serial1.print(F("cmd "));
  Serial1.print(command);
  Serial1.print(F(" -> "));
  Serial1.println(response.length() == 0 ? F("<timeout>") : response);

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

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(OTA_HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(10.5f);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial1.print(F("Connecting WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial1.print('.');
  }

  Serial1.println();
  Serial1.print(F("WiFi connected: "));
  Serial1.println(WiFi.localIP());
}

void setupOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial1.println(F("OTA start"));
  });
  ArduinoOTA.onEnd([]() {
    Serial1.println(F("OTA end"));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial1.print(F("OTA error "));
    Serial1.println(static_cast<int>(error));
  });

  ArduinoOTA.begin();
  Serial1.println(F("OTA ready"));
}

void connectMqtt() {
#if MQTT_ENABLED
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
  bool connected = false;

  if (strlen(MQTT_USER) > 0) {
    connected = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  } else {
    connected = mqtt.connect(clientId.c_str());
  }

  Serial1.println(connected ? F("MQTT connected") : F("MQTT connect failed"));
  if (connected) {
    publishStatus();
  }
#endif
}

void pollInverter() {
  pollCount++;

  if (pollCount == 1) {
    String rawQ = sendCommand("Q");
    if (rawQ.length() > 0) {
      publishText("raw/q", rawQ);
    }

    String rawF = sendCommand("F");
    if (rawF.length() > 0) {
      publishText("raw/f", rawF);
    }

    String rawI = sendCommand("I");
    if (rawI.length() > 0) {
      publishText("raw/i", rawI);
    }
  }

  String rawQ1 = sendCommand("Q1");
  Q1Snapshot snapshot;
  if (parseQ1(rawQ1, snapshot)) {
    publishSnapshot(snapshot);
    publishText("status/last_poll", String(millis()), false);
    Serial1.print(F("battery="));
    Serial1.print(snapshot.batteryVoltage, 1);
    Serial1.print(F("V output="));
    Serial1.print(snapshot.outputVoltage, 1);
    Serial1.print(F("V load="));
    Serial1.print(snapshot.loadPercent);
    Serial1.println(F("%"));
  } else {
    publishText("raw/q1_parse_error", rawQ1.length() > 0 ? rawQ1 : String("<timeout>"), false);
    Serial1.println(F("Q1 parse failed"));
  }
}
} // namespace

void setup() {
  Serial1.begin(DebugBaud);
  Serial1.println();
  Serial1.print(F("FiveStar EB-WF03-01 logger booting v"));
  Serial1.println(FirmwareVersion);

  Serial.begin(InverterBaud, SERIAL_8N1);
  Serial.setTimeout(CommandTimeoutMs);

  connectWifi();
  setupOta();
  connectMqtt();
}

void loop() {
  ArduinoOTA.handle();

#if MQTT_ENABLED
  if (!mqtt.connected()) {
    connectMqtt();
  }
  mqtt.loop();
#endif

  uint32_t now = millis();
  if (now - lastStatusMs >= StatusIntervalMs) {
    lastStatusMs = now;
    publishStatus();
  }

  if (now - lastPollMs >= PollIntervalMs) {
    lastPollMs = now;
    pollInverter();
  }
}
