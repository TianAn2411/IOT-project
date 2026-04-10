#include "task_coreiot.h"

#include "app_rtc.h"
#include "app_wifi.h"
#include "global.h"
#include "task_output.h"
#include "variable.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstring>

extern GlobalContext *global_ctx;

namespace {

WiFiClient g_mqttNetClient;
PubSubClient g_mqttClient(g_mqttNetClient);

uint32_t g_lastTelemetryMs = 0;
uint32_t g_lastReconnectTryMs = 0;

char g_lastMqttServer[65] = {0};
char g_lastMqttUser[65] = {0};
char g_lastMqttPassword[65] = {0};
uint16_t g_lastMqttPort = 0;

constexpr uint32_t kMqttReconnectRetryMs = 5000UL;
constexpr char kTelemetryTopic[] = "v1/devices/me/telemetry";
constexpr char kRpcRequestTopic[] = "v1/devices/me/rpc/request/+";
constexpr char kAttributesTopic[] = "v1/devices/me/attributes";

const char *mqtt_state_text(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT:
      return "CONNECTION_TIMEOUT";
    case MQTT_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case MQTT_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case MQTT_DISCONNECTED:
      return "DISCONNECTED";
    case MQTT_CONNECTED:
      return "CONNECTED";
    case MQTT_CONNECT_BAD_PROTOCOL:
      return "BAD_PROTOCOL";
    case MQTT_CONNECT_BAD_CLIENT_ID:
      return "BAD_CLIENT_ID";
    case MQTT_CONNECT_UNAVAILABLE:
      return "UNAVAILABLE";
    case MQTT_CONNECT_BAD_CREDENTIALS:
      return "BAD_CREDENTIALS";
    case MQTT_CONNECT_UNAUTHORIZED:
      return "UNAUTHORIZED";
    default:
      return "UNKNOWN";
  }
}

bool json_bool_fallback(const JsonVariantConst &variant, bool fallback) {
  if (variant.is<bool>()) {
    return variant.as<bool>();
  }
  if (variant.is<int>()) {
    return variant.as<int>() != 0;
  }
  if (variant.is<const char *>()) {
    const String text = String(variant.as<const char *>());
    return text == "1" || text == "true" || text == "on";
  }
  return fallback;
}

uint8_t json_u8_clamped(const JsonVariantConst &variant, uint8_t fallback) {
  if (!variant.is<int>()) {
    return fallback;
  }
  const int value = variant.as<int>();
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return static_cast<uint8_t>(value);
}

void copy_text(char *dest, size_t size, const char *src) {
  if (size == 0) {
    return;
  }

  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }

  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
}

void sync_mqtt_runtime_config() {
  copy_text(g_lastMqttServer, sizeof(g_lastMqttServer), mqtt_server);
  copy_text(g_lastMqttUser, sizeof(g_lastMqttUser), mqtt_user);
  copy_text(g_lastMqttPassword, sizeof(g_lastMqttPassword), mqtt_password);
  g_lastMqttPort = mqtt_port;
}

bool mqtt_runtime_config_changed() {
  return strcmp(g_lastMqttServer, mqtt_server) != 0 ||
         strcmp(g_lastMqttUser, mqtt_user) != 0 ||
         strcmp(g_lastMqttPassword, mqtt_password) != 0 ||
         g_lastMqttPort != mqtt_port;
}

void ensure_server_config() {
  g_mqttClient.setServer(mqtt_server, static_cast<uint16_t>(mqtt_port));
}

void apply_runtime_mqtt_config_if_needed() {
  if (!mqtt_runtime_config_changed()) {
    return;
  }

  ensure_server_config();
  if (g_mqttClient.connected()) {
    g_mqttClient.disconnect();
  }
  sync_mqtt_runtime_config();
}

