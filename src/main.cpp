#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include <LovyanGFX.hpp>
#include "CST816D.h"

// ============================================================
// AYARLAR
// ============================================================
const char* ssid = "";// wifi name 
const char* password = ""; // wifi password
const char* apiKey = "";// apikey for weather

const char* apiCities[4] = {"Bursa", "Tokyo", "Moscow", "New+York"};
const char* apiCountries[4] = {"TR", "JP", "RU", "US"};
const char* uiCities[4] = {"BURSA", "TOKYO", "MOSKOVA", "NEW YORK"};

int selectedCityIndex = 0; 

const long gmtOffsetSec = 3 * 3600;
const int daylightOffsetSec = 0;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

// ============================================================
// PİNLER
// ============================================================
#define LCD_SCLK 5
#define LCD_MOSI 6
#define LCD_MISO -1
#define LCD_DC   2
#define LCD_CS   3       
#define LCD_RST  8
#define TOUCH_SDA 11
#define TOUCH_SCL 7
#define TOUCH_RST 10 

// ============================================================
// LovyanGFX
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI2_HOST;
    cfg.pin_sclk = LCD_SCLK;
    cfg.pin_mosi = LCD_MOSI;
    cfg.pin_miso = LCD_MISO;
    cfg.pin_dc   = LCD_DC;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    auto p_cfg = _panel_instance.config();
    p_cfg.pin_cs   = LCD_CS;
    p_cfg.pin_rst  = LCD_RST;
    p_cfg.pin_busy = -1;
    p_cfg.memory_width  = 240;
    p_cfg.memory_height = 280;
    p_cfg.panel_width   = 240;
    p_cfg.panel_height  = 280;
    p_cfg.offset_x      = 0;
    p_cfg.offset_y      = 20; 
    p_cfg.readable      = false;
    p_cfg.invert        = true;
    p_cfg.rgb_order     = false;
    _panel_instance.config(p_cfg);
    setPanel(&_panel_instance);
  }
};

LGFX gfx;
lgfx::LGFX_Sprite canvas(&gfx); 
CST816D touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, -1);

int SCREEN_W = 240;
int SCREEN_H = 280;

// Hava Durumu (Saatlik grafik 6 noktalı)
float currentTemp = 0.0;
int currentHumidity = 0;
String currentCondition = "";
float forecastTemps[5] = {0, 0, 0, 0, 0};
String forecastConditions[5];
float hourlyTemps[6] = {0};   
String hourlyTimes[6];        

const char* turkishDayFull[7]  = {"PAZAR", "PAZARTESI", "SALI", "CARSAMBA", "PERSEMBE", "CUMA", "CUMARTESI"};
const char* turkishDayAbbr[7]  = {"PAZ", "PZT", "SAL", "CAR", "PER", "CUM", "CMT"};

int currentWeekday = 1;               
char lastUpdateStr[20] = "--:--"; 

int displayMode = 0; 
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 600000UL; 

bool wasTouched = false;
unsigned long lastDinoFrame = 0; 

// ============================================================
// DİNO OYUNU DEĞİŞKENLERİ
// ============================================================
// gameState: 0 = Akıyor, 1 = UFO Işınlama, 2 = Portal Geliyor, 3 = Ekran Patlaması
int gameState = 0; 
// worldType: 0 = Dünya, 1 = Uzaylı, 2 = Cehennem
int worldType = 0; 
int nextLevelScore = 200; 

float dinoX = 30.0;
float dinoY = 205.0;        
float dinoVy = 0.0;         
const float gravity = 1.6;  
const float jumpForce = -16.5; 
bool isJumping = false;
float obstacleX = 260.0;    
float obstacleSpeed = 8.0;  
int score = 0;
int highScore = 0; 
bool gameOver = false;

int cactusType = 1; 
int cactusH1 = 2, cactusH2 = 2, cactusH3 = 2; 

// Gökyüzü Cisimleri (Bulut, UFO, Kuş)
float cloud1X = 240.0, cloud1Y = 40.0;
float cloud2X = 320.0, cloud2Y = 70.0;
float cloud3X = 400.0, cloud3Y = 50.0;
const float cloudSpeed = 1.5; 

