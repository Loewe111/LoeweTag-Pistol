#include <Arduino.h>
#include <ESP8266WiFi.h>
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

messageTCP message = messageTCP();
WiFiServer server(7084);

DynamicJsonDocument response(JSON_BUFFER_SIZE);
DynamicJsonDocument request(JSON_BUFFER_SIZE);

unsigned long lastMillis = 0;
unsigned int frame = 0;
unsigned int cooldown = 0;

unsigned int deviceID;

uint8_t player_color[] = {255, 127, 0};

uint16_t player_vars[] = {/*HP:*/ 0, /*MHP:*/ 100, /*SP:*/ 0, /*MSP:*/ 100, /*ATK:*/ 0, /*RT:*/ 20, /*PTS:*/ 0, /*KILL:*/ 0};
#define VAR_HP player_vars[0]
#define VAR_MHP player_vars[1]
#define VAR_SP player_vars[2]
#define VAR_MSP player_vars[3]
#define VAR_ATK player_vars[4]
#define VAR_RT player_vars[5]
#define VAR_PTS player_vars[6]
#define VAR_KILL player_vars[7]

bool informationSent = false;

#define STATE_OFFLINE -1
#define STATE_IDLE 0
#define STATE_WAITING 1
#define STATE_RUNNING 2
int state = STATE_OFFLINE;

void shoot() {
  // Send IR Onkyo command, data: IP
  digitalWrite(MOTOR_PIN, HIGH);
  IPAddress ip = WiFi.localIP();
  IrSender.sendOnkyo(ip[2], ip[3], 0);
  digitalWrite(MOTOR_PIN, LOW);
}

void sendInfo() {
  response.clear();
  response["type"] = "device_information";
  response["ip"] = WiFi.localIP().toString();
  response["device_id"] = deviceID;
  response["device_type"] = "pistol";
  response["firmware"] = FIRMWARE;

  message.send(IPAddress(192, 168, 1, 1), response.as<String>().c_str());
}

void swipeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t delayTime) {
  for(int i = 0; i < 6; i++) {
    leds_gun.setPixelColor(i, leds_gun.Color(r, g, b));
    leds_gun.show();
    delay(delayTime);
  }
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
  unsigned int randomID = random(0, 0xFFFF);
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
  WiFi.begin(AP_SSID, AP_PASS);
  server.begin();
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

  // Frame update every 100ms
  if(millis() - lastMillis > 100) {
    lastMillis = millis();
    frame++;
    if(cooldown > 0) {
      cooldown -= 1;
    }
  }

  // delete pointers
  delete sender;
  delete[] content;
}