void mqtt_callback(char *topic, uint8_t *payload, unsigned int length) {
  String topicStr(topic == nullptr ? "" : topic);
  String payloadStr;
  payloadStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    payloadStr += static_cast<char>(payload[i]);
  }

  const String rpcPrefix = "v1/devices/me/rpc/request/";
  if (!topicStr.startsWith(rpcPrefix)) {
    return;
  }

  const String requestId = topicStr.substring(rpcPrefix.length());
  if (requestId.length() == 0) {
    return;
  }

  String method;
  bool commandHandled = false;
  bool commandOk = false;
  bool responseAsBool = false;
  bool responseBoolValue = false;

  StaticJsonDocument<384> reqDoc;
  const DeserializationError err = deserializeJson(reqDoc, payloadStr);
  if (!err) {
    method = String(reqDoc["method"].as<const char *>());
    const JsonVariantConst params = reqDoc["params"];

    if (method == "setPwm2" || method == "setPWM2" || method == "set_pwm2" ||
        method == "setPWM2state" || method == "setPWMstate") {
      bool enabled = task_output_get_pwm2_enabled();
      uint8_t duty = task_output_get_pwm2_duty_percent();

      if (params.is<JsonObjectConst>()) {
        enabled = json_bool_fallback(params["enabled"], enabled);
        duty = json_u8_clamped(params["duty"], duty);
        duty = json_u8_clamped(params["duty_percent"], duty);
      } else {
        enabled = json_bool_fallback(params, enabled);
      }

      task_output_set_pwm2_config(enabled, duty);
      commandHandled = true;
      commandOk = true;
      responseAsBool = true;
      responseBoolValue = task_output_get_pwm2_enabled();
    } else if (method == "getPwm2" || method == "getPWM2" || method == "get_pwm2" ||
               method == "getPWM2state" || method == "getPWMstate") {
      commandHandled = true;
      commandOk = true;
      responseAsBool = true;
      responseBoolValue = task_output_get_pwm2_enabled();
    } else if (method == "setPWM2dutycycle" || method == "setPWMdutycycle" ||
               method == "setPwm2DutyCycle") {
      bool enabled = task_output_get_pwm2_enabled();
      uint8_t duty = task_output_get_pwm2_duty_percent();

      if (params.is<JsonObjectConst>()) {
        duty = json_u8_clamped(params["value"], duty);
        duty = json_u8_clamped(params["duty"], duty);
        duty = json_u8_clamped(params["duty_percent"], duty);
      } else {
        duty = json_u8_clamped(params, duty);
      }

      task_output_set_pwm2_config(enabled, duty);
      commandHandled = true;
      commandOk = true;
    } else if (method == "getPWM2dutycycle" || method == "getPWMdutycycle" ||
               method == "getPwm2DutyCycle") {
      commandHandled = true;
      commandOk = true;
    }
  }

  String respTopic = "v1/devices/me/rpc/response/" + requestId;
  if (responseAsBool) {
    g_mqttClient.publish(respTopic.c_str(), responseBoolValue ? "true" : "false");
    return;
  }

  if (method == "getPWM2dutycycle" || method == "getPWMdutycycle" ||
      method == "getPwm2DutyCycle" || method == "setPWM2dutycycle" ||
      method == "setPWMdutycycle" || method == "setPwm2DutyCycle") {
    const String dutyText = String(task_output_get_pwm2_duty_percent());
    g_mqttClient.publish(respTopic.c_str(), dutyText.c_str());
    return;
  }

  StaticJsonDocument<384> respDoc;
  respDoc["ok"] = commandOk;
  respDoc["handled"] = commandHandled;
  respDoc["method"] = method;
  respDoc["message"] = commandHandled ? "RPC applied" : "Unknown or invalid RPC";
  respDoc["request"] = payloadStr;
  respDoc["pwm2_enabled"] = task_output_get_pwm2_enabled();
  respDoc["pwm2_duty_percent"] = task_output_get_pwm2_duty_percent();

  String respPayload;
  serializeJson(respDoc, respPayload);
  g_mqttClient.publish(respTopic.c_str(), respPayload.c_str());
}

String build_client_id() {
  if (strlen(mqtt_user) > 0) {
    return String(mqtt_user);
  }

  String id = "esp32s3-";
  id += WiFi.macAddress();
  id.replace(":", "");
  return id;
}