// Işınlanma ve Portal Animasyonu
int abductionPhase = 0;
float ufoAnimX = 120.0;
float ufoAnimY = -50.0;
float portalX = 0.0;
int flashFrames = 0;

uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
  return gfx.color565(r, g, b);
}

// ============================================================
// YARDIMCI FONKSİYONLAR
// ============================================================
void syncTimeInfo() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 2000)) {
    currentWeekday = timeinfo.tm_wday;
    snprintf(lastUpdateStr, sizeof(lastUpdateStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }
}

String conditionToTurkish(String condition) {
  if (condition == "Clear") return "GUNESLI";
  if (condition == "Clouds") return "PARCALI BULUT"; 
  if (condition == "Rain") return "YAGMUR";
  if (condition == "Drizzle") return "INCE YAGMUR";
  if (condition == "Thunderstorm") return "FIRTINA";
  if (condition == "Snow") return "KAR";
  return condition;
}

void drawBackButton(LovyanGFX* targetGfx = &gfx) {
  targetGfx->fillRoundRect(5, 5, 80, 30, 6, RGB(200, 50, 50));
  targetGfx->setTextSize(2);
  targetGfx->setTextColor(TFT_WHITE);
  targetGfx->setCursor(15, 12);
  targetGfx->print("<GERI");
}

void drawLastUpdateTime(LovyanGFX* targetGfx = &gfx, uint16_t color = RGB(180, 180, 180)) {
  targetGfx->setTextSize(1);
  targetGfx->setTextColor(color);
  targetGfx->setCursor(SCREEN_W - 130, 265);
  targetGfx->print("Guncellendi: ");
  targetGfx->print(lastUpdateStr);
}

// ============================================================
// SAYFA ÇİZİMLERİ 
// ============================================================
void drawCitySelection() {
  gfx.fillScreen(TFT_BLACK);
  int midX = SCREEN_W / 2;
  int midY = SCREEN_H / 2;

  gfx.fillRect(0, 0, midX, midY, RGB(45, 60, 80)); 
  gfx.fillRect(midX, 0, midX, midY, RGB(80, 45, 55)); 
  gfx.fillRect(0, midY, midX, midY, RGB(45, 80, 55)); 
  gfx.fillRect(midX, midY, midX, midY, RGB(80, 70, 45)); 

  gfx.drawFastHLine(0, midY, SCREEN_W, TFT_WHITE);
  gfx.drawFastVLine(midX, 0, SCREEN_H, TFT_WHITE);

  gfx.setTextSize(2);
  gfx.setTextColor(TFT_WHITE);
  
  gfx.setCursor(25, midY / 2 - 10);  gfx.print("BURSA");
  gfx.setCursor(midX + 25, midY / 2 - 10); gfx.print("TOKYO");
  gfx.setCursor(15, midY + (midY / 2) - 10); gfx.print("MOSKOVA");
  gfx.setCursor(midX + 10, midY + (midY / 2) - 10); gfx.print("NEW YORK");
}

void drawCurrentWeather() {
  if (currentCondition == "Clear") {
    gfx.fillScreen(RGB(40, 140, 255));
    gfx.fillCircle(120, 125, 55, RGB(255, 220, 0));
  } else if (currentCondition == "Clouds") {
    gfx.fillScreen(RGB(40, 50, 60));
    gfx.fillCircle(105, 110, 22, RGB(160, 170, 180));
    gfx.fillCircle(135, 100, 28, RGB(160, 170, 180));
    gfx.fillCircle(160, 112, 22, RGB(160, 170, 180));
    gfx.fillRoundRect(90, 110, 85, 30, 15, RGB(160, 170, 180));
  } else if (currentCondition == "Rain" || currentCondition == "Drizzle" || currentCondition == "Thunderstorm") {
    gfx.fillScreen(RGB(30, 40, 50));
    gfx.fillCircle(105, 95, 22, RGB(100, 110, 120));
    gfx.fillCircle(135, 88, 28, RGB(100, 110, 120));
    gfx.fillCircle(160, 100, 22, RGB(100, 110, 120));
    gfx.fillRoundRect(90, 98, 85, 30, 15, RGB(100, 110, 120));
    gfx.drawLine(110, 135, 105, 148, RGB(80, 190, 255));
    gfx.drawLine(135, 135, 130, 148, RGB(80, 190, 255));
    gfx.drawLine(160, 135, 155, 148, RGB(80, 190, 255));
  } else {
    gfx.fillScreen(RGB(40, 50, 70));
  }

  drawBackButton(); 
  gfx.setTextSize(2);
  gfx.setTextColor(TFT_WHITE);
  gfx.setCursor(95, 12);
  gfx.println("HAVA");

  gfx.setTextColor(RGB(0, 255, 255));
  int cityTextX = 120 - (strlen(uiCities[selectedCityIndex]) * 6);
  gfx.setCursor(cityTextX, 42);
  gfx.println(uiCities[selectedCityIndex]);

  gfx.setTextSize(1);
  gfx.setTextColor(RGB(230, 230, 230));
  int dayTextX = 120 - (strlen(turkishDayFull[currentWeekday]) * 3); 
  gfx.setCursor(dayTextX, 62);
  gfx.println(turkishDayFull[currentWeekday]);

  gfx.setTextSize(3);
  gfx.setTextColor(TFT_YELLOW);
  gfx.setCursor(85, 112);
  
  if(currentTemp == 0.0) gfx.print("--"); 
  else gfx.print(currentTemp, 1);
  
  int cx = gfx.getCursorX();
  int cy = gfx.getCursorY();
  gfx.drawCircle(cx + 6, cy + 4, 4, TFT_YELLOW);
  gfx.setCursor(cx + 16, cy);
  gfx.println("C");

  gfx.setTextSize(2);
  gfx.setTextColor(RGB(200, 230, 255));
  gfx.setCursor(80, 205);
  gfx.print("Nem: %");
  gfx.print(currentHumidity);
  
  gfx.setTextSize(1);
  gfx.setTextColor(TFT_LIGHTGREY);
  gfx.setCursor(10, 265);
  gfx.print("Diger Sayfa ->");
  drawLastUpdateTime();
}

void drawHourlyChart() {
  gfx.fillScreen(RGB(30, 40, 65));
  drawBackButton();
  gfx.setTextSize(2);
  gfx.setTextColor(RGB(255, 255, 200));
  gfx.setCursor(95, 12);
  gfx.println("GRAFIK");

  float minT = hourlyTemps[0], maxT = hourlyTemps[0];
  for(int i = 1; i < 6; i++){
    if(hourlyTemps[i] < minT) minT = hourlyTemps[i];
    if(hourlyTemps[i] > maxT) maxT = hourlyTemps[i];
  }
  if (maxT == minT) { maxT += 1.0; minT -= 1.0; } 

  int graphX = 25;
  int graphY = 80;
  int graphW = 190;
  int graphH = 110;

  for (int i = 0; i < 5; i++) {
    int x1 = graphX + (i * graphW / 5);
    int y1 = graphY + graphH - ((hourlyTemps[i] - minT) / (maxT - minT) * graphH);
    int x2 = graphX + ((i + 1) * graphW / 5);
    int y2 = graphY + graphH - ((hourlyTemps[i+1] - minT) / (maxT - minT) * graphH);

    gfx.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    gfx.drawLine(x1, y1-1, x2, y2-1, TFT_YELLOW); 
  }

  for (int i = 0; i < 6; i++) {
    int x = graphX + (i * graphW / 5);
    int y = graphY + graphH - ((hourlyTemps[i] - minT) / (maxT - minT) * graphH);

    gfx.drawFastVLine(x, y + 5, 215 - y, RGB(70, 80, 100));

    gfx.fillCircle(x, y, 3, TFT_WHITE);
    
    gfx.setTextSize(1);
    gfx.setTextColor(RGB(0, 255, 255));
    int tempVal = (int)hourlyTemps[i];
    int offset = (tempVal >= 10 || tempVal <= -10) ? 6 : 3; 
    gfx.setCursor(x - offset, y - 14);
    gfx.print(tempVal);

    gfx.setTextColor(TFT_LIGHTGREY);
    gfx.setCursor(x - 14, 220); 
    gfx.print(hourlyTimes[i]);
  }

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_LIGHTGREY);
  gfx.setCursor(10, 265);
  gfx.print("Diger Sayfa ->");
  drawLastUpdateTime();
}

