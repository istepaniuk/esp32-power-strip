#include <Arduino.h>

#include <WiFi.h>
#include <WiFiSTA.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#define LED_BLUE_PIN 2
#define LED_RED_PIN 32
#define LED_GREEN_PIN 33
#define RELAY_1_PIN 13
#define RELAY_2_PIN 4
#define RELAY_3_PIN 16
#define RELAY_4_PIN 17
#define RELAY_5_PIN 19
#define RELAY_6_PIN 22
#define RELAY_7_PIN 23
#define RELAY_8_PIN 27

#define LEDS_FAST_BLINK_INTERVAL_MS 200

#define WIFI_SSID "network_ssid"

#define WIFI_PASSWORD "the password"
#define MQTT_SERVER "192.168.81.10"
#define MQTT_USER "guest"
#define MQTT_PASSWORD "guest"
#define MQTT_TOPIC_EVENT "powerstrip.event"
#define MQTT_TOPIC_COMMAND "powerstrip.command"

#define MQTT_MAX_CONNECTION_RETRIES 20

void wifi_setup();
void ota_setup_handler();
void mqtt_reconnect();
void on_mqtt_message_received(char *topic, byte *payload, unsigned int length);
void set_relay_on(int relay_number);
void set_relay_off(int relay_number);
void relay_board_set_port(char value);

WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);

char message_buff[100];
bool debug = true;
char relay_state = 0x00;

void loop() {
    if (!mqtt_client.connected()) {
        mqtt_reconnect();
    }
    mqtt_client.loop();
    ArduinoOTA.handle();
}

void setup() {
    pinMode(RELAY_1_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_2_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_3_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_4_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_5_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_6_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_7_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(RELAY_8_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(LED_BLUE_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);

    digitalWrite(RELAY_1_PIN, HIGH);
    digitalWrite(RELAY_2_PIN, HIGH);
    digitalWrite(RELAY_3_PIN, HIGH);
    digitalWrite(RELAY_4_PIN, HIGH);
    digitalWrite(RELAY_5_PIN, HIGH);
    digitalWrite(RELAY_6_PIN, HIGH);
    digitalWrite(RELAY_7_PIN, HIGH);
    digitalWrite(RELAY_8_PIN, HIGH);
    digitalWrite(LED_BLUE_PIN, HIGH);
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_GREEN_PIN, HIGH);

    relay_state = 0x00;

    Serial.begin(115200);
    wifi_setup();
    mqtt_client.setServer(MQTT_SERVER, 1883);
    mqtt_client.setCallback(on_mqtt_message_received);
    mqtt_reconnect();
    digitalWrite(LED_BLUE_PIN, LOW);
}

void wifi_setup() {
    digitalWrite(LED_BLUE_PIN, LOW);
    delay(10);
    Serial.println();
    Serial.print("Connecting to '");
    Serial.print(WIFI_SSID);
    Serial.print("' .");

    WiFi.setHostname("garden");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setHostname("gardeny");

    while (WiFi.status() != WL_CONNECTED) {
        delay(LEDS_FAST_BLINK_INTERVAL_MS);
        digitalWrite(LED_BLUE_PIN, !digitalRead(LED_BLUE_PIN));
        Serial.print(".");
    }
    digitalWrite(LED_BLUE_PIN, HIGH);

    Serial.println("OK");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    ota_setup_handler();
}

void ota_setup_handler() {
    Serial.println("Starting OTA handler...");

    ArduinoOTA.onStart([]() {
      digitalWrite(LED_BLUE_PIN, LOW);

      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
      } else { // U_FS
          type = "filesystem";
      }

      Serial.println("OTA Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("\nOTA End");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      digitalWrite(LED_BLUE_PIN, !digitalRead(LED_BLUE_PIN));
    });

    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
          Serial.println("OTA Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
          Serial.println("OTA Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
          Serial.println("OTA Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
          Serial.println("OTA Receive Failed");
      } else if (error == OTA_END_ERROR) {
          Serial.println("OTA End Failed");
      }
    });

    ArduinoOTA.begin();
    ArduinoOTA.setRebootOnSuccess(true);
    Serial.println("OTA Initialized");
}

void mqtt_reconnect() {
    int retries = 0;

    digitalWrite(LED_BLUE_PIN, HIGH);
    while (!mqtt_client.connected()) {
        //? wifi_setup();
        Serial.print("Connecting to MQTT broker...");
        if (mqtt_client.connect("lock", MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("Connected, subscribing...");
            mqtt_client.subscribe(MQTT_TOPIC_COMMAND);
            digitalWrite(LED_BLUE_PIN, LOW);
        } else {
            retries++;
            Serial.print("FAILED: ");
            Serial.print(mqtt_client.state());
            if (retries > MQTT_MAX_CONNECTION_RETRIES) {
                Serial.println("Max retries reached. Reset!");
                ESP.restart();
            }
            Serial.println(" Retrying...");
            digitalWrite(LED_BLUE_PIN, LOW);
            delay(100);
            digitalWrite(LED_BLUE_PIN, HIGH);
            delay(100);
            digitalWrite(LED_BLUE_PIN, LOW);
            delay(100);
            digitalWrite(LED_BLUE_PIN, HIGH);
            delay(700);
        }
    }
    Serial.println("OK: MQTT connected & subscribed");
    mqtt_client.publish(MQTT_TOPIC_EVENT, "connected", true);
}

void on_mqtt_message_received(char *topic, byte *payload, unsigned int length) {
    const char *cmd_ping = "ping";
    const char *cmd_set = "set";
    const char *cmd_reset = "res";

    int i;
    for (i = 0; i < length; i++) {
        message_buff[i] = payload[i];
    }
    message_buff[i] = '\0';
    String received_message = String(message_buff);

    if (debug) {
        Serial.println("cmd> " + received_message);
    }

    if (received_message == cmd_ping) {
        Serial.println("ping!");
    } else if (strncmp(message_buff, cmd_set, strlen(cmd_set)) == 0) {
        char relay_number = message_buff[strlen(cmd_set)] - '0';
        Serial.printf("SET: %i\n", relay_number);
        set_relay_on(relay_number);
    } else if (strncmp(message_buff, cmd_reset, strlen(cmd_reset)) == 0) {
        char relay_number = message_buff[strlen(cmd_reset)] - '0';
        Serial.printf("RESET: %i\n", relay_number);
        set_relay_off(relay_number);
    }
}

void set_relay_on(int relay_number) {
    relay_state |= (0x01 << (relay_number - 1));
    relay_board_set_port(relay_state);
}

void set_relay_off(int relay_number) {
    relay_state &= (0xFE << (relay_number - 1));
    relay_board_set_port(relay_state);
}

void relay_board_set_port(char value) {
    Serial.printf(">> RELAY %d\n", value);
    digitalWrite(RELAY_1_PIN, !(value & 0b00000001));
    digitalWrite(RELAY_2_PIN, !(value & 0b00000010));
    digitalWrite(RELAY_3_PIN, !(value & 0b00000100));
    digitalWrite(RELAY_4_PIN, !(value & 0b00001000));
    digitalWrite(RELAY_5_PIN, !(value & 0b00010000));
    digitalWrite(RELAY_6_PIN, !(value & 0b00100000));
    digitalWrite(RELAY_7_PIN, !(value & 0b01000000));
    digitalWrite(RELAY_8_PIN, !(value & 0b10000000));

    // turn on the red LED if all outputs are off
    digitalWrite(LED_RED_PIN, !value);
}
