#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "webpages.h"
#include "wifi_config.h"

String currentTimestamp();
void connectWiFi();
void syncTime();
void handleRoot();
void handleActivate();
void handleGetTimestamps();
void handleClearTimestamps();
void handleNotFound();

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET -18000 // UTC-5 (Eastern Standard Time) in seconds
#define DST_OFFSET 3600   // 1 hour DST offset in seconds

#define RELAY_PIN D0       // Xiao ESP32-C6 pin D0 (GPIO 0)
#define RELAY_PULSE_MS 500 // How long the relay stays closed (ms)
#define ACTIVATE_DEBOUNCE_MS 500

#define WEB_PORT 80
#define MAX_TIMESTAMPS 500
#define TIMESTAMP_LENGTH 20

// ── Globals ─────────────────────────────────────────────────
WebServer server(WEB_PORT);
char timestampQueue[MAX_TIMESTAMPS][TIMESTAMP_LENGTH];
size_t timestampHead = 0;
size_t timestampCount = 0;
unsigned long lastActivationMs = 0;
bool hasActivated = false;

void enqueueTimestamp(const String &timestamp)
{
  size_t index = (timestampHead + timestampCount) % MAX_TIMESTAMPS;
  timestamp.toCharArray(timestampQueue[index], TIMESTAMP_LENGTH);

  if (timestampCount < MAX_TIMESTAMPS)
  {
    timestampCount++;
  }
  else
  {
    timestampHead = (timestampHead + 1) % MAX_TIMESTAMPS;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== Garage Opener =====");

  // Relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Network
  connectWiFi();
  syncTime();

  // Server
  server.on("/", handleRoot);
  server.on("/activate", handleActivate);
  server.on("/timestamps", HTTP_GET, handleGetTimestamps);
  server.on("/timestamps", HTTP_DELETE, handleClearTimestamps);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop()
{
  server.handleClient();
  // Reconnect WiFi if it drops
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi lost — reconnecting…");
    connectWiFi();
    syncTime();
  }
}

void connectWiFi()
{
  Serial.printf("Connecting to %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("\nConnected!  IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.println("\nFailed to connect — restarting in 5 s…");
    delay(5000);
    ESP.restart();
  }
}

void syncTime()
{
  configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER);
  Serial.print("Waiting for NTP sync");
  struct tm ti;
  int tries = 0;
  while (!getLocalTime(&ti) && tries < 20)
  {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (tries < 20)
  {
    Serial.printf("Time: %04d-%02d-%02dT%02d:%02d:%02d\n",
                  ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                  ti.tm_hour, ti.tm_min, ti.tm_sec);
  }
  else
  {
    Serial.println("NTP sync failed — timestamps may be wrong");
  }
}

String currentTimestamp()
{
  struct tm ti;
  if (!getLocalTime(&ti))
    return "1970-01-01T00:00:00";
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &ti);
  return String(buf);
}

void handleActivate()
{
  unsigned long now = millis();
  if (hasActivated && now - lastActivationMs < ACTIVATE_DEBOUNCE_MS)
  {
    server.send(429, "application/json", "{\"status\":\"error\",\"message\":\"Activation is cooling down\"}");
    return;
  }

  Serial.println(">> Garage door activated");
  String ts = currentTimestamp();
  enqueueTimestamp(ts);

  // Pulse the relay
  digitalWrite(RELAY_PIN, HIGH);
  delay(RELAY_PULSE_MS);
  digitalWrite(RELAY_PIN, LOW);
  lastActivationMs = millis();
  hasActivated = true;

  Serial.printf("Logged at %s\n", ts.c_str());

  // Respond with JSON
  String json = "{\"status\":\"ok\",\"message\":\"Activated at " + ts + "\"}";
  server.send(200, "application/json", json);
}

void handleGetTimestamps()
{
  String json = "[";
  for (size_t i = 0; i < timestampCount; i++)
  {
    if (i > 0)
      json += ",";
    size_t index = (timestampHead + i) % MAX_TIMESTAMPS;
    json += "\"";
    json += timestampQueue[index];
    json += "\"";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleClearTimestamps()
{
  size_t clearedCount = timestampCount;
  timestampHead = 0;
  timestampCount = 0;

  String json = "{\"status\":\"ok\",\"cleared\":";
  json += String(clearedCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot()
{
  server.send(200, "text/html", index_html);
}

void handleNotFound()
{
  server.send(404, "text/html", not_found_html);
}