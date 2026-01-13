#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Deneyap_BasincOlcer.h> 
#include <Adafruit_SHT4x.h>      
#include <Adafruit_NeoPixel.h>
#include "time.h"
#include "esp32-hal-ledc.h"
#include <math.h>

// ==========================================
// --- KULLANICI AYARLARI ---
const char* ssid     = "***";       
const char* password = "***";    
// *** API KEY BURAYA ***
const char* apiKey   = "***";
// KONUM
String latitude  = "40.14"; 
String longitude = "29.98"; 
String city      = "Bilecik"; 

#define MAX_RECORD_SECONDS 10 
// ==========================================

// --- NOTALAR ---
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_CS4 277
#define NOTE_A5  880
#define NOTE_F4  349
#define NOTE_AS5 932
#define REST 0

// --- SISTEM AYARLARI ---
int sesEsigi    = 300; 
int sistemSesSeviyesi = 255; 

// --- PINLER ---
#define I2S_SCK D0  
#define I2S_WS  D1  
#define I2S_SD  D4  

#define SPEAKER_PIN D8 
#define POT_PIN     A0   
#define BTN_PIN     D14  
#define RGB_PIN     D9 
#define NUMPIXELS   1   

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3D 
#define I2S_PORT I2S_NUM_0

#define PWM_FREQ 2000
#define PWM_RESOLUTION 8
#define PWM_CHANNEL 0 

#define SAMPLE_RATE 16000
#define SAMPLE_BITS 16
#define WAV_HEADER_SIZE 44

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
AtmosphericPressure BaroSensor; 
Adafruit_SHT4x sht4;            
Adafruit_NeoPixel pixels(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// DEĞİŞKENLER
int aktifUygulama = 0; 
int seciliMenu = 1; 
const int toplamMenu = 7; 

const char* aylarTR[] = {"OCAK", "SUBAT", "MART", "NISAN", "MAYIS", "HAZIRAN", "TEMMUZ", "AGUSTOS", "EYLUL", "EKIM", "KASIM", "ARALIK"};
const char* gunlerKisa[] = {"Pz", "Pt", "Sa", "Ca", "Pe", "Cu", "Ct"};

int havaKodlari[5];
float maxSicaklik[5];
float minSicaklik[5];
String gunIsimleri[5]; 
bool havaVerisiVar = false;

String netSaat = "--:--";
String netTarih = "--.--.--";
String hataMesaji = ""; 

String sonCevap = "";     
int currentLen = 0;       
unsigned long lastCharTime = 0; 
String sohbetGecmisi = ""; 
bool muzikMenuAktif = false;

int lastStabilPot = 0; 
unsigned long sonGozKirpma = 0;
bool gozlerKapali = false;
unsigned long sonHavaGuncelleme = 0;
int rgbModu = 0; 
unsigned long lastDiskoTime = 0; 

// SAAT MODU (True=Dijital, False=Analog)
bool saatModuDijital = true; 

// POMODORO DEĞİŞKENLERİ
unsigned long pomoBaslangic = 0;
unsigned long pomoToplamGecen = 0;     
bool pomoCalisiyor = false;
bool pomoBitti = false;
bool pomoDuraklatildi = false;

// Yeni Ayar Değişkenleri
bool pomoEditMode = false;
unsigned long pomoSure = 25 * 60 * 1000; // Varsayılan 25dk (Değişebilir)
int pomoPresets[] = {1, 10, 25, 30, 60}; // Seçenekler
int pomoSecilenIndex = 2; // Varsayılan 25 (Dizinin 2. elemanı)

const i2s_config_t i2s_config = {
  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
  .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 8,
  .dma_buf_len = 64,
  .use_apll = false
};
const i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = -1, .data_in_num = I2S_SD };

// --- YARDIMCI FONKSİYONLAR ---

String trDuzelt(String text) {
  text.replace("ı", "i"); text.replace("İ", "I");
  text.replace("ş", "s"); text.replace("Ş", "S");
  text.replace("ğ", "g"); text.replace("Ğ", "G");
  text.replace("ü", "u"); text.replace("Ü", "U");
  text.replace("ö", "o"); text.replace("Ö", "O");
  text.replace("ç", "c"); text.replace("Ç", "C");
  text.replace("\"", ""); text.replace("\n", " ");
  return text;
}

void playTone(int freq, int duration) {
  if(sistemSesSeviyesi > 10) {
    int duty = map(sistemSesSeviyesi, 0, 255, 0, 128); 
    ledcSetup(PWM_CHANNEL, freq, PWM_RESOLUTION);
    ledcAttachPin(SPEAKER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, duty); delay(duration); ledcWrite(PWM_CHANNEL, 0); 
  } else { delay(duration); }
}

void playStart() { playTone(1000, 100); delay(50); playTone(2000, 100); }
void playClick() { playTone(2000, 30); } 
void playEnter() { playTone(1500, 50); delay(50); playTone(2500, 80); }
void playResponse() { playTone(800, 100); delay(50); playTone(1200, 200); } 
void playRecStart() { playTone(600, 100); }
void playAlarm() { 
  // Alarm Sesi (Daha belirgin)
  for(int i=0; i<4; i++) { 
    playTone(2000, 100); delay(50); 
    playTone(2000, 100); delay(50); 
    playTone(2000, 100); delay(400); 
  }
}
void playTypeSound() { int freq = random(1000, 3000); int duration = random(30, 60); playTone(freq, duration); }

