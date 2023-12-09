#ifndef MESSAGE_TCP_H
#define MESSAGE_TCP_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

class messageTCP
{
  private:
    WiFiClient _client;
  public:
    messageTCP();
    ~messageTCP();
    void send(IPAddress ip, char* data);
    void send(IPAddress ip, const char* data);
    char* receive(WiFiServer* server, IPAddress* sender, int size);
};

#endif