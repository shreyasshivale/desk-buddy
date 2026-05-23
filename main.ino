// ============================================================
//  DESK BUDDY v2.0  –  XIAO ESP32S3 + ST7789 240×240
//  Pinout (your confirmed wiring):
//    SCK  → GPIO7  (D8)
//    MOSI → GPIO9  (D10)
//    DC   → GPIO1  (D0)
//    RST  → GPIO2  (D1)
//    CS   → -1     (tied to GND / no CS)
//    BLK  → 3.3V   (always on)
//    Touch OUT → GPIO3 (D2)
// ============================================================

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>
#include "time.h"

// ============================================================
//  DISPLAY DRIVER
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
public:
  LGFX() {
    auto b = _bus.config();
    b.spi_host    = SPI2_HOST;
    b.spi_mode    = 3;
    b.freq_write  = 40000000;
    b.spi_3wire   = false;
    b.use_lock    = true;
    b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk    = 7;   // D8
    b.pin_mosi    = 9;   // D10
    b.pin_miso    = -1;
    b.pin_dc      = 1;   // D0
    _bus.config(b);
    _panel.setBus(&_bus);

    auto p = _panel.config();
    p.pin_cs          = -1;  // CS tied to GND
    p.pin_rst         = 2;   // D1
    p.pin_busy        = -1;
    p.memory_width    = 240;
    p.memory_height   = 240;
    p.panel_width     = 240;
    p.panel_height    = 240;
    p.offset_x        = 0;
    p.offset_y        = 0;
    p.offset_rotation = 0;
    p.readable        = false;
    p.invert          = true;
    p.rgb_order       = false;
    _panel.config(p);
    setPanel(&_panel);
  }
};

LGFX lcd;

// ============================================================
//  FULL-SCREEN SPRITE  (double-buffer → zero flicker)
// ============================================================
LGFX_Sprite canvas(&lcd);

// ============================================================
//  COLOUR PALETTE
// ============================================================
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_DGRAY   0x2104  // dark background shade
#define C_MGRAY   0x4208  // mid-gray for UI chrome

// Eye / accent colours  (RGB565)
#define NUM_COLOURS 9
const uint16_t PALETTE[NUM_COLOURS] = {
  0x07FF,  // cyan
  0xF81F,  // magenta
  0xFFE0,  // yellow
  0x7E0F,  // lime
  0xFD20,  // orange
  0xFE19,  // hot pink
  0x001F,  // blue
  0x07E0,  // green
  0xFFFF   // white
};

// ============================================================
//  TOUCH
// ============================================================
#define TOUCH_PIN 3   // GPIO3 (D2)

unsigned long touchDownAt    = 0;
unsigned long lastReleaseAt  = 0;
bool          touchHeld      = false;
bool          lastTouchState = false;
unsigned long lastDebounce   = 0;
#define DEBOUNCE_MS   220
#define LONG_PRESS_MS 850
#define DTAP_MS       700

// ============================================================
//  WIFI / CONFIG PORTAL
// ============================================================
#define PREF_NS       "db"
#define AP_SSID       "DeskBuddy-Setup"
#define AP_PASS       "12345678"

String  wifi_ssid, wifi_pass;
String  ow_city    = "Pune";
String  ow_country = "IN";
String  ow_apikey;
long    gmtOffset  = 19800;   // IST default

Preferences prefs;
WebServer   server(80);
DNSServer   dns;
bool        cfgMode = false;

// ============================================================
//  WEATHER DATA
// ============================================================
float  curTemp   = 0;
int    curHum    = 0;
String wMain     = "Loading";
String wDesc     = "please wait";

struct Forecast { String label; int temp; };
Forecast fc[3];

unsigned long lastWeatherMs = 0;
#define WEATHER_MS (10UL * 60UL * 1000UL)