// --- MÜZİK ---
void calShapeOfYou() { playTone(NOTE_CS4, 200); delay(250); playTone(NOTE_E4, 200); delay(250); }
void calStillDre() { playTone(NOTE_A5, 150); delay(180); playTone(NOTE_A5, 150); delay(180); }
void calWayDownWeGo() { playTone(NOTE_F4, 300); delay(350); playTone(NOTE_AS5, 300); delay(350); }

void appMuzikMenu() {
  int potVal = analogRead(POT_PIN); int secim = map(potVal, 0, 4096, 1, 5); if(secim > 4) secim = 4;
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 0); display.print("MUZIK SEC"); display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(5, 20); if(secim==1) display.print(">"); else display.print(" "); display.print("Shape of You");
  display.setCursor(5, 30); if(secim==2) display.print(">"); else display.print(" "); display.print("Still D.R.E");
  display.setCursor(5, 40); if(secim==3) display.print(">"); else display.print(" "); display.print("Way Down We Go");
  display.setCursor(5, 50); if(secim==4) display.print(">"); else display.print(" "); display.print("IPTAL");
  display.display();
  if (digitalRead(BTN_PIN) == LOW) {
    playEnter(); delay(200); display.clearDisplay(); display.setCursor(30, 30); display.print("CALINIYOR..."); display.display();
    if (secim == 1) calShapeOfYou(); else if (secim == 2) calStillDre(); else if (secim == 3) calWayDownWeGo();
    muzikMenuAktif = false; sonCevap = ""; while(digitalRead(BTN_PIN) == LOW); 
  }
}

void createWavHeader(uint8_t* header, int waveDataSize){
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  unsigned int fileSize = waveDataSize + WAV_HEADER_SIZE - 8;
  header[4] = (byte)(fileSize & 0xFF); header[5] = (byte)((fileSize >> 8) & 0xFF); header[6] = (byte)((fileSize >> 16) & 0xFF); header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 0x10; header[17] = 0x00; header[18] = 0x00; header[19] = 0x00;
  header[20] = 0x01; header[21] = 0x00; header[22] = 0x01; header[23] = 0x00; 
  header[24] = 0x80; header[25] = 0x3E; header[26] = 0x00; header[27] = 0x00; 
  header[28] = 0x00; header[29] = 0x7D; header[30] = 0x00; header[31] = 0x00; 
  header[32] = 0x02; header[33] = 0x00; header[34] = 0x10; header[35] = 0x00; 
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  header[40] = (byte)(waveDataSize & 0xFF); header[41] = (byte)((waveDataSize >> 8) & 0xFF); header[42] = (byte)((waveDataSize >> 16) & 0xFF); header[43] = (byte)((waveDataSize >> 24) & 0xFF);
}

void cizMiniGozler(int x, int y) {
  if (millis() - sonGozKirpma > 3000 + random(2000)) {
    gozlerKapali = true;
    if (millis() - sonGozKirpma > 3150 + random(2000)) { sonGozKirpma = millis(); gozlerKapali = false; }
  }
  if(!gozlerKapali) {
    display.fillCircle(x - 12, y, 6, SSD1306_WHITE); display.fillCircle(x - 12, y, 2, SSD1306_BLACK);
    display.fillCircle(x + 12, y, 6, SSD1306_WHITE); display.fillCircle(x + 12, y, 2, SSD1306_BLACK);
  } else {
    display.drawFastHLine(x - 18, y, 12, SSD1306_WHITE); display.drawFastHLine(x + 6, y, 12, SSD1306_WHITE);
  }
}

void cizMiniArayuz(String mesaj, int scrollOffset = 0, bool daktiloModu = false) {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); 
  int startY = 26 - scrollOffset; display.setCursor(0, startY);
  if(daktiloModu) { display.print(sonCevap.substring(0, currentLen)); } else { display.print(mesaj); }
  display.fillRect(0, 0, 128, 24, SSD1306_BLACK); cizMiniGozler(64, 10); 
  display.drawFastHLine(0, 22, 128, SSD1306_WHITE); display.display();
}

String askGPT(String userText) {
  if(WiFi.status() != WL_CONNECTED) return "Wifi Yok";
  HTTPClient http; WiFiClientSecure client; client.setInsecure(); client.setTimeout(30000); 

  if (http.begin(client, "https://api.openai.com/v1/chat/completions")) {
    http.addHeader("Content-Type", "application/json"); http.addHeader("Authorization", String("Bearer ") + apiKey);
    String payload = "{ \"model\": \"gpt-4o-mini\", \"messages\": [ {\"role\": \"system\", \"content\": \"Adin Mike. Kullanicinin asistanisin. Kisa cevap ver. Eger kullanici sarki cal derse SADECE [M] yaz.\"},";
    if (sohbetGecmisi.length() > 5) { payload += sohbetGecmisi; }
    payload += "{\"role\": \"user\", \"content\": \"" + userText + "\"} ], \"max_tokens\": 150 }";
    int httpCode = http.POST(payload);
    if(httpCode == 200) {
      String response = http.getString(); DynamicJsonDocument doc(2048); deserializeJson(doc, response); http.end();
      String assistantReply = trDuzelt(doc["choices"][0]["message"]["content"].as<String>());
      if (assistantReply.indexOf("[M]") >= 0) { muzikMenuAktif = true; assistantReply = ""; } else { muzikMenuAktif = false; }
      if (assistantReply.length() > 0) {
          String yeniHafiza = "{\"role\":\"user\",\"content\":\"" + userText + "\"},{\"role\":\"assistant\",\"content\":\"" + assistantReply + "\"},";
          sohbetGecmisi += yeniHafiza; if (sohbetGecmisi.length() > 1500) { sohbetGecmisi = ""; }
      }
      return assistantReply;
    } else { http.end(); return "GPT Hatasi"; }
  } return "Baglanti Hata";
}