void drawWeeklyForecast() {
  gfx.fillScreen(RGB(30, 40, 65));
  drawBackButton();

  gfx.setTextSize(2);
  gfx.setTextColor(RGB(255, 255, 200));
  gfx.setCursor(95, 12);
  gfx.println("TAHMIN");

  for (int i = 0; i < 5; i++) {
    int y = 45 + (i * 40);
    int dayIdx = (currentWeekday + i + 1) % 7;

    gfx.setTextSize(1);
    gfx.setTextColor(TFT_WHITE);
    gfx.setCursor(12, y + 4);
    gfx.print(turkishDayAbbr[dayIdx]);

    int iconX = 85;
    int iconY = y + 8;
    if (forecastConditions[i] == "Clear") {
      gfx.fillCircle(iconX, iconY, 5, RGB(255, 210, 0)); 
    } else if (forecastConditions[i] == "Clouds") {
      gfx.fillCircle(iconX - 3, iconY, 3, RGB(180, 190, 200));
      gfx.fillCircle(iconX + 3, iconY - 2, 4, RGB(180, 190, 200)); 
    } else {
      gfx.fillCircle(iconX, iconY, 4, RGB(80, 190, 255)); 
    }

    gfx.setCursor(135, y);
    gfx.setTextSize(2);
    gfx.setTextColor(TFT_YELLOW); 
    if(forecastTemps[i] == 0.0) gfx.print("--"); 
    else gfx.print(forecastTemps[i], 1);
    
    int cx = gfx.getCursorX();
    int cy = gfx.getCursorY();
    gfx.drawCircle(cx + 3, cy + 3, 2, TFT_YELLOW);
    gfx.setCursor(cx + 8, cy);
    gfx.print("C");

    gfx.setTextSize(1);
    gfx.setTextColor(RGB(0, 255, 255));
    gfx.setCursor(12, y + 16);
    gfx.print(conditionToTurkish(forecastConditions[i]));
    
    gfx.drawFastHLine(12, y + 26, 215, RGB(90, 100, 120));
  }
  
  gfx.setTextSize(1);
  gfx.setTextColor(TFT_LIGHTGREY);
  gfx.setCursor(10, 265);
  gfx.print("Oyun Icin Dokun");
  drawLastUpdateTime();
}