// ============================================================
//  PAGE / STATE
// ============================================================
// 0 = eyes   1 = clock   2 = weather   3 = forecast
int  page       = 0;
int  lastPage   = -1;
int  colIdx     = 0;
bool roundEyes  = false;

unsigned long lastInfoDraw = 0;

// ============================================================
//  EYE ANIMATION STATE
// ============================================================
float  eyeAngle  = 0.0f;

// Blink system
float         blinkScale    = 1.0f;   // 1.0 = open, 0.0 = closed
bool          blinking      = false;
bool          blinkClosing  = true;
float         blinkSpeed    = 0.18f;
unsigned long nextBlink     = 0;

// Sleep dimming (no touch for 60s)
unsigned long lastTouch = 0;
bool          sleeping  = false;

// Eye geometry  (80×80 sprite per eye)
#define EW       80
#define EH       80
#define PUPIL_R  17
#define PAD       6
#define EL_CX    70   // left eye centre x on 240px canvas
#define ER_CX   170   // right eye centre x
#define E_CY    120   // eye centre y

LGFX_Sprite eyeSpr(&lcd);

// ============================================================
//  PREFERENCES
// ============================================================
void loadPrefs() {
  prefs.begin(PREF_NS, true);
  wifi_ssid  = prefs.getString("ssid", "wifi_name");
  wifi_pass  = prefs.getString("pass", "wifi_password");
  ow_city    = prefs.getString("city", "Pune");
  ow_country = prefs.getString("country", "IN");
  ow_apikey  = prefs.getString("apikey", "9d2c17d9d48847b35a43b65008439a62");
  gmtOffset  = prefs.getLong("gmt", 19800);
  colIdx     = prefs.getUChar("col", 0);
  if (colIdx >= NUM_COLOURS) colIdx = 0;
  prefs.end();
}

void savePrefs() {
  prefs.begin(PREF_NS, false);
  prefs.putString("ssid",    wifi_ssid);
  prefs.putString("pass",    wifi_pass);
  prefs.putString("city",    ow_city);
  prefs.putString("country", ow_country);
  prefs.putString("apikey",  ow_apikey);
  prefs.putLong("gmt",       gmtOffset);
  prefs.putUChar("col",      (uint8_t)colIdx);
  prefs.end();
}

void saveColour() {
  prefs.begin(PREF_NS, false);
  prefs.putUChar("col", (uint8_t)colIdx);
  prefs.end();
}