String recordAndTranscribe() {
  int bufferSize = SAMPLE_RATE * 2 * MAX_RECORD_SECONDS; uint8_t* audioBuffer = (uint8_t*)malloc(bufferSize);
  if(audioBuffer == NULL) return "Hafiza Dolu!";
  size_t bytesRead = 0; size_t bytesRecorded = 0;
  i2s_read(I2S_PORT, audioBuffer, 1024, &bytesRead, 10); 
  playRecStart(); unsigned long startTime = millis();
  while(digitalRead(BTN_PIN) == LOW && (millis() - startTime < (MAX_RECORD_SECONDS * 1000))) {
      size_t chunk = 0; i2s_read(I2S_PORT, audioBuffer + bytesRecorded, 1024, &chunk, portMAX_DELAY);
      bytesRecorded += chunk; if(bytesRecorded >= bufferSize) break; cizMiniArayuz("Dinliyorum...", 0, false);
  }
  cizMiniArayuz("Isliyorum...", 0, false);
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(40000); 
  if (client.connect("api.openai.com", 443)) {
    String boundary = "ESP32Boundary";
    String header = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    String footer = "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n--" + boundary + "--\r\n";
    int totalLen = header.length() + WAV_HEADER_SIZE + bytesRecorded + footer.length();
    client.println("POST /v1/audio/transcriptions HTTP/1.1"); client.println("Host: api.openai.com");
    client.println("Authorization: Bearer " + String(apiKey)); client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(totalLen)); client.println(); client.print(header);
    uint8_t wavHeader[44]; createWavHeader(wavHeader, bytesRecorded); client.write(wavHeader, 44);
    int chunkSize = 1024; for (int i = 0; i < bytesRecorded; i += chunkSize) { int remain = bytesRecorded - i; if (remain > chunkSize) remain = chunkSize; client.write(audioBuffer + i, remain); }
    client.print(footer); free(audioBuffer); 
    String response = ""; while (client.connected()) { String line = client.readStringUntil('\n'); if (line == "\r") break; }
    String jsonResponse = client.readString(); DynamicJsonDocument doc(1024); deserializeJson(doc, jsonResponse);
    if (doc.containsKey("error")) return "API Hatasi";
    String text = doc["text"].as<String>(); text.replace("\"", ""); text.replace("\n", " ");
    if(text.length() > 0) return trDuzelt(text); else return "Anlasilmadi";
  } else { free(audioBuffer); return "Baglanti Yok"; }
}

String getGunIsmi(int offset) {
  struct tm timeinfo; if(!getLocalTime(&timeinfo)) return "Gun" + String(offset);
  int bugun = timeinfo.tm_wday; int hedefGun = (bugun + offset) % 7;
  switch(hedefGun) {
    case 0: return "PZR"; case 1: return "PTS"; case 2: return "SAL"; case 3: return "CRS"; case 4: return "PRS"; case 5: return "CUM"; case 6: return "CTS"; default: return "HATA";
  }
}
String wmoKodunuCevir(int code) {
  if(code == 0) return "GUNESLI"; if(code >= 1 && code <= 3) return "BULUTLU"; if(code == 45 || code == 48) return "SISLI";
  if(code >= 51 && code <= 67) return "YAGMURLU"; if(code >= 71 && code <= 77) return "KARLI"; if(code >= 95) return "FIRTINA"; return "BILINMIYOR";
}
void havaDurumuGuncelle() {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http; String url = "http://api.open-meteo.com/v1/forecast?latitude=" + latitude + "&longitude=" + longitude + "&daily=weathercode,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=5";
    http.begin(url); int httpCode = http.GET();
    if(httpCode == 200) { 
      String payload = http.getString(); DynamicJsonDocument doc(4096); deserializeJson(doc, payload);
      for(int i=0; i<5; i++) {
          havaKodlari[i] = doc["daily"]["weathercode"][i]; maxSicaklik[i] = doc["daily"]["temperature_2m_max"][i]; minSicaklik[i] = doc["daily"]["temperature_2m_min"][i];
          if(i==0) gunIsimleri[i] = "BUGUN"; else if(i==1) gunIsimleri[i] = "YARIN"; else gunIsimleri[i] = getGunIsmi(i);
      } havaVerisiVar = true;
    } http.end();
  }
}

void wifiBaslat() {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20); display.print("Wi-Fi: "); display.print(ssid); display.display();
  WiFi.begin(ssid, password); int t = 0;
  while(WiFi.status() != WL_CONNECTED && t < 20) { delay(500); t++; display.setCursor(10+t*5, 40); display.print("."); display.display(); }
  if(WiFi.status() == WL_CONNECTED) {
    display.clearDisplay(); display.setCursor(30, 30); display.print("BAGLANDI!"); display.display();
    playEnter(); configTime(10800, 0, "pool.ntp.org"); delay(100); havaDurumuGuncelle(); 
  } else { display.clearDisplay(); display.setCursor(20, 30); display.print("HATA"); display.display(); delay(2000); }
}