// ============================================================
// OYUN GRAFİK ÇİZİMLERİ (Bulut, UFO, Kuş, Kaktüs, Piramit, Lav)
// ============================================================
void drawCloud(float x, float y) {
  uint16_t cloudColor = RGB(150, 200, 255); 
  canvas.fillCircle(x, y, 6, cloudColor);
  canvas.fillCircle(x + 10, y - 4, 8, cloudColor);
  canvas.fillCircle(x + 20, y, 6, cloudColor);
  canvas.fillRoundRect(x, y - 2, 20, 8, 4, cloudColor);
}

void drawAlienUFO(float x, float y) {
  canvas.fillCircle(x + 12, y, 6, RGB(100, 255, 255)); 
  canvas.fillRoundRect(x, y + 2, 24, 6, 3, RGB(140, 140, 140)); 
  canvas.fillCircle(x + 5, y + 5, 2, TFT_RED); 
  canvas.fillCircle(x + 19, y + 5, 2, TFT_RED);
}

void drawBird(float x, float y) {
  canvas.drawLine(x, y, x - 6, y - 4, TFT_BLACK);
  canvas.drawLine(x, y, x + 6, y - 4, TFT_BLACK);
  canvas.drawLine(x, y - 1, x - 6, y - 5, TFT_BLACK); 
  canvas.drawLine(x, y - 1, x + 6, y - 5, TFT_BLACK);
}

void drawSingleCactus(float x, int hType) {
  uint16_t color = RGB(34, 170, 50);
  int h = (hType == 1) ? 20 : (hType == 2) ? 30 : 40;
  int yTop = 225 - h; 
  canvas.fillRect((int)x + 5, yTop, 6, h, color);
  canvas.fillRect((int)x, yTop + (h/2) - 2, 5, 4, color); 
  canvas.fillRect((int)x, yTop + (h/2) - 10, 4, 12, color);
  canvas.fillRect((int)x + 11, yTop + (h/2) + 2, 5, 4, color);
  canvas.fillRect((int)x + 12, yTop + (h/2) - 6, 4, 12, color);
}

