#include "messageTCP.h"


messageTCP::messageTCP()
{
  _client = WiFiClient();
}

messageTCP::~messageTCP()
{
  _client.stop();
}

void messageTCP::send(IPAddress ip, char* data)
{
  _client.connect(ip, 7084);
  _client.print(data);
  _client.stop();
}

void messageTCP::send(IPAddress ip, const char* data)
{
  _client.connect(ip, 7084);
  _client.println(data);
  _client.stop();
}

char* messageTCP::receive(WiFiServer* server, IPAddress* sender, int size)
{
  char* data = new char[size];
  WiFiClient client = server->available(); // Get the client
  if (!client) return data; // Return if no client is connected
  while (client.connected()) // While the client is connected
  {
    if (client.available()) // If the client has data
    {
      int bytes = client.readBytesUntil('\n', data, size); // Read the data
      data[bytes] = '\0'; // Add a null terminator
      break; // Break the loop
    }
  }
  *sender = client.remoteIP(); // Get the IP address of the client
  client.stop(); // Stop the client
  return data; // Return if client is connected
}