bool ciftTiklamaKontrol() {
  unsigned long baslamaZamani = millis(); while(digitalRead(BTN_PIN) == LOW); delay(50); 
  while(millis() - baslamaZamani < 400) { if(digitalRead(BTN_PIN) == LOW) { playClick(); while(digitalRead(BTN_PIN) == LOW); return true; } }
  return false; 
}

// --- HAVA DURUMU ANİMASYONLARI ---
void cizHavaAnimasyon(int x, int y, int code) {
  int frame = (millis() / 200) % 4; 
  if (code == 0) { 
    display.fillCircle(x, y, 6, SSD1306_WHITE); 
    if (frame % 2 == 0) { display.drawFastHLine(x-10, y, 20, SSD1306_WHITE); display.drawFastVLine(x, y-10, 20, SSD1306_WHITE); } 
    else { display.drawLine(x-7, y-7, x+7, y+7, SSD1306_WHITE); display.drawLine(x-7, y+7, x+7, y-7, SSD1306_WHITE); } 
  }
  else if ((code >= 1 && code <= 3) || code == 45 || code == 48) { 
    int offset = (frame == 0 || frame == 2) ? 0 : (frame == 1 ? 1 : -1); 
    display.fillCircle(x+offset+4, y, 6, SSD1306_WHITE); display.fillCircle(x+offset-4, y, 6, SSD1306_WHITE); display.fillCircle(x+offset, y-4, 6, SSD1306_WHITE); 
  }
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) { 
    display.fillCircle(x+4, y-4, 6, SSD1306_WHITE); display.fillCircle(x-4, y-4, 6, SSD1306_WHITE); display.fillCircle(x, y-8, 6, SSD1306_WHITE); 
    int dropY = frame * 3; 
    display.drawLine(x-4, y+dropY, x-4, y+dropY+3, SSD1306_WHITE); display.drawLine(x+4, y+dropY, x+4, y+dropY+3, SSD1306_WHITE); 
  }
  else if (code >= 71 && code <= 77) { 
    display.fillCircle(x+4, y-4, 6, SSD1306_WHITE); display.fillCircle(x-4, y-4, 6, SSD1306_WHITE); display.fillCircle(x, y-8, 6, SSD1306_WHITE); 
    int snowY = frame * 2; 
    display.drawPixel(x-5, y+snowY, SSD1306_WHITE); display.drawPixel(x+5, y+snowY+2, SSD1306_WHITE); display.drawPixel(x, y+snowY+1, SSD1306_WHITE); 
  }
  else { display.drawRect(x-5, y-5, 10, 10, SSD1306_WHITE); }
}

// --- İKONLAR ---
void drawIconFace(int x, int y) { display.drawCircle(x, y, 16, SSD1306_WHITE); display.fillCircle(x-6, y-4, 2, SSD1306_WHITE); display.fillCircle(x+6, y-4, 2, SSD1306_WHITE); display.drawRoundRect(x-6, y+4, 12, 4, 2, SSD1306_WHITE); }
void drawIconAtmosphere(int x, int y) { display.drawRect(x-6, y-8, 4, 12, SSD1306_WHITE); display.fillCircle(x-4, y+6, 4, SSD1306_WHITE); display.drawCircle(x+6, y, 6, SSD1306_WHITE); }
void drawIconSunCloud(int x, int y) { display.drawCircle(x+6, y-6, 4, SSD1306_WHITE); display.fillCircle(x-4, y, 6, SSD1306_WHITE); display.fillCircle(x+4, y, 5, SSD1306_WHITE); }
void drawIconBulb(int x, int y) { display.drawCircle(x, y-4, 10, SSD1306_WHITE); display.fillRect(x-4, y+6, 8, 6, SSD1306_WHITE); }
void drawIconClock(int x, int y) { display.drawCircle(x, y, 14, SSD1306_WHITE); display.drawLine(x, y, x, y-10, SSD1306_WHITE); display.drawLine(x, y, x+6, y, SSD1306_WHITE); }
void drawIconCalendar(int x, int y) { display.drawRect(x-10, y-10, 20, 20, SSD1306_WHITE); display.drawFastHLine(x-10, y-4, 20, SSD1306_WHITE); display.setCursor(x-3, y); display.setTextSize(1); display.print("10"); }
void drawIconPomodoro(int x, int y) { display.fillCircle(x, y, 14, SSD1306_WHITE); display.fillCircle(x, y, 10, SSD1306_BLACK); display.drawFastVLine(x, y-14, 6, SSD1306_WHITE); display.fillTriangle(x, y-10, x+4, y-8, x, y-6, SSD1306_WHITE); }

void printCenteredText(String text, int y, int size) {
  int16_t x1, y1; uint16_t w, h; display.setTextSize(size); display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2; display.setCursor(x, y); display.print(text);
}

