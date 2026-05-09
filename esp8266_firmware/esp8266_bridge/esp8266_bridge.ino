/*
  PicoKey ESP8266 Bridge
  Board: NodeMCU / Wemos D1 Mini

  WIRING:
    ESP8266 D6 (GPIO12) RX  ←  Pico GP0 TX
    ESP8266 D5 (GPIO14) TX  →  Pico GP1 RX
    GND ←→ GND

  Libraries required: ESP8266WiFi, ESP8266HTTPClient, SoftwareSerial, ArduinoJson 6.x
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// ── CONFIG — edit these ────────────────────────────────────────────────────
const char* WIFI_SSID    = "YOUR_WIFI_SSID";
const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL   = "https://whitebot.skillsupriselab.com/picokey/api.php";
const char* DEVICE_TOKEN = "CHANGE_THIS_SECRET_TOKEN_32CHARS";
// ──────────────────────────────────────────────────────────────────────────

#define UART_BAUD    9600       // 9600 = reliable on SoftwareSerial; match pico_main.py
#define POLL_MS      600UL
#define HB_MS        8000UL
#define HTTP_TIMEOUT 4000
#define WIFI_RETRY   40         // 40 × 500ms = 20s before restart

SoftwareSerial picoSerial(D6, D5); // RX=D6(GPIO12), TX=D5(GPIO14)

static unsigned long lastPoll = 0;
static unsigned long lastHB   = 0;
static char rxBuf[256];
static uint8_t rxIdx = 0;

// ─────────────────────────────────────────────────────────────────────────────
void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
    if (++n > WIFI_RETRY) {
      Serial.println("\n[WiFi] Timeout — restarting");
      ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] OK  IP=%s  RSSI=%d dBm\n",
    WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== PicoKey Bridge v2 ===");

  picoSerial.begin(UART_BAUD);
  Serial.printf("[UART] %d baud  RX=D6(GPIO12) TX=D5(GPIO14)\n", UART_BAUD);

  wifiConnect();
  sendHeartbeat("online");
}

void loop() {
  yield();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost — reconnecting");
    WiFi.disconnect();
    delay(500);
    wifiConnect();
    return;
  }

  unsigned long now = millis();
  if (now - lastHB   >= HB_MS)   { lastHB   = now; sendHeartbeat("alive"); }
  if (now - lastPoll >= POLL_MS)  { lastPoll = now; fetchAndForward(); }

  // ── Read lines from Pico ─────────────────────────────────────────────────
  while (picoSerial.available()) {
    char c = (char)picoSerial.read();
    if (c == '\n' || c == '\r') {
      if (rxIdx > 0) {
        rxBuf[rxIdx] = '\0';
        Serial.printf("[PICO←] %s\n", rxBuf);
        rxIdx = 0;
      }
    } else if (rxIdx < (uint8_t)(sizeof(rxBuf) - 1)) {
      rxBuf[rxIdx++] = c;
    }
    yield();
  }
}

// ── POST — returns HTTP status code ────────────────────────────────────────
int httpPost(const char* url, const String& body, String& respOut) {
  WiFiClientSecure client;
  client.setInsecure(); // skip cert verification — fine for device→own server
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(HTTP_TIMEOUT);
  int code = http.POST(body);
  respOut = (code > 0) ? http.getString() : "";
  http.end();
  client.stop();
  yield();
  return code;
}

// ── GET — returns HTTP status code ─────────────────────────────────────────
int httpGet(const String& url, String& respOut) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Connection", "close");
  http.setTimeout(HTTP_TIMEOUT);
  int code = http.GET();
  respOut = (code > 0) ? http.getString() : "";
  http.end();
  client.stop();
  yield();
  return code;
}

// ── Poll server for next command ───────────────────────────────────────────
void fetchAndForward() {
  String url = String(SERVER_URL) + "?action=poll&token=" + DEVICE_TOKEN;
  String resp;
  int code = httpGet(url, resp);

  if (code != 200) {
    Serial.printf("[POLL] HTTP %d  url=%s\n", code, url.c_str());
    return;
  }

  resp.trim();
  // Strip UTF-8 BOM that some PHP hosts prepend
  if (resp.length() >= 3
      && (uint8_t)resp[0] == 0xEF
      && (uint8_t)resp[1] == 0xBB
      && (uint8_t)resp[2] == 0xBF) {
    resp = resp.substring(3);
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) {
    Serial.printf("[POLL] JSON err  raw=%s\n", resp.c_str());
    return;
  }

  const char* status = doc["status"] | "";
  if (strcmp(status, "ok") == 0 && doc.containsKey("cmd")) {
    const char* cmdJson = doc["cmd"] | "";
    if (cmdJson[0]) {
      picoSerial.println(cmdJson);
      Serial.printf("[→PICO] %s\n", cmdJson);
    }
  }
  // status=="empty" is normal, no log
}

// ── Heartbeat ──────────────────────────────────────────────────────────────
void sendHeartbeat(const char* state) {
  StaticJsonDocument<200> doc;
  doc["action"] = "heartbeat";
  doc["token"]  = DEVICE_TOKEN;
  doc["state"]  = state;
  doc["ip"]     = WiFi.localIP().toString();
  doc["rssi"]   = WiFi.RSSI();
  String body;
  serializeJson(doc, body);

  String resp;
  int code = httpPost(SERVER_URL, body, resp);
  if (code == 200) {
    Serial.printf("[HB] OK (%s)\n", state);
  } else {
    Serial.printf("[HB] FAIL HTTP %d\n  url=%s\n  token prefix=%s\n",
      code, SERVER_URL, String(DEVICE_TOKEN).substring(0, 6).c_str());
  }
}