void drawSinglePyramid(float x, int hType) {
  int h = (hType == 1) ? 20 : (hType == 2) ? 30 : 40;
  int base = 16;
  int yTop = 225 - h;
  canvas.fillTriangle(x, 225, x + base, 225, x + base/2, yTop, RGB(220, 180, 50));
  canvas.drawLine(x + base/2, yTop, x + base/2 + 3, 225, RGB(180, 120, 30));
}

void drawLava(float x, int hType) {
  int h = (hType == 1) ? 20 : (hType == 2) ? 30 : 40;
  int yTop = 225 - h;
  uint16_t lavaRed = RGB(255, 30, 0); 
  
  canvas.fillRect((int)x, yTop, 16, h, lavaRed);
  canvas.fillCircle((int)x + 4, yTop, 4, lavaRed); 
  canvas.fillCircle((int)x + 12, yTop, 4, lavaRed);
  
  canvas.fillRect((int)x + 4, yTop + 6, 8, 4, TFT_ORANGE);
  canvas.fillRect((int)x + 8, yTop + 14, 4, 6, TFT_YELLOW);
}

void resetDinoGame() {
  gameState = 0;
  worldType = 0;
  nextLevelScore = 200;
  dinoX = 30.0;
  dinoY = 205.0;
  dinoVy = 0.0;
  isJumping = false;
  obstacleX = SCREEN_W + 40.0;
  obstacleSpeed = 8.0;
  score = 0;
  gameOver = false;
  abductionPhase = 0;
  portalX = 0;
  
  cloud1X = SCREEN_W; cloud1Y = 40.0;
  cloud2X = SCREEN_W + 80.0; cloud2Y = 70.0;
  cloud3X = SCREEN_W + 160.0; cloud3Y = 50.0;
  cactusType = 1; cactusH1 = 2;
}