// --- ANA MENÜ ---
void cizMenu() {
  display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
  if(seciliMenu > 1) { display.setCursor(4, 25); display.print("<"); }
  if(seciliMenu < toplamMenu) { display.setCursor(114, 25); display.print(">"); }
  int cx = 64; int cy = 28; display.drawCircle(cx, cy, 18, SSD1306_WHITE); 
  
  if(seciliMenu == 1)      { drawIconFace(cx, cy);      printCenteredText("MIKE", 52, 1); } 
  else if(seciliMenu == 2) { drawIconAtmosphere(cx, cy); printCenteredText("ODA", 52, 1); } 
  else if(seciliMenu == 3) { drawIconSunCloud(cx, cy);   printCenteredText("SEHIR", 52, 1); } 
  else if(seciliMenu == 4) { drawIconBulb(cx, cy);       printCenteredText("LED", 52, 1); }
  else if(seciliMenu == 5) { drawIconClock(cx, cy);      printCenteredText("SAAT", 52, 1); }
  else if(seciliMenu == 6) { drawIconCalendar(cx, cy);   printCenteredText("TAKVIM", 52, 1); }
  else if(seciliMenu == 7) { drawIconPomodoro(cx, cy);   printCenteredText("FOCUS", 52, 1); }

  int barWidth = 100; int barX = (128 - barWidth) / 2; int barY = 62;
  display.drawFastHLine(barX, barY, barWidth, SSD1306_WHITE);
  int segmentWidth = barWidth / toplamMenu; int activeX = barX + ((seciliMenu - 1) * segmentWidth);
  display.fillRect(activeX, barY - 1, segmentWidth, 3, SSD1306_WHITE);
}

void cizAgizYeni(int duygu, int sesPuan) {
  int agizY = 42; 
  if (sesPuan > 15 || duygu == 1) { 
    int agizGenislik = map(sesPuan, 0, 100, 16, 24); int agizYukseklik = map(sesPuan, 0, 100, 4, 10); 
    display.fillRoundRect(64 - agizGenislik/2, agizY, agizGenislik, agizYukseklik, 4, SSD1306_WHITE);
    display.fillRoundRect(64 - agizGenislik/2 + 2, agizY-2, agizGenislik-4, agizYukseklik, 4, SSD1306_BLACK);
  } else { display.fillRoundRect(58, agizY+4, 12, 3, 1, SSD1306_WHITE); }
}

void cizGozlerYeni(int kirpTip, int xOfset, int yOfset, int duygu) {
  display.fillCircle(38, 25, 16, SSD1306_WHITE); display.fillCircle(38 + xOfset, 25 + yOfset, 7, SSD1306_BLACK); 
  display.fillCircle(90, 25, 16, SSD1306_WHITE); display.fillCircle(90 + xOfset, 25 + yOfset, 7, SSD1306_BLACK);
  if(duygu != 2) { display.fillCircle(40+xOfset,23+yOfset,2,SSD1306_WHITE); display.fillCircle(92+xOfset,23+yOfset,2,SSD1306_WHITE); }
  if (kirpTip == 1) { display.fillRect(24, 22, 28, 6, SSD1306_WHITE); display.fillRect(76, 22, 28, 6, SSD1306_WHITE); } 
}

// --- UYGULAMALAR ---

void appAsistanFull() {
  int32_t sample = 0; size_t bytes = 0; i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes, 0); 
  int32_t raw = abs(sample >> 14); int barH = map(raw, 0, 8000, 0, 40); if(barH>40) barH=40;
  if(sonCevap == "") {
    cizGozlerYeni(0, 0, 0, 0); cizAgizYeni(0, 0); printCenteredText("BAS KONUS", 55, 1);
    display.drawRect(120, 10, 6, 42, SSD1306_WHITE); if(barH > 1) display.fillRect(122, 50-barH, 2, barH, SSD1306_WHITE);
  } else {
    int potVal = analogRead(POT_PIN); if (abs(potVal - lastStabilPot) > 50) { lastStabilPot = potVal; }
    int lineCharCount = 21; int totalLines = (sonCevap.length() / lineCharCount) + 3; int totalHeight = totalLines * 10; 
    int maxScroll = totalHeight - 35; if (maxScroll < 0) maxScroll = 0;
    int scrollOffset = map(lastStabilPot, 0, 4096, 0, maxScroll + 10); 
    if (currentLen < sonCevap.length()) { if (millis() - lastCharTime > 60) { currentLen++; lastCharTime = millis(); if (random(0, 10) > 6) { playTypeSound(); } } }
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    int startY = 26 - scrollOffset; display.setCursor(0, startY); display.print(sonCevap.substring(0, currentLen));
    display.fillRect(0, 0, 128, 24, SSD1306_BLACK); cizMiniGozler(64, 10); display.drawFastHLine(0, 22, 128, SSD1306_WHITE);
  }
}

void mikeDinlemeIslemi() {
    currentLen = 0; sonCevap = ""; muzikMenuAktif = false;
    String duyulanMetin = recordAndTranscribe(); cizMiniArayuz("Dusunuyor...", 0, false);
    if(duyulanMetin != "Anlasilmadi" && duyulanMetin != "Baglanti Yok" && duyulanMetin != "API Hatasi" && duyulanMetin != "Hafiza Dolu!") { sonCevap = askGPT(duyulanMetin); } else { sonCevap = duyulanMetin; currentLen = sonCevap.length(); }
    playResponse(); while(digitalRead(BTN_PIN) == LOW); 
}

