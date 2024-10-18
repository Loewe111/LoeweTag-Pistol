#define NO_LED_FEEDBACK_CODE

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Adafruit_NeoPixel.h>
#include <IRremote.hpp>
#include "animations/animations.h"
#include "pistol.h"
#include "build_info.h"

// User settings
#define LED_BRIGHTNESS 64 // 0-255

// Global constants 
#define FIRMWARE "dev 0.10.2"

// Pin definitions
#define PIN_GUN_TRIGGER 15 // Connector X7
#define PIN_GUN_IR_BEAM_NARROW 0 // Connector X1
#define PIN_GUN_IR_BEAM_WIDE 12 // Connector X2
#define PIN_GUN_MOTOR 13 // Connector X4

#define PIN_GUN_LEDS 14

#define PIN_SENSORS_IR_RECEIVE 5 
#define PIN_SENSORS_LEDS 4

Adafruit_NeoPixel leds_gun(6, PIN_GUN_LEDS, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel leds_sensors(4, PIN_SENSORS_LEDS, NEO_GRB + NEO_KHZ800);

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
    .active = false,
    .beam_type = pistol_weapon_t::narrow
  },
  .health = 8,
  .max_health = 12
};

animations anim(&leds_gun, &leds_sensors, &state);

void sendInfo();

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
      message_set_health_t set_health;
      memcpy(&set_health, message->data, sizeof(message_set_health_t));
      state.health = set_health.health;
      state.max_health = set_health.max_health;
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

uint8_t getSendPin() {
  if (state.weapon.beam_type == pistol_weapon_t::narrow) {
    return PIN_GUN_IR_BEAM_NARROW;
  } else if (state.weapon.beam_type == pistol_weapon_t::wide) {
    return PIN_GUN_IR_BEAM_WIDE;
  }
  return -1;
}

void shoot() {
  uint8_t sendPin = getSendPin();
  IrSender.setSendPin(sendPin);

  // Activate the narrow beam if the weapon is not set to narrow
  if (state.weapon.beam_type != pistol_weapon_t::narrow) {
    digitalWrite(PIN_GUN_IR_BEAM_NARROW, HIGH);
  }

  // Activate the motor
  digitalWrite(PIN_GUN_MOTOR, HIGH);

  // Send the IR signal
  IrSender.sendOnkyo(deviceID, 0, 0);

  // Deactivate the motor and narrow beam
  digitalWrite(PIN_GUN_MOTOR, LOW);
  digitalWrite(PIN_GUN_IR_BEAM_NARROW, LOW);
}

void flashMotor() {
  digitalWrite(PIN_GUN_MOTOR, HIGH);
  delay(30);
  digitalWrite(PIN_GUN_MOTOR, LOW);
}

uint16_t generateDeviceID() { // Generate a unique Device ID based on the MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  uint64_t macID = ((uint64_t)mac[0] << 40) | ((uint64_t)mac[1] << 32) | ((uint64_t)mac[2] << 24) |
                   ((uint64_t)mac[3] << 16) | ((uint64_t)mac[4] << 8) | ((uint64_t)mac[5]);
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

  pinMode(PIN_GUN_MOTOR, OUTPUT);

  leds_sensors.begin();
  leds_sensors.show();
  leds_sensors.setBrightness(LED_BRIGHTNESS);

  leds_gun.begin();
  leds_gun.show();
  leds_gun.setBrightness(LED_BRIGHTNESS);

  IrSender.begin(PIN_GUN_IR_BEAM_NARROW);
  IrSender.enableIROut(38);
  IrReceiver.begin(PIN_SENSORS_IR_RECEIVE);
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
  if(digitalRead(PIN_GUN_TRIGGER) && millis() - lastShoot > state.weapon.reload_time && state.weapon.active) {
    anim.setAnimation(ANIM_SHOOTING);
    shoot();
    lastShoot = millis();
  }

  if(IrReceiver.decode()) {
    if(IrReceiver.decodedIRData.protocol != UNKNOWN) {
      anim.setAnimation(ANIM_HIT);
      message_hit_t hit = {(uint16_t)IrReceiver.decodedIRData.decodedRawData};
      message_base_t* message = create_message(deviceID, masterID, MESSAGE_HIT, &hit);
      sendMessage(message);
    }
    IrReceiver.resume();
  }

  if (millis() - lastPing > pingInterval + 1000 && state.gamestate != GAMESTATE_OFFLINE) {
    updateState(GAMESTATE_OFFLINE);
    Serial.println("Timeout");
  }

  anim.draw();
}