// ============================================================
// OYUN MOTORU
// ============================================================
void updateDinoGame() {
  canvas.startWrite();

  if (gameState == 3) {
    uint16_t nextBg = (worldType == 0) ? TFT_WHITE : (worldType == 1) ? RGB(255, 130, 130) : TFT_BLACK;
    canvas.fillScreen(nextBg); 
    flashFrames--;
    if (flashFrames <= 0) {
      worldType = (worldType + 1) % 3; 
      
      if (worldType == 1) { 
        nextLevelScore = score + 300; 
      } else if (worldType == 2) { 
        nextLevelScore = score + 400; 
      } else { 
        nextLevelScore = score + 200; 
        obstacleSpeed += 1.5; 
      }

      gameState = 0;             
      dinoX = 30.0;
      dinoY = 205.0;
      obstacleX = SCREEN_W + 50.0;
      abductionPhase = 0;
    }
    canvas.pushSprite(0, 0);
    canvas.endWrite();
    return;
  }

  uint16_t bgColor = (worldType == 0) ? TFT_BLACK : (worldType == 1) ? TFT_WHITE : RGB(255, 130, 130);
  uint16_t fgColor = (worldType == 0) ? TFT_WHITE : TFT_BLACK; 
  
  canvas.fillScreen(bgColor);

  if (gameOver) {
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(60, 60);
    canvas.println("OYUN BITTI!");
    
    canvas.setTextSize(1);
    canvas.setTextColor(fgColor); 
    canvas.setCursor(75, 100);
    canvas.print("Skor: "); canvas.println(score);

    canvas.setCursor(75, 120);
    canvas.setTextColor(RGB(200, 150, 0)); 
    canvas.print("En Yuksek: "); canvas.println(highScore);
    
    canvas.setCursor(35, 160);
    canvas.setTextColor((worldType == 1) ? RGB(0, 150, 0) : TFT_LIGHTGREEN); 
    canvas.println("Yeniden Oyna: Ekrana Dokun");
    
    canvas.fillRoundRect(5, 5, 90, 30, 6, RGB(200, 50, 50));
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 12);
    canvas.print("ANA MENU");

    canvas.pushSprite(0, 0);
    canvas.endWrite();
    return;
  }

  if (gameState == 0 || gameState == 2) {
    dinoVy += gravity;
    dinoY += dinoVy;
    if (dinoY >= 205.0) {
      dinoY = 205.0;
      isJumping = false;
      dinoVy = 0.0;
    }

    cloud1X -= cloudSpeed; cloud2X -= cloudSpeed; cloud3X -= cloudSpeed;
    if (cloud1X < -30) { cloud1X = SCREEN_W + random(10, 50); cloud1Y = random(30, 90); }
    if (cloud2X < -30) { cloud2X = SCREEN_W + random(30, 80); cloud2Y = random(40, 100); }
    if (cloud3X < -30) { cloud3X = SCREEN_W + random(50, 120); cloud3Y = random(20, 80); }

    if (gameState == 0) {
      obstacleX -= obstacleSpeed;
      if (obstacleX < -50) {
        obstacleX = SCREEN_W + random(20, 60);
        
        if (cactusType == 3) score += 30; else score += 10;
        if (score > highScore) highScore = score;
        
        if (worldType == 0 && obstacleSpeed < 16.0) obstacleSpeed += 0.2; 
        
        cactusType = (random(0, 2) == 0) ? 1 : 3;
        cactusH1 = random(1, 4); cactusH2 = random(1, 4); cactusH3 = random(1, 4);
      }

      int maxHType = cactusH1;
      if (cactusType == 3) {
        if (cactusH2 > maxHType) maxHType = cactusH2;
        if (cactusH3 > maxHType) maxHType = cactusH3;
      }
      int pixelH = (maxHType == 1) ? 20 : (maxHType == 2) ? 30 : 40;
      int cactusTopY = 225 - pixelH;
      int cactusTotalWidth = (cactusType == 1) ? 16 : 44; 
      
      if (obstacleX < dinoX + 18 && obstacleX + cactusTotalWidth > dinoX && dinoY + 20 > cactusTopY) {
        gameOver = true;
        if (score > highScore) highScore = score;
      }

      if (score >= nextLevelScore) {
        if (worldType == 0) {
          gameState = 1; 
          abductionPhase = 0; 
          ufoAnimX = SCREEN_W / 2;
          ufoAnimY = -50; 
        } else {
          gameState = 2; 
          portalX = SCREEN_W + 30;
        }
      }
    } 
    else if (gameState == 2) {
      portalX -= obstacleSpeed;
      if (dinoX + 18 >= portalX - 15) { 
        gameState = 3; 
        flashFrames = 15; 
        score += 10;
      }
    }
  } 
  else if (gameState == 1) {
    if (abductionPhase == 0) {
      if (dinoY < 205.0) {
        dinoVy += gravity;
        dinoY += dinoVy;
        if (dinoY > 205.0) dinoY = 205.0; 
      } else {
        dinoY = 205.0; dinoVy = 0.0;
        abductionPhase = 1; 
      }
    } 
    else if (abductionPhase == 1) {
      if (dinoX < (SCREEN_W / 2) - 9) dinoX += 2.0;
      else abductionPhase = 2; 
    }
    else if (abductionPhase == 2) {
      if (ufoAnimY < 60.0) ufoAnimY += 2.0;
      else abductionPhase = 3; 
    }
    else if (abductionPhase == 3) {
      dinoY -= 3.0;
      if (dinoY < ufoAnimY + 5) {
        gameState = 3; 
        flashFrames = 15; 
      }
    }
  }

  if (worldType == 0) {
    drawCloud(cloud1X, cloud1Y); drawCloud(cloud2X, cloud2Y); drawCloud(cloud3X, cloud3Y);
  } else if (worldType == 1) {
    drawAlienUFO(cloud1X, cloud1Y); drawAlienUFO(cloud2X, cloud2Y); drawAlienUFO(cloud3X, cloud3Y);
  } else {
    drawBird(cloud1X, cloud1Y); drawBird(cloud2X, cloud2Y); drawBird(cloud3X, cloud3Y); 
  }

  canvas.drawFastHLine(0, 225, SCREEN_W, fgColor);
  drawBackButton(&canvas); 

  canvas.setTextSize(1);
  canvas.setTextColor(fgColor); 
  canvas.setCursor(SCREEN_W - 120, 10);
  canvas.print("Skor:"); canvas.print(score);
  canvas.print(" Max:"); canvas.print(highScore);

  if (gameState == 0 || gameState == 1) {
    if (worldType == 0) {
      if (cactusType == 1) drawSingleCactus(obstacleX, cactusH1);
      else { drawSingleCactus(obstacleX, cactusH1); drawSingleCactus(obstacleX + 14, cactusH2); drawSingleCactus(obstacleX + 28, cactusH3); }
    } else if (worldType == 1) {
      if (cactusType == 1) drawSinglePyramid(obstacleX, cactusH1);
      else { drawSinglePyramid(obstacleX, cactusH1); drawSinglePyramid(obstacleX + 14, cactusH2); drawSinglePyramid(obstacleX + 28, cactusH3); }
    } else {
      if (cactusType == 1) drawLava(obstacleX, cactusH1);
      else { drawLava(obstacleX, cactusH1); drawLava(obstacleX + 14, cactusH2); drawLava(obstacleX + 28, cactusH3); }
    }
  }

  if (gameState == 2) {
    canvas.fillCircle(portalX, 205, 30, RGB(60, 60, 60)); 
    canvas.fillCircle(portalX, 205, 20, RGB(20, 20, 20)); 
  }

  if (gameState == 1) {
    if (abductionPhase == 3) {
      canvas.fillTriangle(ufoAnimX, ufoAnimY + 15, dinoX - 10, 225, dinoX + 28, 225, RGB(255, 255, 100));
    }
    if (abductionPhase >= 2) {
      canvas.fillCircle(ufoAnimX, ufoAnimY, 15, RGB(100, 255, 255)); 
      canvas.fillRoundRect(ufoAnimX - 35, ufoAnimY, 70, 14, 7, RGB(180, 180, 190)); 
      canvas.fillRoundRect(ufoAnimX - 20, ufoAnimY + 10, 40, 8, 4, RGB(100, 100, 100)); 
      
      canvas.fillCircle(ufoAnimX - 25, ufoAnimY + 7, 3, TFT_RED);
      canvas.fillCircle(ufoAnimX - 10, ufoAnimY + 9, 3, TFT_GREEN);
      canvas.fillCircle(ufoAnimX + 10, ufoAnimY + 9, 3, TFT_GREEN);
      canvas.fillCircle(ufoAnimX + 25, ufoAnimY + 7, 3, TFT_RED);
    }
  }

  uint16_t dinoColor = (worldType == 0) ? TFT_WHITE : (worldType == 1) ? TFT_GREEN : RGB(139, 0, 0);
  uint16_t eyeColor = (worldType == 1) ? TFT_RED : TFT_BLACK;
  
  canvas.fillRect((int)dinoX, (int)dinoY, 18, 20, dinoColor); 
  canvas.fillRect((int)dinoX + 12, (int)dinoY + 4, 3, 3, eyeColor); 

  canvas.pushSprite(0, 0);
  canvas.endWrite();
}