void appOdaHava() {
  sensors_event_t humidity, temp; sht4.getEvent(&humidity, &temp); float press = BaroSensor.getPressure();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawCircle(20, 45, 6, SSD1306_WHITE); display.drawRect(17, 20, 6, 20, SSD1306_WHITE); display.fillCircle(20, 45, 3, SSD1306_WHITE); display.fillRect(19, 30, 2, 15, SSD1306_WHITE); 
  display.setTextSize(3); display.setCursor(35, 25); display.print((int)temp.temperature); display.setTextSize(1); display.setCursor(72, 25); display.print("o"); 
  display.drawRect(90, 15, 10, 40, SSD1306_WHITE); int barHeight = map((int)humidity.relative_humidity, 0, 100, 0, 36); display.fillRect(92, 53 - barHeight, 6, barHeight, SSD1306_WHITE); 
  display.setCursor(90, 5); display.print("%"); display.print((int)humidity.relative_humidity); display.setCursor(35, 50); display.print("hPa:"); display.print((int)press);
}

void appSehirHava() {
  int potVal = analogRead(POT_PIN); int secilenGun = map(potVal, 0, 4096, 0, 5); if(secilenGun > 4) secilenGun = 4;
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE); display.setCursor(5, 5); display.print(city); display.setCursor(80, 5); display.print(gunIsimleri[secilenGun]); display.drawLine(0, 15, 128, 15, SSD1306_WHITE);
  if(havaVerisiVar) {
    cizHavaAnimasyon(25, 40, havaKodlari[secilenGun]); 
    display.setTextSize(2); display.setCursor(50, 22); display.print((int)maxSicaklik[secilenGun]); display.setTextSize(1); display.print("/"); display.print((int)minSicaklik[secilenGun]); display.print("C");
    display.setCursor(50, 42); display.print(wmoKodunuCevir(havaKodlari[secilenGun])); 
  } else { display.setCursor(20, 30); display.print("YUKLENIYOR..."); }
}

void appLed() {
  String renkAdi = "KAPALI"; uint32_t secilenRenk = pixels.Color(0,0,0);
  switch(rgbModu) { 
    case 0: secilenRenk = pixels.Color(0, 0, 0); renkAdi="KAPALI"; break; case 1: secilenRenk = pixels.Color(255, 0, 0); renkAdi="KIRMIZI"; break; case 2: secilenRenk = pixels.Color(0, 255, 0); renkAdi="YESIL"; break; 
    case 3: secilenRenk = pixels.Color(0, 0, 255); renkAdi="MAVI"; break; case 4: secilenRenk = pixels.Color(255, 255, 0); renkAdi="SARI"; break; case 5: secilenRenk = pixels.Color(255, 0, 255); renkAdi="MOR"; break; 
    case 6: secilenRenk = pixels.Color(0, 255, 255); renkAdi="TURKUAZ"; break; case 7: secilenRenk = pixels.Color(255, 255, 255); renkAdi="BEYAZ"; break; 
    case 8: if (millis() - lastDiskoTime > 100) { secilenRenk = pixels.Color(random(255), random(255), random(255)); lastDiskoTime = millis(); for(int i=0; i<NUMPIXELS; i++) { pixels.setPixelColor(i, secilenRenk); } pixels.show(); } renkAdi="DISKO"; break; 
  }
  if (rgbModu != 8) { for(int i=0; i<NUMPIXELS; i++) { pixels.setPixelColor(i, secilenRenk); } pixels.show(); }
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE); printCenteredText("LED AYARI", 4, 1); display.drawLine(0, 14, 128, 14, SSD1306_WHITE);
  int cx = 64; int cy = 32; display.drawCircle(cx, cy, 10, SSD1306_WHITE); display.fillRect(cx-4, cy+10, 8, 4, SSD1306_WHITE); 
  if(rgbModu != 0) { display.drawLine(cx-14, cy, cx-18, cy, SSD1306_WHITE); display.drawLine(cx+14, cy, cx+18, cy, SSD1306_WHITE); display.drawLine(cx, cy-14, cx, cy-18, SSD1306_WHITE); }
  printCenteredText(renkAdi, 50, 1);
}

float degToRad(float deg) { return deg * PI / 180.0; }