// ============================================================
//  WEB CONFIG PORTAL
// ============================================================
static const char HTML[] PROGMEM = R"(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Desk Buddy</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#0a0a0a;color:#0ff;min-height:100vh;display:flex;
align-items:center;justify-content:center;padding:20px}
.card{background:#111;border:1px solid #0ff4;border-radius:14px;padding:28px;
max-width:380px;width:100%}
h1{text-align:center;font-size:1.4em;letter-spacing:.1em;margin-bottom:22px;
color:#0ff;text-transform:uppercase}
.sec{font-size:.75em;letter-spacing:.12em;color:#0ff8;margin:18px 0 8px;
text-transform:uppercase}
label{display:block;font-size:.85em;color:#aaa;margin-bottom:4px}
input{width:100%;padding:10px 12px;background:#1a1a1a;border:1px solid #0ff3;
border-radius:8px;color:#0ff;font-size:.95em;margin-bottom:12px;outline:none}
input:focus{border-color:#0ff}
button{width:100%;padding:13px;background:#0ff;color:#000;border:none;
border-radius:8px;font-size:1em;font-weight:bold;cursor:pointer;margin-top:8px;
letter-spacing:.05em}
button:hover{background:#0dd}
.msg{text-align:center;font-size:.85em;color:#0ff;margin-top:14px}
</style></head><body><div class="card">
<h1>&#x1F916; Desk Buddy</h1>
<form method="POST" action="/save">
<div class="sec">WiFi</div>
<label>SSID</label><input name="ssid" value="%s" maxlength="32" placeholder="Network name">
<label>Password</label><input type="password" name="pass" maxlength="64" placeholder="Leave blank to keep current">
<div class="sec">Weather</div>
<label>OpenWeather API Key</label><input name="apikey" value="%s" maxlength="64" placeholder="Get free key at openweathermap.org">
<label>City</label><input name="city" value="%s" maxlength="64" placeholder="e.g. Pune">
<label>Country code</label><input name="country" value="%s" maxlength="4" placeholder="e.g. IN">
<div class="sec">Timezone</div>
<label>Offset from UTC (hours) &mdash; IST = 5.5</label>
<input name="tz" value="%s" maxlength="6" placeholder="5.5">
<button type="submit">Save &amp; Reboot</button>
</form>
<p class="msg">%s</p>
</div></body></html>)";

void serveForm(bool saved) {
  char buf[2400];
  String tz  = String(gmtOffset / 3600.0f, 1);
  String msg = saved ? "Saved! Rebooting..." : "";
  snprintf(buf, sizeof(buf), HTML,
    wifi_ssid.c_str(), ow_apikey.c_str(),
    ow_city.c_str(), ow_country.c_str(),
    tz.c_str(), msg.c_str());
  server.send(200, "text/html", buf);
}

void handleSave() {
  if (server.method() != HTTP_POST) { server.send(405); return; }
  wifi_ssid  = server.arg("ssid").substring(0, 32);
  String p   = server.arg("pass");
  if (p.length()) wifi_pass = p.substring(0, 64);
  ow_apikey  = server.arg("apikey").substring(0, 64);
  ow_city    = server.arg("city").substring(0, 64);
  ow_country = server.arg("country").substring(0, 4);
  float tz   = server.arg("tz").toFloat();
  gmtOffset  = (long)(tz * 3600.0f);
  if (!ow_city.length())    ow_city    = "Pune";
  if (!ow_country.length()) ow_country = "IN";
  savePrefs();
  serveForm(true);
  delay(600);
  ESP.restart();
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/",      HTTP_GET,  []{ serveForm(false); });
  server.on("/save",  HTTP_POST, handleSave);
  server.onNotFound([]{ server.sendHeader("Location","http://192.168.4.1",true); server.send(302); });
  server.begin();
  cfgMode = true;
}

// ============================================================
//  WEATHER FETCH
// ============================================================
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED || !ow_apikey.length()) return;

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q="
    + ow_city + "," + ow_country
    + "&appid=" + ow_apikey + "&units=metric";
  http.begin(url);
  if (http.GET() == 200) {
    JSONVar o = JSON.parse(http.getString());
    if (JSON.typeof(o) != "undefined") {
      curTemp = double(o["main"]["temp"]);
      curHum  = int(o["main"]["humidity"]);
      wMain   = (const char*)o["weather"][0]["main"];
      wDesc   = (const char*)o["weather"][0]["description"];
      if (wDesc.length()) wDesc[0] = toupper(wDesc[0]);
    }
  }
  http.end();

  url = "http://api.openweathermap.org/data/2.5/forecast?q="
    + ow_city + "," + ow_country
    + "&appid=" + ow_apikey + "&units=metric";
  http.begin(url);
  if (http.GET() == 200) {
    JSONVar fo = JSON.parse(http.getString());
    if (JSON.typeof(fo) != "undefined") {
      int idx[3]          = { 1, 3, 7 };
      const char* lbl[3]  = { "Soon", "Later", "Tomorrow" };
      for (int i = 0; i < 3; i++) {
        fc[i].temp  = (int)double(fo["list"][idx[i]]["main"]["temp"]);
        fc[i].label = lbl[i];
      }
    }
  }
  http.end();
}

// ============================================================
//  DRAW HELPERS
// ============================================================

// ---- Draw a single eye into eyeSpr at given pupil offset ----
void drawEye(int16_t offX, int16_t offY, float blinkSc, bool round) {
  eyeSpr.fillScreen(C_BLACK);

  uint16_t eyeCol = PALETTE[colIdx];
  int16_t hW = EW / 2;
  int16_t hH = EH / 2;

  // Blink: compress eye height vertically around centre
  int16_t eyeH2 = (int16_t)((EH / 2) * blinkSc);   // half-height after blink
  if (eyeH2 < 2) eyeH2 = 2;

  int16_t top  = hH - eyeH2;
  int16_t bot  = hH + eyeH2;
  int16_t eyeH = bot - top;

  if (round) {
    // Circular eye: use ellipse approach via fillRoundRect with max radius
    int16_t r = min(hW, eyeH2);
    eyeSpr.fillRoundRect(hW - hW, top, EW, eyeH, r, eyeCol);
  } else {
    // Rounded rectangle eye with fixed corner radius
    eyeSpr.fillRoundRect(0, top, EW, eyeH, 14, eyeCol);
  }

  // Pupil — clamped inside sclera
  int16_t px = hW + offX;
  int16_t py = hH + offY;

  // Clamp
  int16_t pMin = PAD + PUPIL_R;
  int16_t pMaxX = EW - PAD - PUPIL_R;
  int16_t pMaxY = hH + eyeH2 - PAD - PUPIL_R;
  int16_t pMinY = hH - eyeH2 + PAD + PUPIL_R;
  if (px < pMin)  px = pMin;
  if (px > pMaxX) px = pMaxX;
  if (py < pMinY) py = pMinY;
  if (py > pMaxY) py = pMaxY;

  // Only draw pupil if eye is open enough
  if (eyeH2 > PUPIL_R + PAD) {
    eyeSpr.fillCircle(px, py, PUPIL_R, C_BLACK);
    // Small white highlight dot
    eyeSpr.fillCircle(px - 5, py - 5, 4, C_WHITE);
  }
}

// ---- Eyes page ----
void drawEyesPage() {
  canvas.fillScreen(C_BLACK);

  // Pupil orbit
  float maxX = (EW / 2.0f) - PAD - PUPIL_R;
  float maxY = (EH / 2.0f) - PAD - PUPIL_R;
  int16_t offX = (int16_t)(cosf(eyeAngle)          * maxX * 0.6f);
  int16_t offY = (int16_t)(sinf(eyeAngle * 0.61f)  * maxY * 0.5f);

  drawEye(offX, offY, blinkScale, roundEyes);
  eyeSpr.pushSprite(&canvas, EL_CX - EW/2, E_CY - EH/2);

  // Mirror pupil X for natural look
  drawEye(-offX, offY, blinkScale, roundEyes);
  eyeSpr.pushSprite(&canvas, ER_CX - EW/2, E_CY - EH/2);

  // Tiny page indicator dots at bottom
  uint16_t ac = PALETTE[colIdx];
  for (int i = 0; i < 4; i++) {
    int16_t dx = 100 + i * 14;
    canvas.fillCircle(dx, 222, 3, (i == page) ? ac : C_MGRAY);
  }

  // Sleep indicator: dim border
  if (sleeping) {
    canvas.drawRoundRect(2, 2, 236, 236, 8, C_MGRAY);
  }

  canvas.pushSprite(0, 0);
}

// ---- Clock page ----
void drawClockPage() {
  canvas.fillScreen(C_BLACK);
  uint16_t ac = PALETTE[colIdx];

  struct tm ti;
  if (!getLocalTime(&ti)) {
    canvas.setTextColor(C_MGRAY, C_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);
    canvas.drawString("No time yet", 120, 120);
    canvas.pushSprite(0, 0);
    return;
  }

  // Hour : Minute  (large, Font7 = 7-segment style)
  int h12 = ti.tm_hour % 12;
  if (!h12) h12 = 12;
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", h12, ti.tm_min);

  canvas.setTextColor(ac, C_BLACK);
  canvas.setFont(&fonts::Font7);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1.4f);
  canvas.drawString(timeBuf, 120, 100);

  // AM/PM badge
  canvas.setFont(nullptr);
  canvas.setTextSize(2);
  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.drawString((ti.tm_hour < 12) ? "AM" : "PM", 120, 135);

  // Date line
  char dateBuf[20];
  strftime(dateBuf, sizeof(dateBuf), "%a  %d %b", &ti);
  canvas.setTextSize(2);
  canvas.setTextColor(ac, C_BLACK);
  canvas.drawString(dateBuf, 120, 162);

  // Thin accent line under time
  canvas.drawFastHLine(40, 112, 160, ac);

  // Page dots
  for (int i = 0; i < 4; i++)
    canvas.fillCircle(100 + i * 14, 222, 3, (i == page) ? ac : C_MGRAY);

  canvas.pushSprite(0, 0);
}

// ---- Weather icon helper (simple geometry) ----
void drawWeatherIcon(int cx, int cy, const String& main, uint16_t col) {
  if (main == "Clear") {
    // Sun
    canvas.fillCircle(cx, cy, 14, 0xFFE0);
    for (int a = 0; a < 360; a += 45)
      canvas.drawLine(cx, cy,
        cx + (int)(cos(a * DEG_TO_RAD) * 22),
        cy + (int)(sin(a * DEG_TO_RAD) * 22), 0xFFE0);
  } else if (main == "Clouds" || main == "Mist" || main == "Fog") {
    canvas.fillCircle(cx - 8, cy,     12, C_MGRAY);
    canvas.fillCircle(cx + 5, cy - 6, 10, C_MGRAY);
    canvas.fillRoundRect(cx - 18, cy, 32, 10, 5, C_MGRAY);
  } else if (main == "Rain" || main == "Drizzle") {
    canvas.fillCircle(cx, cy - 4, 12, 0x435F);
    canvas.fillRoundRect(cx - 14, cy - 4, 28, 10, 5, 0x435F);
    for (int d = -1; d <= 1; d++)
      canvas.drawLine(cx + d*9, cy + 10, cx + d*9 - 4, cy + 22, 0x07FF);
  } else if (main == "Thunderstorm") {
    canvas.fillCircle(cx, cy - 4, 12, C_MGRAY);
    // Bolt
    canvas.fillTriangle(cx + 4, cy + 4, cx - 4, cy + 16, cx, cy + 16, 0xFFE0);
    canvas.fillTriangle(cx, cy + 10, cx + 8, cy + 22, cx - 2, cy + 22, 0xFFE0);
  } else if (main == "Snow") {
    for (int a = 0; a < 360; a += 60) {
      canvas.drawLine(cx, cy,
        cx + (int)(cos(a * DEG_TO_RAD) * 14),
        cy + (int)(sin(a * DEG_TO_RAD) * 14), C_WHITE);
    }
    canvas.fillCircle(cx, cy, 4, C_WHITE);
  } else {
    // Generic: question / star
    canvas.fillCircle(cx, cy, 10, col);
  }
}

// ---- Weather page ----
void drawWeatherPage() {
  canvas.fillScreen(C_BLACK);
  uint16_t ac = PALETTE[colIdx];

  // Left side: Temperature
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(5);

  String tStr = String((int)round(curTemp));
  canvas.drawString(tStr, 80, 100);

  int w = canvas.textWidth(tStr);

  // Degree + C
  canvas.setTextSize(2);
  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.drawString("o", 80 + w/2 + 6, 75);
  canvas.drawString("C", 80 + w/2 + 14, 95);

  // Right side: Weather icon
  drawWeatherIcon(180, 80, wMain, ac);

  // Location (top)
  canvas.setTextSize(2);
  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.drawString(ow_city + ", " + ow_country, 120, 20);

  // Divider
  canvas.drawFastHLine(20, 140, 200, C_MGRAY);

  // Weather info
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(2);
  canvas.drawString(wMain, 120, 165);

  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.setTextSize(1);
  canvas.drawString(wDesc, 120, 185);

  // Humidity
  canvas.setTextSize(2);
  canvas.drawString(String(curHum) + "%", 120, 210);

  // Page dots
  for (int i = 0; i < 4; i++)
    canvas.fillCircle(100 + i * 14, 225, 3, (i == page) ? ac : C_MGRAY);

  canvas.pushSprite(0, 0);
}

// ---- Forecast page ----
void drawForecastPage() {
  canvas.fillScreen(C_BLACK);
  uint16_t ac = PALETTE[colIdx];

  canvas.setFont(nullptr);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(2);
  canvas.drawString("FORECAST", 120, 22);
  canvas.drawFastHLine(30, 38, 180, ac);

  for (int i = 0; i < 3; i++) {
    int16_t y = 75 + i * 52;
    // Card background
    canvas.fillRoundRect(16, y - 20, 208, 40, 8, C_DGRAY);
    canvas.drawRoundRect(16, y - 20, 208, 40, 8, (i == 0) ? ac : C_MGRAY);

    canvas.setTextColor(C_MGRAY, C_DGRAY);
    canvas.setTextSize(2);
    canvas.setTextDatum(middle_left);
    canvas.drawString(fc[i].label, 30, y);

    canvas.setTextColor(ac, C_DGRAY);
    canvas.setTextSize(2);
    canvas.setTextDatum(middle_right);
    canvas.drawString(String(fc[i].temp) + " C", 212, y);
  }

  // Page dots
  for (int i = 0; i < 4; i++)
    canvas.fillCircle(100 + i * 14, 222, 3, (i == page) ? ac : C_MGRAY);

  canvas.pushSprite(0, 0);
}

// ---- Config-mode screen ----
void drawConfigScreen() {
  canvas.fillScreen(C_BLACK);
  uint16_t ac = PALETTE[colIdx];

  canvas.setFont(nullptr);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(2);
  canvas.drawString("SETUP MODE", 120, 48);
  canvas.drawFastHLine(30, 65, 180, ac);

  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.setTextSize(1);
  canvas.drawString("Connect to WiFi:", 120, 95);
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(2);
  canvas.drawString(AP_SSID, 120, 118);

  canvas.setTextColor(C_MGRAY, C_BLACK);
  canvas.setTextSize(1);
  canvas.drawString("Password: " AP_PASS, 120, 142);
  canvas.drawString("Then open browser:", 120, 165);
  canvas.setTextColor(ac, C_BLACK);
  canvas.setTextSize(2);
  canvas.drawString("192.168.4.1", 120, 188);

  canvas.pushSprite(0, 0);
}

// ---- Splash screen ----
void drawSplash(const char* line1, uint16_t col) {
  canvas.fillScreen(C_BLACK);
  canvas.setFont(nullptr);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(col, C_BLACK);
  canvas.setTextSize(3);
  canvas.drawString(line1, 120, 120);
  canvas.pushSprite(0, 0);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  lcd.init();
  lcd.setRotation(1);    // portrait, eyes centred

  // Allocate full-screen canvas
  canvas.setColorDepth(16);
  canvas.createSprite(240, 240);

  // Allocate single eye sprite
  eyeSpr.setColorDepth(16);
  eyeSpr.createSprite(EW, EH);

  pinMode(TOUCH_PIN, INPUT_PULLDOWN);

  // Splash 1: studio name
  drawSplash("ESC-Labs", C_WHITE);
  delay(900);

  // Splash 2: product name cycling colours
  for (int i = 0; i < NUM_COLOURS; i++) {
    drawSplash("DESK BUDDY", PALETTE[i]);
    delay(220);
  }

  // Load saved settings
  loadPrefs();

  // Connect to WiFi
  drawSplash("Connecting...", PALETTE[colIdx]);

  if (wifi_ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 14000) delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    startAP();
    drawConfigScreen();
    return;
  }

  // WiFi connected
  configTime(gmtOffset, 0, "pool.ntp.org");
  fetchWeather();
  lastWeatherMs = millis();

  // Serve config page while running
  server.on("/",     HTTP_GET,  []{ serveForm(false); });
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  lastTouch  = millis();
  nextBlink  = millis() + random(2500, 5000);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {

  // ---- Config-AP mode: just run the web server ----
  if (cfgMode) {
    dns.processNextRequest();
    server.handleClient();
    delay(10);
    return;
  }

  server.handleClient();

  unsigned long now = millis();

  // ---- Weather refresh ----
  if (now - lastWeatherMs > WEATHER_MS) {
    fetchWeather();
    lastWeatherMs = now;
  }

  // ---- Touch gesture detection ----
  bool touched = digitalRead(TOUCH_PIN);

  if (touched && !lastTouchState && (now - lastDebounce > DEBOUNCE_MS)) {
    // Touch start
    touchDownAt = now;
    touchHeld   = true;
    lastDebounce = now;
  }

  if (!touched && lastTouchState && touchHeld) {
    // Touch release
    touchHeld = false;
    unsigned long held = now - touchDownAt;
    lastDebounce = now;

    if (held >= LONG_PRESS_MS && page == 0) {
      // Long press on eyes → toggle eye shape
      roundEyes = !roundEyes;
    } else if (held < LONG_PRESS_MS) {
      // Short tap
      if (page == 0 && (now - lastReleaseAt) < DTAP_MS) {
        // Double-tap → cycle colour
        colIdx = (colIdx + 1) % NUM_COLOURS;
        saveColour();
      } else {
        // Single tap → next page
        page = (page + 1) % 4;
      }
      lastReleaseAt = now;
      lastTouch = now;
    }
  }

  lastTouchState = touched;

  // ---- Sleep dimming ----
  sleeping = (now - lastTouch > 60000UL);

  // ---- Blink logic (eyes page only) ----
  if (page == 0) {
    if (!blinking && now >= nextBlink) {
      blinking     = true;
      blinkClosing = true;
      blinkSpeed   = sleeping ? 0.08f : 0.18f;
    }

    if (blinking) {
      if (blinkClosing) {
        blinkScale -= blinkSpeed;
        if (blinkScale <= 0.05f) { blinkScale = 0.05f; blinkClosing = false; }
      } else {
        blinkScale += blinkSpeed;
        if (blinkScale >= 1.0f) {
          blinkScale = 1.0f;
          blinking   = false;
          nextBlink  = now + random(sleeping ? 1000 : 3000, sleeping ? 3000 : 7000);
        }
      }
    }
  } else {
    blinkScale = 1.0f;
    blinking   = false;
  }

  // ---- Pupil orbit ----
  eyeAngle += sleeping ? 0.004f : 0.025f;
  if (eyeAngle > TWO_PI) eyeAngle -= TWO_PI;

  // ---- Draw current page ----
  if (page == 0) {
    // Eyes: animate every frame (no throttle needed, full-canvas sprite)
    drawEyesPage();

  } else if (page == 1) {
    // Clock: redraw every second
    if (page != lastPage || now - lastInfoDraw > 1000) {
      drawClockPage();
      lastInfoDraw = now;
    }

  } else if (page == 2) {
    // Weather: redraw every 2s (data changes slowly)
    if (page != lastPage || now - lastInfoDraw > 2000) {
      drawWeatherPage();
      lastInfoDraw = now;
    }

  } else {
    // Forecast: redraw every 5s
    if (page != lastPage || now - lastInfoDraw > 5000) {
      drawForecastPage();
      lastInfoDraw = now;
    }
  }

  lastPage = page;
  delay(20);   // ~50 fps cap; sprite push is DMA so actual loop is smooth
}