// ============================================================
// VERİ ÇEKME İŞLEMİ (API JSON)
// ============================================================
bool fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  bool success = true;

  syncTimeInfo();

  String selectedApiCity = apiCities[selectedCityIndex];
  String selectedApiCountry = apiCountries[selectedCityIndex];

  String currentPath = "http://api.openweathermap.org/data/2.5/weather?q=" + selectedApiCity + "," + selectedApiCountry + "&units=metric&appid=" + String(apiKey);
  http.begin(client, currentPath);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, http.getString())) {
      currentTemp = doc["main"]["temp"] | 0.0;
      currentHumidity = doc["main"]["humidity"] | 0;
      currentCondition = doc["weather"][0]["main"].as<String>();
    }
  } else success = false;
  http.end();

  String forecastPath = "http://api.openweathermap.org/data/2.5/forecast?q=" + selectedApiCity + "," + selectedApiCountry + "&units=metric&appid=" + String(apiKey);
  http.begin(client, forecastPath);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(24576); 
    DeserializationError error = deserializeJson(doc, http.getString());
    if (!error) {
      JsonArray list = doc["list"].as<JsonArray>();
      int dayIndex = 0;
      
      for (int i = 0; i < list.size(); i++) {
        if (i < 6) {
          hourlyTemps[i] = list[i]["main"]["temp"] | 0.0;
          String dt_txt = list[i]["dt_txt"].as<String>();
          
          int utcHour = dt_txt.substring(11, 13).toInt();
          int localHour = (utcHour + 3) % 24;
          
          char timeBuf[6];
          sprintf(timeBuf, "%02d:00", localHour);
          hourlyTimes[i] = String(timeBuf); 
        }

        if (i % 8 == 0 && dayIndex < 5) {
          forecastTemps[dayIndex] = list[i]["main"]["temp"] | 0.0;
          forecastConditions[dayIndex] = list[i]["weather"][0]["main"].as<String>();
          dayIndex++;
        }
      }
    } else success = false;
  } else success = false;
  
  http.end();
  return success;
}