// --- SAAT (MODERN DIJITAL + ANALOG SECENEKLI) ---
void appSaat() {
  struct tm t; if(!getLocalTime(&t)) { display.setCursor(10,30); display.print("SAAT BEKLENIYOR"); return; }
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);

  if (saatModuDijital) {
      // --- DİJİTAL GÖRÜNÜM ---
      display.drawRoundRect(8, 10, 50, 40, 4, SSD1306_WHITE); 
      display.drawRoundRect(70, 10, 50, 40, 4, SSD1306_WHITE); 
      display.setTextSize(3); 
      display.setCursor(12, 20); if(t.tm_hour < 10) display.print("0"); display.print(t.tm_hour);
      display.setCursor(74, 20); if(t.tm_min < 10) display.print("0"); display.print(t.tm_min);
      if (t.tm_sec % 2 == 0) { display.fillCircle(64, 25, 2, SSD1306_WHITE); display.fillCircle(64, 35, 2, SSD1306_WHITE); }
      int secWidth = map(t.tm_sec, 0, 60, 0, 128); display.fillRect(0, 60, secWidth, 4, SSD1306_WHITE);
  } else {
      // --- ANALOG GÖRÜNÜM ---
      int cx = 64; int cy = 32; int r = 30;
      display.drawCircle(cx, cy, r, SSD1306_WHITE); display.fillCircle(cx, cy, 2, SSD1306_WHITE);
      for(int i=0; i<12; i++) { float angle = degToRad(i * 30); display.drawPixel(cx + (r-2)*cos(angle), cy + (r-2)*sin(angle), SSD1306_WHITE); }
      float s_angle = degToRad((t.tm_sec * 6) - 90); display.drawLine(cx, cy, cx + (r-2)*cos(s_angle), cy + (r-2)*sin(s_angle), SSD1306_WHITE);
      float m_angle = degToRad((t.tm_min * 6) - 90); display.drawLine(cx, cy, cx + (r-5)*cos(m_angle), cy + (r-5)*sin(m_angle), SSD1306_WHITE);
      float h_angle = degToRad(((t.tm_hour % 12) * 30 + t.tm_min * 0.5) - 90); display.drawLine(cx, cy, cx + (r-12)*cos(h_angle), cy + (r-12)*sin(h_angle), SSD1306_WHITE);
  }
}

void appTakvim() {
  struct tm t; if(!getLocalTime(&t)) return;
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  String header = String(aylarTR[t.tm_mon]) + " " + String(t.tm_year + 1900);
  printCenteredText(header, 0, 1); display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  for(int i=0; i<7; i++) { display.setCursor(4 + (i*18), 14); display.print(gunlerKisa[i]); }
  int currentDay = t.tm_mday; int currentWeekDay = t.tm_wday; 
  int firstDayOfWeek = (currentWeekDay - (currentDay - 1) % 7 + 7) % 7; 
  int daysInMonth = 31; if(t.tm_mon == 1) daysInMonth = 28; else if(t.tm_mon == 3 || t.tm_mon == 5 || t.tm_mon == 8 || t.tm_mon == 10) daysInMonth = 30;
  int x = 0; int y = 24;
  for(int d = 1; d <= daysInMonth; d++) {
      int wDay = (firstDayOfWeek + d - 1) % 7; int xPos = 4 + (wDay * 18);
      if(d == currentDay) { display.fillRect(xPos-2, y-1, 14, 9, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); } 
      else { display.setTextColor(SSD1306_WHITE); }
      display.setCursor(xPos, y); display.print(d); if(wDay == 6) y += 10; 
  }
}

void appPomodoro() {
  // Eğer Edit Modundaysak
  if(pomoEditMode) {
     int potVal = analogRead(POT_PIN);
     pomoSecilenIndex = map(potVal, 0, 4096, 0, 5); // 5 seçenek var
     if(pomoSecilenIndex > 4) pomoSecilenIndex = 4;
     
     display.clearDisplay();
     display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
     printCenteredText("SURE AYARLA", 10, 1);
     
     display.setTextSize(3); 
     display.setCursor(40, 30);
     int val = pomoPresets[pomoSecilenIndex];
     if(val < 10) display.print("0"); 
     display.print(val);
     display.setTextSize(1); display.print(" dk");
     
     printCenteredText("Tusa Bas -> Kaydet", 55, 1);
     return; // Fonksiyondan çık, alttakileri çizme
  }

  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  if (pomoCalisiyor) {
    unsigned long anlikGecen = millis() - pomoBaslangic;
    unsigned long toplamGecenSanal = anlikGecen + pomoToplamGecen;
    unsigned long kalanSure = 0;
    if (pomoSure > toplamGecenSanal) kalanSure = pomoSure - toplamGecenSanal;
    else { pomoCalisiyor = false; pomoBitti = true; pomoDuraklatildi = false; pomoToplamGecen = 0; playAlarm(); }
    int dak = (kalanSure / 1000) / 60; int san = (kalanSure / 1000) % 60;
    display.setTextSize(3); display.setCursor(20, 20);
    if(dak < 10) display.print("0"); display.print(dak); display.print(":");
    if(san < 10) display.print("0"); display.print(san);
    printCenteredText("CALISIYOR...", 50, 1);
  } else if (pomoDuraklatildi) {
     unsigned long kalanSure = pomoSure - pomoToplamGecen;
     int dak = (kalanSure / 1000) / 60; int san = (kalanSure / 1000) % 60;
     display.setTextSize(3); display.setCursor(20, 20);
     if(dak < 10) display.print("0"); display.print(dak); display.print(":");
     if(san < 10) display.print("0"); display.print(san);
     printCenteredText("DURAKLATILDI", 50, 1);
  } else {
    if (pomoBitti) { printCenteredText("SURE BITTI!", 20, 2); printCenteredText("Cift Tik -> Cikis", 45, 1); } 
    else { 
       // Bekleme ekranı, seçilen süreyi göster
       int setDak = pomoSure / 1000 / 60;
       display.setTextSize(3); display.setCursor(20, 20); 
       if(setDak < 10) display.print("0"); display.print(setDak); display.print(":00");
       printCenteredText("BASLAT", 50, 1); 
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(POT_PIN, INPUT); pinMode(BTN_PIN, INPUT_PULLUP); 
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); ledcAttachPin(SPEAKER_PIN, PWM_CHANNEL);
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL); i2s_set_pin(I2S_PORT, &pin_config);
  pixels.begin(); pixels.clear(); pixels.show();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);
  BaroSensor.begin(0x76); sht4.begin();
  wifiBaslat(); 
}

