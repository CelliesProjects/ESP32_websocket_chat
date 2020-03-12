// https://techtutorialsx.com/2018/08/14/esp32-async-http-web-server-websockets-introduction/
// https://javascript.info/websocket

#include <AsyncTCP.h>              /* https://github.com/me-no-dev/AsyncTCP */
#include <ESPAsyncWebServer.h>
#include "index_htm.h"

const char* ssid = "huiskamer";
const char* password = "0987654321";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

std::vector<uint32_t>  clientId;
std::vector<IPAddress> clientIp;
std::vector<String>    clientName;

void setup()
{
  WiFi.begin(ssid, password);
  ESP_LOGI(TAG, "Connecting to %s\n", ssid);
  while (!WiFi.isConnected()) delay(10);

  server.on("/", HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_htm, index_htm_len);
    request->send(response);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.onNotFound([](AsyncWebServerRequest * request)
  {
    ESP_LOGI(TAG, "Not found http://%s%s\n", request->host().c_str(), request->url().c_str());
    request->send(404);
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.begin();
  ESP_LOGI(TAG, "Setup Done. Connected to %s - %s\n", ssid, WiFi.localIP().toString().c_str());
}

void loop()
{
  ws.cleanupClients();
  delay(100);
}

void sendUserlistToAll() {
  char userlist[500];
  uint16_t curr = snprintf(userlist, sizeof(userlist), "USERLIST\n");
  for (uint8_t i = 0; i < ws.count(); i++) {
    if (!clientName[i].equals(""))
      curr += snprintf(userlist + curr, sizeof(userlist) - curr, "%s\n", clientName[i].c_str());
    else
      curr += snprintf(userlist + curr, sizeof(userlist) - curr, "%i\n", clientId[i]);
  }
  ESP_LOGI(TAG, "Sending userlist with size %i bytes to all clients:\n%s ",curr , userlist);
   
  ws.binaryAll(userlist);
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    clientId.push_back(client->id());
    clientIp.push_back(client->remoteIP());
    clientName.push_back("");    // TODO: auto fill in name
    ESP_LOGI(TAG, "ws[%s][%u] connect\n", server->url(), client->id());
    //client->ping();
    client->printf("<green>Hello %i@%s - Welcome @ ESP32 chat :)</green>", client->id(), client->remoteIP().toString().c_str());
    ws.printfAll("<red>%i@%s entered the room. There are %lu users online.</red>", client->id(), client->remoteIP().toString().c_str(), ws.count());
    sendUserlistToAll();
  }


  else if (type == WS_EVT_DISCONNECT) {
    uint8_t i{0};
    while (clientId[i] != client->id()) i++;
    ESP_LOGI(TAG, "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    if (clientName[i].equals("")) 
      ws.printfAll("<red>Client %u left the room. There are %lu users online.</red>.", client->id(), ws.count());
    else
      ws.printfAll("<red>%s left the room. There are %lu users online.</red>.", clientName[i], ws.count());
    clientId.erase(clientId.begin() + i);
    clientIp.erase(clientIp.begin() + i);
    clientName.erase(clientName.begin() + i);
    sendUserlistToAll();
  }


  else if (type == WS_EVT_ERROR) {
    //error was received from the other end
    ESP_LOGE(TAG, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  }


  else if (type == WS_EVT_PONG) {
    //pong message was received (in response to a ping request maybe)
    ESP_LOGI(TAG, "ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
  }


  else if (type == WS_EVT_DATA) {
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      ESP_LOGI(TAG, "ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT) {
        data[len] = 0;
        ESP_LOGI(TAG, "%s\n", (char*)data);
      } else {
        //client->text((char*)data);
        /*
          for (size_t i = 0; i < info->len; i++) {
          ESP_LOGI(TAG, "%02x ", data[i]);
          }
          ESP_LOGI(TAG, "\n");
        */
      }
      if (info->opcode == WS_TEXT) {
        //client->text((char*)data);

    
        
        uint8_t i{0};
        while (clientId[i] != client->id()) i++;
        if (!clientName[i].equals(""))            
          ws.printfAll("<red>%s:</red> %s", clientName[i], (char*)data);
        else        
          ws.printfAll("<red>%u@%s:</red> %s", client->id(), client->remoteIP().toString().c_str(), (char*)data);
      } else {
        //get the command from the binary data
        char command[10];
        uint8_t i{0};
        while (data[i] != '\n' && i < sizeof(command) - 1) {
          command[i] = data[i];
          i++;
        }
        command[i] = 0;
        ESP_LOGI(TAG, "command: %s", command);

        //we have decoded the command

        if (0 == strcmp(command, "NAME")) {
          ESP_LOGI(TAG, "Request for name change");
          i++;
          char username[15];
          while (data[i] != '\n' && (i-5) < (sizeof(username) - 1)) {
            username[i-5] = data[i];
            i++;
          }
          username[i-5]=0;

          //find the client id in 'clientId' and change the corresponding name
          uint8_t i{0};
          while (clientId[i] != client->id()) i++;
          clientName[i] = username;
          ESP_LOGI(TAG, "new name: %s", clientName[i].c_str());

          sendUserlistToAll();
        }
        //client->binary("I got your binary message");

      }
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        if (info->num == 0)
          ESP_LOGI(TAG, "ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        ESP_LOGI(TAG, "ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      ESP_LOGI(TAG, "ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT) {
        data[len] = 0;
        ESP_LOGI(TAG, "%s\n", (char*)data);
      } else {
        for (size_t i = 0; i < len; i++) {
          ESP_LOGI(TAG, "%02x ", data[i]);
        }
        ESP_LOGI(TAG, "\n");
      }

      if ((info->index + len) == info->len) {
        ESP_LOGI(TAG, "ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          ESP_LOGI(TAG, "ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}