// ============================================================
// DOKUNMATİK EKRAN DEĞİŞİMİ
// ============================================================
void updateScreen() {
  if (displayMode == 0) drawCitySelection();
  else if (displayMode == 1) drawCurrentWeather();
  else if (displayMode == 2) drawHourlyChart(); 
  else if (displayMode == 3) drawWeeklyForecast();
  else if (displayMode == 4) resetDinoGame();
}

void checkTouch() {
  uint16_t raw_x = 0, raw_y = 0;
  uint8_t gesture = 0;
  bool touched = touch.getTouch(&raw_x, &raw_y, &gesture);

  if (touched && !wasTouched) {
    wasTouched = true; 
    
    uint16_t x = raw_y;
    uint16_t y = 240 - raw_x;

    bool isBackButton = (x < 90 && y < 50);

    if (displayMode == 0) {
      if (x < SCREEN_W / 2 && y < SCREEN_H / 2) selectedCityIndex = 0; 
      else if (x >= SCREEN_W / 2 && y < SCREEN_H / 2) selectedCityIndex = 1; 
      else if (x < SCREEN_W / 2 && y >= SCREEN_H / 2) selectedCityIndex = 2; 
      else selectedCityIndex = 3; 

      gfx.fillScreen(TFT_BLACK);
      gfx.setTextSize(2);
      gfx.setTextColor(TFT_WHITE);
      gfx.setCursor(20, SCREEN_H / 2 - 10);
      gfx.println("Veri Cekiliyor...");
      
      fetchWeatherData();
      displayMode = 1; 
      updateScreen();
    } 
    else if (displayMode == 1) { 
      if (isBackButton) displayMode = 0; 
      else displayMode = 2; 
      updateScreen();
    }
    else if (displayMode == 2) { 
      if (isBackButton) displayMode = 1; 
      else displayMode = 3; 
      updateScreen();
    }
    else if (displayMode == 3) { 
      if (isBackButton) displayMode = 2; 
      else displayMode = 4; 
      updateScreen();
    }
    else if (displayMode == 4) { 
      if (isBackButton) {
        if (gameOver) {
          displayMode = 0; // ANA MENÜ butonuna basıldığında direkt Şehir Seçimine döner!
          updateScreen();
        } else {
          displayMode = 3; // Oyun oynanırken normal geri tuşuna basılırsa Haftalık Tahmine döner
          updateScreen();
        }
      } else {
        if (!gameOver && !isJumping && (gameState == 0 || gameState == 2)) {
          dinoVy = jumpForce;
          isJumping = true;
        } else if (gameOver) {
          resetDinoGame();
        }
      }
    }
  } 
  if (!touched) wasTouched = false;
}

// ============================================================
// KURULUM
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  gfx.init();
  gfx.setRotation(1);
  SCREEN_W = gfx.width();
  SCREEN_H = gfx.height();
  canvas.createSprite(SCREEN_W, SCREEN_H);

  gfx.fillScreen(TFT_BLACK);
  gfx.setTextSize(2);
  gfx.setTextColor(TFT_WHITE);
  gfx.setCursor(20, 100);
  gfx.println("Baglaniyor...");

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(50);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000); 
  touch.begin();

  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer1, ntpServer2);
    syncTimeInfo();
  }
  
  displayMode = 0; 
  updateScreen();
  lastWeatherUpdate = millis();
}

void loop() {
  checkTouch();

  if (displayMode == 4) {
    if (millis() - lastDinoFrame >= 20) { 
      updateDinoGame();
      lastDinoFrame = millis();
    }
  }

  if (millis() - lastWeatherUpdate >= weatherUpdateInterval && displayMode != 0) {
    fetchWeatherData();
    if (displayMode != 4) updateScreen();
    lastWeatherUpdate = millis();
  }
}