void loop() {
  if(millis() - sonHavaGuncelleme > 1800000) { havaDurumuGuncelle(); sonHavaGuncelleme = millis(); }
  int potDeger = analogRead(POT_PIN);

  if (aktifUygulama == 0) {
    int hamSecim = map(potDeger, 0, 4096, 1, toplamMenu + 1);
    if(hamSecim > toplamMenu) hamSecim = toplamMenu; if(hamSecim < 1) hamSecim = 1;
    seciliMenu = hamSecim;
  } 
  else if (aktifUygulama == 4) { 
    int hamRenk = map(potDeger, 0, 4096, 0, 9); if(hamRenk > 8) hamRenk = 8; rgbModu = hamRenk;
  }

  // --- TUŞ KONTROLÜ ---
  if (digitalRead(BTN_PIN) == LOW) {
    // 1. Ana Menüdeyken
    if (aktifUygulama == 0) { aktifUygulama = seciliMenu; playEnter(); delay(300); } 
    
    // 2. Mike (Asistan)
    else if (aktifUygulama == 1) {
          unsigned long baslama = millis(); while(digitalRead(BTN_PIN) == LOW && (millis() - baslama < 300));
          if (digitalRead(BTN_PIN) == HIGH) {
             if (!muzikMenuAktif) { if (sonCevap == "") { aktifUygulama = 0; } else { sonCevap = ""; currentLen = 0; } playClick(); }
          } else { if (!muzikMenuAktif) { mikeDinlemeIslemi(); } }
    }
    
    // 3. SAAT (TEK TIK: DEGISTIR, CIFT TIK: CIK)
    else if (aktifUygulama == 5) {
          if (ciftTiklamaKontrol()) { aktifUygulama = 0; playClick(); } // Çift Tık -> Çıkış
          else { saatModuDijital = !saatModuDijital; playClick(); } // Tek Tık -> Mod Değiştir
    }

    // 4. POMODORO KONTROLU (OZEL MANTIK)
    else if (aktifUygulama == 7) { 
         // Önce çift tıklama var mı kontrol et (ÇIKIŞ İÇİN)
         bool ciftTik = false;
         unsigned long basma = millis();
         while(digitalRead(BTN_PIN) == LOW); // İlk bırakma
         unsigned long sure = millis() - basma;
         
         if (sure < 500) { // Kısa basıldıysa ikinciyi bekle
             unsigned long bekleme = millis();
             while(millis() - bekleme < 250) {
                 if(digitalRead(BTN_PIN) == LOW) { ciftTik = true; while(digitalRead(BTN_PIN) == LOW); break; }
             }
         }

         if (ciftTik) {
             // ÇİFT TIK -> ÇIKIŞ
             if (pomoEditMode) pomoEditMode = false; // Edit modundaysak önce ondan çık
             else { aktifUygulama = 0; playClick(); }
         } 
         else {
             if (sure > 3000) {
                 // ÇOK UZUN BASIŞ (3sn) -> EDIT MODU AC/KAPA
                 pomoEditMode = !pomoEditMode;
                 playEnter();
             }
             else if (sure > 800) { 
                 // UZUN BASIŞ (1sn) -> SIFIRLA (Edit modunda değilsek)
                 if(!pomoEditMode) {
                   pomoCalisiyor = false; pomoDuraklatildi = false; pomoBitti = false; pomoToplamGecen = 0; playAlarm(); 
                 }
             } else {
                 // TEK BASIŞ -> AKSİYON
                 if (pomoEditMode) {
                     // Edit modundayız -> KAYDET VE ÇIK
                     pomoSure = pomoPresets[pomoSecilenIndex] * 60 * 1000;
                     pomoEditMode = false;
                     // Sayacı da sıfırla ki yeni süre gelsin
                     pomoCalisiyor = false; pomoDuraklatildi = false; pomoBitti = false; pomoToplamGecen = 0;
                     playEnter();
                 } else {
                     // Normal moddayız -> BAŞLAT / DURAKLAT
                     if (pomoBitti) { pomoBitti = false; } 
                     else if (pomoCalisiyor) { pomoCalisiyor = false; pomoDuraklatildi = true; pomoToplamGecen += (millis() - pomoBaslangic); } 
                     else { pomoCalisiyor = true; pomoDuraklatildi = false; pomoBaslangic = millis(); }
                     playClick();
                 }
             }
         }
    }
    
    // 5. Diğer Menülerden Çıkış (Çift Tıklama Standart)
    else { if(ciftTiklamaKontrol()) { aktifUygulama = 0; playClick(); } }
  }

  display.clearDisplay();
  if (muzikMenuAktif) { appMuzikMenu(); } 
  else {
      if (aktifUygulama == 0)      cizMenu(); 
      else if (aktifUygulama == 1) appAsistanFull(); 
      else if (aktifUygulama == 2) appOdaHava(); 
      else if (aktifUygulama == 3) appSehirHava(); 
      else if (aktifUygulama == 4) appLed(); 
      else if (aktifUygulama == 5) appSaat();   
      else if (aktifUygulama == 6) appTakvim(); 
      else if (aktifUygulama == 7) appPomodoro(); 
  }
  display.display();
}