bool connect_mqtt() {
  if (!app_wifi_is_sta_connected() || !app_wifi_is_internet_connected()) {
    return false;
  }

  if (strlen(mqtt_server) == 0 || mqtt_port == 0) {
    return false;
  }

  ensure_server_config();

  const String clientId = build_client_id();
  bool ok = false;

  if (strlen(mqtt_user) > 0) {
    // Primary mode: explicit username/password from web config.
    ok = g_mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password);
    if (!ok) {
      // Fallback used by some brokers/platforms: token as username without password.
      ok = g_mqttClient.connect(clientId.c_str(), mqtt_user, nullptr);
    }
  } else {
    ok = g_mqttClient.connect(clientId.c_str());
  }

  if (!ok && strlen(mqtt_password) > 0) {
    // Last fallback for platforms where the entered password field contains the device token.
    ok = g_mqttClient.connect(clientId.c_str(), mqtt_password, nullptr);
  }

  if (ok) {
    const bool subOk = g_mqttClient.subscribe(kRpcRequestTopic);
    Serial.printf("[MQTT] Connected broker=%s:%u clientId=%s subscribeRpc=%s\n",
                  mqtt_server,
                  static_cast<unsigned>(mqtt_port),
                  clientId.c_str(),
                  subOk ? "ok" : "fail");
    sync_mqtt_runtime_config();
  } else {
    const int st = g_mqttClient.state();
    Serial.printf("[MQTT] Connect failed state=%d(%s) broker=%s:%u user=%s\n",
            st,
            mqtt_state_text(st),
                  mqtt_server,
                  static_cast<unsigned>(mqtt_port),
                  strlen(mqtt_user) > 0 ? mqtt_user : "(empty)");
  }

  return ok;
}

void send_attributes() {
  StaticJsonDocument<256> doc;
  doc["device"] = "esp32-s3";
  doc["mqtt_interval_sec"] = mqtt_publish_interval_ms / 1000UL;
  doc["wifi_ssid"] = app_wifi_get_connected_ssid();

  String payload;
  serializeJson(doc, payload);
  const bool ok = g_mqttClient.publish(kAttributesTopic, payload.c_str());
  if (!ok) {
    const int st = g_mqttClient.state();
    Serial.printf("[MQTT] Publish attributes failed state=%d(%s)\n",
            st,
            mqtt_state_text(st));
  }
}

void send_telemetry() {
  float t = 0.0f;
  float h = 0.0f;
  String prediction = "Unknown";

  if (global_ctx && xSemaphoreTake(global_ctx->dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    t = global_ctx->temperature;
    h = global_ctx->humidity;
    prediction = global_ctx->predictedWeather;
    xSemaphoreGive(global_ctx->dataMutex);
  }

  const bool ntpSynced = app_rtc_is_ntp_synced();
  const uint32_t rtcTimestampSec = app_rtc_now_timestamp();
  const uint64_t tsMs = static_cast<uint64_t>(rtcTimestampSec) * 1000ULL;

  StaticJsonDocument<384> doc;
  // Only attach explicit timestamp when RTC is NTP-synced.
  // Otherwise let the platform assign server-side current time.
  if (ntpSynced && rtcTimestampSec > 0) {
    doc["ts"] = tsMs;
  }
  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["prediction"] = prediction;

  String payload;
  serializeJson(doc, payload);

  const bool ok = g_mqttClient.publish(kTelemetryTopic, payload.c_str());
  if (!ok) {
    const int st = g_mqttClient.state();
    Serial.printf("[MQTT] Publish telemetry failed state=%d(%s) payload=%s\n",
                  st,
                  mqtt_state_text(st),
                  payload.c_str());
  }
}

}  // namespace

void task_coreiot(void *pvParameters) {
  (void)pvParameters;

  g_mqttClient.setBufferSize(512);
  g_mqttClient.setCallback(mqtt_callback);
  ensure_server_config();
  sync_mqtt_runtime_config();

  for (;;) {
    const uint32_t nowMs = millis();

    apply_runtime_mqtt_config_if_needed();

    if (!app_wifi_is_sta_connected() || !app_wifi_is_internet_connected()) {
      if (g_mqttClient.connected()) {
        g_mqttClient.disconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (!g_mqttClient.connected()) {
      if ((nowMs - g_lastReconnectTryMs) >= kMqttReconnectRetryMs) {
        g_lastReconnectTryMs = nowMs;
        if (connect_mqtt()) {
          send_attributes();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    g_mqttClient.loop();

    if ((nowMs - g_lastTelemetryMs) >= mqtt_publish_interval_ms) {
      g_lastTelemetryMs = nowMs;
      send_telemetry();
      send_attributes();
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
