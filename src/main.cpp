#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Adafruit_NeoPixel.h>
#include <IRremote.hpp>
#include <ArduinoJson.h>
#include "messageTCP.h"

// User settings
#define AP_SSID "LoeweTag-Link"
#define AP_PASS "12345678"
#define LED_BRIGHTNESS 64 // 0-255

// Global constants 
#define CHAR_BUFFER_SIZE 200
#define JSON_BUFFER_SIZE 200
#define FIRMWARE "dev 0.10.2"

// Pin definitions
#define BUTTON_PIN 15
#define IR_RECEIVE_PIN 5
#define IR_SEND_PIN 13
#define LEDS_GUN_PIN 14
#define LEDS_SENSORS_PIN 4
#define MOTOR_PIN 12

Adafruit_NeoPixel leds_gun(6, LEDS_GUN_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel leds_sensors(4, LEDS_SENSORS_PIN, NEO_GRB + NEO_KHZ800);

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t masterID = 0;

unsigned long lastShoot = 0;

unsigned long lastPing = 0;
unsigned int pingInterval = 2000;

uint16_t deviceID;

pistol_state_t state = {
  .gamestate = GAMESTATE_OFFLINE,
  .color = {255, 0, 255},
  .weapon = {
    .reload_type = pistol_weapon_t::automatic,
    .reload_time = 500,
    .power = 0,
    .active = false
  },
  .health = 8,
  .max_health = 12
};

animations anim(&leds_gun, &leds_sensors, &state);

template <typename T>
message_base_t* create_message(uint16_t sender, uint16_t target, message_type_t type, T* data) {
  message_base_t* message = (message_base_t*)malloc(sizeof(message_base_t) + sizeof(T));
  message->sender = sender;
  message->target = target;
  message->type = type;
  message->length = sizeof(T);
  memcpy(message->data, data, sizeof(T));
  return message;
}

void sendMessage(message_base_t* message, bool keepMessage = false) {
  esp_now_send(broadcastAddress, (uint8_t*)message, sizeof(message_base_t) + message->length);
  if (!keepMessage) free(message);
}

void pingMaster() {
  message_ping_t ping;
  message_base_t* message = create_message(deviceID, masterID, MESSAGE_PING, &ping);
  sendMessage(message);
}

void updateState(gamestate_t newState) {
  state.gamestate = newState;

  if (state.gamestate == GAMESTATE_OFFLINE) {
    anim.setAnimation(ANIM_CONNECTING);
  } else if (state.gamestate == GAMESTATE_IDLE) {
    anim.setAnimation(ANIM_IDLE);
  } else if (state.gamestate == GAMESTATE_PLAYING) {
    anim.setAnimation(ANIM_PLAYING);
  }
}

void handleMessages(uint8_t *mac, uint8_t *data, uint8_t len) {
  message_base_t* message = (message_base_t*)malloc(len);
  memcpy(message, data, len);
  if (message->target != deviceID && message->target != 0xFFFF) {
    free(message);
    return; // Message is not for this device
  }

  switch(message->type) {
    case MESSAGE_PING: {
      message_ping_t ping;
      memcpy(&ping, message->data, sizeof(message_ping_t));
      if (state.gamestate == GAMESTATE_OFFLINE) {
        masterID = message->sender;
        updateState(GAMESTATE_IDLE);
        Serial.println("Found Master: " + String(masterID));
        sendInfo();
      }
      lastPing = millis();
      pingMaster();
    } break;
    case MESSAGE_DEVICE_INFORMATION: {
      break; // Don't care, this device should not receive this message
    }
    case MESSAGE_SET_GAMESTATE: {
      message_set_gamestate_t set_gamestate;
      memcpy(&set_gamestate, message->data, sizeof(message_set_gamestate_t));
      updateState(set_gamestate.gamestate);
    } break;
    case MESSAGE_SET_COLOR: {
      message_set_color_t set_color;
      memcpy(&set_color, message->data, sizeof(message_set_color_t));
      state.color = (pistol_color_t){set_color.r, set_color.g, set_color.b};
      Serial.println("Received color: " + String(state.color.r) + " " + String(state.color.g) + " " + String(state.color.b));
    } break;
    case MESSAGE_SET_WEAPON: {
      message_set_weapon_t set_weapon;
      memcpy(&set_weapon, message->data, sizeof(message_set_weapon_t));
      state.weapon = set_weapon.weapon;
    } break;
    case MESSAGE_SET_HEALTH: {
      uint16_t health;
      memcpy(&health, message->data, sizeof(uint16_t));
      state.health = health;
    } break;
    case MESSAGE_HIT: {
      break; // Don't care, this device should not receive this message
    }
    default: {
      Serial.println("Received unknown message");
    } break;
  }
  free(message);
}

void sendInfo() {
  message_device_information_t response = {deviceID, DEVICE_TYPE_PISTOL};
  message_base_t* message = create_message(deviceID, masterID, MESSAGE_DEVICE_INFORMATION, &response);
  sendMessage(message);
}

void shoot() {
  digitalWrite(MOTOR_PIN, HIGH);
  uint8_t byte0 = deviceID >> 8;
  uint8_t byte1 = deviceID & 0xFF;
  IrSender.sendOnkyo(byte0, byte1, 0);
  digitalWrite(MOTOR_PIN, LOW);
}

void flashMotor() {
  digitalWrite(MOTOR_PIN, HIGH);
  delay(30);
  digitalWrite(MOTOR_PIN, LOW);
}

unsigned int generateDeviceID() { // Generate a unique Device ID based on the MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  unsigned long macID = ((unsigned long)mac[0] << 40) | ((unsigned long)mac[1] << 32) | ((unsigned long)mac[2] << 24) |
                        ((unsigned long)mac[3] << 16) | ((unsigned long)mac[4] << 8) | ((unsigned long)mac[5]);
  randomSeed(macID); // Seed the random number generator with the mac address
  uint16_t randomID = random(0, 0xFFFF);
  return randomID;
}

void setup() {
  deviceID = generateDeviceID();
  Serial.begin(115200);
  Serial.println("LoeweTag Pistol | (c) 2024 Loewe111 | loewe111.github.io/LoeweTag");
  Serial.println("Build: " + String(BUILD_INFO));
  Serial.println("Firmware: " + String(FIRMWARE));
  Serial.println("MAC: " + WiFi.macAddress());
  Serial.println("Device ID: " + String(deviceID));

  pinMode(MOTOR_PIN, OUTPUT);

  leds_sensors.begin();
  leds_sensors.show();
  leds_sensors.setBrightness(LED_BRIGHTNESS);

  leds_gun.begin();
  leds_gun.show();
  leds_gun.setBrightness(LED_BRIGHTNESS);

  IrSender.begin(IR_SEND_PIN);
  IrSender.enableIROut(38);
  IrReceiver.begin(IR_RECEIVE_PIN);
  IrReceiver.start();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(handleMessages);

}

void loop() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if(!connected) { // If not connected to WiFi
    leds_sensors.fill(leds_sensors.Color(0, 0, 0));
    leds_gun.fill(leds_gun.Color(0, 0, 0));
    if(frame % 10 == 0) {
      leds_gun.setPixelColor(5, 0, 0, 255);
    } else {
      leds_gun.setPixelColor(5, 255, 0, 0);
    }
    leds_sensors.show();
    leds_gun.show();
    state = STATE_OFFLINE;
    informationSent = false;
  } else { // If connected to WiFi, do game logic
    if(state == STATE_OFFLINE) state = STATE_IDLE; // If just connected, set state to idle
    if(state == STATE_IDLE) {
      leds_sensors.fill(leds_gun.Color(0, 0, 0));
      leds_gun.fill(leds_gun.Color(0, 0, 0));
      leds_gun.setPixelColor(5, 0, 255, 0);
      leds_sensors.show();
      leds_gun.show();
    } else if(state == STATE_WAITING) {
      int index = map(sin(frame) * 10, -10, 10, 0, 5);
      leds_gun.fill(leds_gun.Color(0, 0, 0));
      leds_gun.setPixelColor(index, leds_gun.Color(player_color[0], player_color[1], player_color[2]));
      leds_gun.show();
      leds_sensors.fill(leds_sensors.Color(0, 0, 0));
      leds_sensors.show();
    } else if(state == STATE_RUNNING) {
      double multiplier = sin(frame / 10.0) / 2.0 + 0.5;
      int leds = map(VAR_HP, 0, VAR_MHP, 0, 6);
      int r = player_color[0] * multiplier;
      int g = player_color[1] * multiplier;
      int b = player_color[2] * multiplier;

      leds_gun.fill(leds_gun.Color(0, 0, 0));
      for(int i = 0; i < leds; i++) {
        leds_gun.setPixelColor(i, leds_gun.Color(r, g, b));
      }
      leds_gun.show();
      if(VAR_HP > 0){
        leds_sensors.fill(leds_sensors.Color(r, g, b));
      } else {
        leds_sensors.fill(leds_sensors.Color(0, 0, 0));
        leds_sensors.setPixelColor(frame%4, leds_sensors.Color(r, g, b));
      }
      leds_sensors.show();
    }
    if(!informationSent) {
      Serial.println("Connected as " + WiFi.localIP().toString());
      sendInfo();
      informationSent = true;
    }
  }

  if(!connected && frame % 100 == 0) {
    WiFi.begin(AP_SSID, AP_PASS);
  }

  if(digitalRead(BUTTON_PIN) && cooldown == 0 && VAR_ATK > 0) {
    shoot();
    cooldown = VAR_RT;
  }

  IPAddress* sender = new IPAddress(); // Create a new IP address
  char* content = message.receive(&server, sender, CHAR_BUFFER_SIZE); // Get the message and the sender

  if(*sender){
    Serial.println("Received message from " + sender->toString() + ": " + content);
    request.clear();
    deserializeJson(request, content);
    if(request["type"] == "color") {
      player_color[0] = request["r"];
      player_color[1] = request["g"];
      player_color[2] = request["b"];
    } else if(request["type"] == "vars") {
      VAR_HP = request["HP"];
      VAR_MHP = request["MHP"];
      VAR_SP = request["SP"];
      VAR_MSP = request["MSP"];
      VAR_ATK = request["ATK"];
      VAR_RT = request["RT"];
      VAR_PTS = request["PTS"];
      VAR_KILL = request["KILL"];
    } else if(request["type"] == "gamestate") {
      state = request["state"];
    } else if(request["type"] == "locate") {
      leds_gun.fill(leds_gun.Color(0, 0, 0));
      swipeColor(255, 0, 0, 50);
      flashMotor();
      swipeColor(0, 255, 0, 50);
      flashMotor();
      swipeColor(0, 0, 255, 50);
      flashMotor();
      swipeColor(0, 0, 0, 50);
    } else if(request["type"] == "information") {
      sendInfo();
    }
  }

  if(IrReceiver.decode()) {
    if(IrReceiver.decodedIRData.protocol != UNKNOWN) {
      uint8_t address = IrReceiver.decodedIRData.decodedRawData;
      uint8_t command = IrReceiver.decodedIRData.decodedRawData >> 16;
      IPAddress ip = IPAddress(192, 168, address, command);
      response.clear();
      response["type"] = "hit";
      response["ip"] = WiFi.localIP().toString();
      response["sender"] = ip.toString();
      message.send(IPAddress(192, 168, 1, 1), response.as<String>().c_str());
    }
    IrReceiver.resume();
  }

  if (millis() - lastPing > pingInterval + 1000 && state.gamestate != GAMESTATE_OFFLINE) {
    updateState(GAMESTATE_OFFLINE);
    Serial.println("Timeout");
  }

  anim.draw();
}