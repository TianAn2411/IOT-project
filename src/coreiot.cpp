#include "coreiot.h"

WiFiClient espClient;
PubSubClient client(espClient);


void reconnect(GlobalContext *ctx) {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
        
      Serial.println("connected to CoreIOT Server!");
      client.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("Subscribed to v1/devices/me/rpc/request/+");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  // Allocate a temporary buffer for the message
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Payload: ");
  Serial.println(message);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* method = doc["method"];
  if (strcmp(method, "setStateLED") == 0) {
    const char* params = doc["params"];

    if (strcmp(params, "ON") == 0) {
      Serial.println("Device turned ON.");
    } else {   
      Serial.println("Device turned OFF.");
    }
  } else {
    Serial.print("Unknown method: ");
    Serial.println(method);
  }
}


void setup_coreiot(GlobalContext *ctx){

  while(1){
    if (xSemaphoreTake(ctx->xBinarySemaphoreInternet, portMAX_DELAY)) {
      break;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println(" Connected!");

  client.setServer(ctx->CORE_IOT_SERVER.c_str(), ctx->CORE_IOT_PORT.toInt());
  client.setCallback(callback);
}

void coreiot_task(void *pvParameters){
    GlobalContext *ctx = (GlobalContext *)pvParameters;
    setup_coreiot(ctx);

    while(1){

        if (!client.connected()) {
            reconnect(ctx);
        }
        client.loop();

        float t = -1;
        float h = -1;
        if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
            t = ctx->temperature;
            h = ctx->humidity;
            xSemaphoreGive(ctx->dataMutex);
        }

        // Sample payload, publish to 'v1/devices/me/telemetry'
        String payload = "{\"temperature\":" + String(t) +  ",\"humidity\":" + String(h) + "}";
        
        client.publish("v1/devices/me/telemetry", payload.c_str());
        
        Serial.println("Published payload: " + payload);
        vTaskDelay(10000 / portTICK_PERIOD_MS);  // Publish every 10 seconds
    }
}