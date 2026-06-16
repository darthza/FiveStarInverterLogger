#pragma once

// Copy this file to include/config.h and edit the values before building.

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

#define OTA_HOSTNAME "fivestar-inverter"
#define OTA_PASSWORD "change-me"

// Leave MQTT disabled until the serial logger is proven on the inverter.
#define MQTT_ENABLED 0
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_TOPIC_PREFIX "fivestar/inverter"

