// https://techtutorialsx.com/2018/08/14/esp32-async-http-web-server-websockets-introduction/
// https://javascript.info/websocket

#include <AsyncTCP.h>              /* Reports as 1.0.3 https://github.com/me-no-dev/AsyncTCP */
#include <ESPAsyncWebServer.h>
#include "index_htm.h"

const char* ssid = "SSID";
const char* password = "PSK";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
  ESP_LOGI(TAG, "Setup Done. Connected to %s\n", ssid);
}

void loop()
{
  ws.cleanupClients();
  delay(1000);
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    //client connected
    ESP_LOGI(TAG, "ws[%s][%u] connect\n", server->url(), client->id());
    //client->ping();
    ws.printfAll("<red>%i@%s entered the room</red>", client->id(), client->remoteIP().toString().c_str());
    client->printf("<green>Hello %i@%s - Welcome @ ESP32 chat :)</green>", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    //client disconnected
    ESP_LOGI(TAG, "ws[%s][%u] disconnect: %u\n", server->url(), client->id());

    ws.printfAll("<red>Client %u left the room</red>.", client->id());
  } else if (type == WS_EVT_ERROR) {
    //error was received from the other end
    ESP_LOGI(TAG, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if (type == WS_EVT_PONG) {
    //pong message was received (in response to a ping request maybe)
    ESP_LOGI(TAG, "ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
  } else if (type == WS_EVT_DATA) {
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      ESP_LOGI(TAG, "ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT) {
        data[len] = 0;
        ESP_LOGI(TAG, "%s\n", (char*)data);
      } else {
        client->text((char*)data);
        for (size_t i = 0; i < info->len; i++) {
          ESP_LOGI(TAG, "%02x ", data[i]);
        }
        ESP_LOGI(TAG, "\n");
      }
      if (info->opcode == WS_TEXT) {
        //client->text((char*)data);
        ws.printfAll("<red>%u@%s:</red> %s", client->id(), client->remoteIP().toString().c_str(), (char*)data);
      }
      else
        client->binary("I got your binary message");
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
