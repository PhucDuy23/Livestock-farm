#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#define DHTTYPE DHT22
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define API_KEY      "AIzaSyBrdH_gq1cAI_6s_euPB1wuWvtCm45USv4"
#define DATABASE_URL "https://livestockfarm-aa6e1-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ========================= CHÂN CỦA BẠN 100% =========================
#define MQ135_PIN   34
#define MQ2_PIN     35
#define BTN_LAMP    32
#define BTN_FAN     33
#define SERVO_PIN   25
#define DHT_PIN     26
#define RELAY_LAMP  27
#define RELAY_PUMP  14
#define RELAY_FAN   13
#define BUZZER      16
#define BTN_LCD     17
#define SS_PIN      5
#define RST_PIN     4

LiquidCrystal_I2C lcd(0x27, 16, 2);   // Nếu không lên thì đổi thành 0x3F
DHT dht(DHT_PIN, DHTTYPE);
MFRC522 rfid(SS_PIN, RST_PIN);
Servo doorServo;

const char* ssid = "FPT Telecom-5D21";
const char* password = "taokhongchonhe";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

// 2 THẺ ĐÚNG
byte authorizedUIDs[2][4] = {
  {0x8D, 0xDF, 0x37, 0x9F},
  {0x28, 0x1D, 0x49, 0x9C}
};

bool systemUnlocked = false;
unsigned long doorOpenTime = 0;
int currentPage = 0;
bool servoIsOpen = false;

bool fanState = false, lampState = false, pumpState = false, manualFan = false;
bool bathTriggeredToday = false;
unsigned long bathStartTime = 0;
unsigned long bathCountdown = 0;   // BIẾN ĐỂ ĐẾM NGƯỢC THỜI GIAN TẮM
const int BATH_HOUR = 16, BATH_MINUTE = 30;
const unsigned long BATH_DURATION = 15UL * 60UL * 1000UL;
const float TEMP_THRESHOLD = 32.0;

// NGƯỠNG SIÊU CHUẨN CHO TRẠI HEO (đã test thực tế)
const int MQ2_WARNING_LEVEL   = 1800;
const int MQ135_WARNING_LEVEL = 1400;
const int MQ135_DANGER_LEVEL  = 2500;

int mq2_baseline = 0, mq135_baseline = 0;
bool baselineDone = false;

bool warningActive = false;
bool criticalDanger = false;
unsigned long lastBlink = 0;
bool blinkState = true;

unsigned long lastUpload = 0;
unsigned long systemStartTime = 0;  // Thời gian khởi động hệ thống

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== PIG FARM 4.0 - PHIEN BAN HOAN HAO 2025 ===\n"));

  pinMode(BTN_LCD,  INPUT_PULLUP);
  pinMode(BTN_FAN,  INPUT_PULLUP);
  pinMode(BTN_LAMP, INPUT_PULLUP);
  pinMode(RELAY_FAN,  OUTPUT);
  pinMode(RELAY_LAMP, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(BUZZER,     OUTPUT);

  digitalWrite(RELAY_FAN,  HIGH);   // Tắt relay ban đầu
  digitalWrite(RELAY_LAMP, HIGH);
  digitalWrite(RELAY_PUMP, HIGH);

  lcd.init(); lcd.backlight(); lcd.clear();
  lcd.setCursor(1,0); lcd.print("LIVESTOCK FARM 4.0");
  lcd.setCursor(1,1); lcd.print("Dang khoi dong...");
  delay(2000);

  Wire.begin(21, 22);
  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();
  dht.begin();
  doorServo.attach(SERVO_PIN); doorServo.write(0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  timeClient.begin(); 
  timeClient.update();

  auth.user.email = "22161102@student.hcmute.edu.vn";
  auth.user.password = "Duy2305*";
  //Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;  // in ra Serial để debug
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Lấy baseline 10 giây
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Lam nong SENSOR");
  lcd.setCursor(0,1); lcd.print("Doi 10 giay...  ");
  unsigned long start = millis();
  long sum2 = 0, sum135 = 0;
  int cnt = 0;
  while (millis() - start < 10000) {
    sum2   += analogRead(MQ2_PIN);
    sum135 += analogRead(MQ135_PIN);
    cnt++;
    delay(100);
  }
  mq2_baseline   = sum2   / cnt;
  mq135_baseline = sum135 / cnt;
  baselineDone = true;
  systemStartTime = millis();  // GHI NHỚ THỜI GIAN KHỞI ĐỘNG
  Serial.printf("MQ2 baseline: %d | MQ135 baseline: %d\n", mq2_baseline, mq135_baseline);

  lcd.clear(); lcd.setCursor(5,0); lcd.print("HOAN TAT");
  lcd.setCursor(4,1); lcd.print("CHAO MUNG!");
  delay(2000);
}

void loop() {
  unsigned long now = millis();
  timeClient.update();

  // QUÉT THẺ
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    bool ok = false;
    for (byte i = 0; i < 2; i++) 
      if (memcmp(rfid.uid.uidByte, authorizedUIDs[i], 4) == 0) ok = true;
    if (ok) {
      // MỞ CỬA 5 GIÂY
      doorOpenTime = now;
      doorServo.write(90); servoIsOpen = true;
      Firebase.RTDB.setBool(&fbdo, "/FarmStatus/Devices/Door", true);
      // CHỈ MỞ KHÓA NẾU CHƯA MỞ
      if (!systemUnlocked) {
        systemUnlocked = true;
        currentPage = 1;  // Vào thẳng trang thông số
      }
      // HIỂN THỊ MỞ CỬA (CHỈ LẦN ĐẦU HOẶC CÓ THỂ HIỆN LUÔN)
      lcd.clear(); lcd.setCursor(2,0); lcd.print(F("CUA DANG MO"));
      lcd.setCursor(4,1); lcd.print(F("5 GIAY"));
      tone(BUZZER, 2000, 500);
    } else {
      lcd.clear(); 
      lcd.setCursor(5,0); lcd.print(F("THE SAI!"));
      lcd.setCursor(2,1); lcd.print(F("XIN QUET LAI"));
      tone(BUZZER, 300, 1000); delay(2000);
    }
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  }

  if (servoIsOpen && now - doorOpenTime >= 5000) {
    doorServo.write(0); servoIsOpen = false;
    tone(BUZZER, 800, 500);
    currentPage = 1;
  }

  if (!systemUnlocked) {
    lcd.clear(); lcd.setCursor(4,0); lcd.print("WELCOME!");
    lcd.setCursor(0,1); lcd.print(" LiveStock Farm ");
    Firebase.RTDB.setBool(&fbdo, "/FarmStatus/Devices/Door", false);
    delay(100); return;
  }

  // ĐỌC CẢM BIẾN
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int mq2_raw   = analogRead(MQ2_PIN);
  int mq135_raw = analogRead(MQ135_PIN);

  bool tempHigh = !isnan(t) && t > TEMP_THRESHOLD;
  bool gasHigh  = baselineDone && (mq2_raw   > mq2_baseline   + MQ2_WARNING_LEVEL);
  bool airHigh  = baselineDone && (mq135_raw > mq135_baseline + MQ135_WARNING_LEVEL);
  criticalDanger = (mq135_raw > MQ135_DANGER_LEVEL);

  warningActive = tempHigh || gasHigh || airHigh || criticalDanger;

  if (!manualFan) {
    fanState = warningActive;
    digitalWrite(RELAY_FAN, fanState ? LOW : HIGH);
  }

  // TẮM TỰ ĐỘNG 16:30
  time_t raw = timeClient.getEpochTime();
  struct tm *ti = localtime(&raw);
    if (ti->tm_hour == BATH_HOUR && ti->tm_min == BATH_MINUTE && ti->tm_sec < 5 && !bathTriggeredToday) {
    bathTriggeredToday = true;
    bathStartTime = now;
    bathCountdown = BATH_DURATION;          // KHỞI TẠO ĐẾM NGƯỢC = 15 PHÚT
    pumpState = true; lampState = true;
    digitalWrite(RELAY_PUMP, LOW); 
    digitalWrite(RELAY_LAMP, LOW);
    tone(BUZZER, 2500, 1000);
  }

  // CẬP NHẬT ĐẾM NGƯỢC REALTIME
  if (pumpState) {
    unsigned long elapsed = now - bathStartTime;
    if (elapsed < BATH_DURATION) {
      bathCountdown = BATH_DURATION - elapsed;
    } else {
      bathCountdown = 0;
      pumpState = false;
      digitalWrite(RELAY_PUMP, HIGH);
    }
  }
  if (ti->tm_hour == 0 && ti->tm_min == 0 && ti->tm_sec < 5) bathTriggeredToday = false;
  if (pumpState && now - bathStartTime >= BATH_DURATION) {
    pumpState = false; digitalWrite(RELAY_PUMP, HIGH);
  }

  // NÚT BẤM
  if (digitalRead(BTN_LAMP)==LOW)  { delay(50); if(digitalRead(BTN_LAMP)==LOW)  { lampState=!lampState; digitalWrite(RELAY_LAMP, lampState?LOW:HIGH); while(digitalRead(BTN_LAMP)==LOW); }}
  if (digitalRead(BTN_FAN)==LOW)   { delay(50); if(digitalRead(BTN_FAN)==LOW)   { manualFan=!manualFan; fanState=!fanState; digitalWrite(RELAY_FAN, fanState?LOW:HIGH); while(digitalRead(BTN_FAN)==LOW); }}
  if (digitalRead(BTN_LCD)==LOW)   { delay(50); if(digitalRead(BTN_LCD)==LOW)   { currentPage=(currentPage+1)%8; while(digitalRead(BTN_LCD)==LOW); }}

  // GỬI DỮ LIỆU LÊN FIREBASE MỖI 10 GIÂY
if (Firebase.ready() && (millis() - lastUpload >= 10000 || lastUpload == 0)) {
  lastUpload = millis();

  FirebaseJson json;

  unsigned long uptimeSec = (millis() - systemStartTime) / 1000;
  unsigned long daysTotal = uptimeSec / 86400;

  int months = daysTotal / 30;
  int days   = daysTotal % 30;
  char uptimeStr[10];
  sprintf(uptimeStr, "%02dm-%02dd", months, days);
  json.set("UpTime", uptimeStr);
  /* ===== SENSORS ===== */
  json.set("Sensors/Temperature", t);
  json.set("Sensors/Humidity", h);
  json.set("Sensors/MQ2", mq2_raw);
  json.set("Sensors/MQ135", mq135_raw);

  /* ===== DEVICES ===== */
  json.set("Devices/Lamp", lampState);
  json.set("Devices/Fan", fanState);
  json.set("Devices/Pump", pumpState);
  json.set("Devices/Door", servoIsOpen);

  /* ===== AUTO SCHEDULE ===== */
  json.set("AutoSchedule/IsRunning", pumpState);
  json.set("AutoSchedule/TodayWatered", bathTriggeredToday);
  json.set("AutoSchedule/WateringDuration", 900);
  json.set("AutoSchedule/NextWatering", "16:30");

  /* ===== ALERTS ===== */
  json.set("Alerts/Buzzer", criticalDanger || warningActive);
  json.set("Alerts/Gas_alert", gasHigh);
  json.set("Alerts/Pollution_alert", airHigh || criticalDanger);
  json.set("Alerts/Temperature_alert", tempHigh);

  // 🔥 DÒNG QUAN TRỌNG NHẤT
  if (Firebase.RTDB.updateNode(&fbdo, "/FarmStatus", &json)) {
    Serial.println("✅ Firebase updated");
  } else {
    Serial.println("❌ Firebase error: " + fbdo.errorReason());
  }
}
  // HIỂN THỊ + BÁO ĐỘNG ĐỎ
  if (millis() - lastBlink > (criticalDanger ? 200 : 500)) { 
    lastBlink = millis(); blinkState = !blinkState; 
  }
  lcd.clear();

  if (criticalDanger && blinkState) {
    lcd.setCursor(0,0); lcd.print("!!! NGUY HIEM !!!");
    lcd.setCursor(0,1); lcd.print("NH3 QUA CAO!    ");
    tone(BUZZER, 3000);
  }
  else if (warningActive && blinkState) {
    lcd.setCursor(0,0); lcd.print("!!! CANH BAO !!!");
    lcd.setCursor(0,1);
    if (tempHigh) lcd.print("NHIET DO CAO!   ");
    else if (gasHigh) lcd.print("KHI GAS CAO     ");
    else lcd.print("O NHIEM KHONG KHI");
  } 
  else {
    switch(currentPage) {
      case 0: if(servoIsOpen){ lcd.setCursor(4,0); lcd.print("CUA DA MO"); 
                               lcd.setCursor(0,1); lcd.print("SE DONG SAU:"); 
                               lcd.setCursor(13,1); lcd.print((5000-(now-doorOpenTime))/1000); lcd.print("s"); } break;
      case 1: lcd.setCursor(0,0); lcd.printf("DAY: %02d/%02d/%04d", ti->tm_mday, ti->tm_mon+1, ti->tm_year+1900);
              lcd.setCursor(0,1); lcd.printf("TIME: %02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec); break;
      case 2: lcd.setCursor(0,0); isnan(t)?lcd.print("TEMP: ERR  "):lcd.printf("TEMP: %.1fC", t);
              lcd.setCursor(0,1); isnan(h)?lcd.print("HUMI: ERR  "):lcd.printf("HUMI: %.1f%%", h); break;
      case 3: lcd.setCursor(0,0); lcd.print("MQ135 (NH3)     "); lcd.setCursor(0,1); lcd.printf("Raw: %4d    ", mq135_raw); break;
      case 4: lcd.setCursor(0,0); lcd.print("MQ2 (Gas/Khoi)  "); lcd.setCursor(0,1); lcd.printf("Raw: %4d    ", mq2_raw); break;
      case 5: lcd.setCursor(0,0); lcd.print("FAN:"); lcd.print(fanState?" ON ":" OFF");
              lcd.setCursor(0,1); lcd.print("LAMP:"); lcd.print(lampState?" ON ":" OFF"); break;
      case 6:
        if (pumpState) {
          lcd.setCursor(0,0); lcd.print("TT: DANG TAM   ");
          int mins = bathCountdown / 60000;
          int secs = (bathCountdown % 60000) / 1000;
          lcd.setCursor(0,1); lcd.printf("Con: %02d:%02d       ", mins, secs);
        } else {
          lcd.setCursor(0,0); lcd.print("TAM: 16:30      ");
          lcd.setCursor(0,1); lcd.print("16:30 hang ngay ");
        }
        break;
      case 7:
          unsigned long uptimeSec = (millis() - systemStartTime) / 1000;
          unsigned long daysTotal = uptimeSec / 86400;

          int months = daysTotal / 30;
          int days   = daysTotal % 30;

          int hours = (uptimeSec % 86400) / 3600;
          int mins  = (uptimeSec % 3600) / 60;
          int secs  = uptimeSec % 60;

          lcd.setCursor(0, 0);
          lcd.print(F("  TG NUOI HEO  "));

          lcd.setCursor(0, 1);
          lcd.printf("%02dm-%02dd %02d:%02d:%02d  ",months, days, hours, mins, secs);
      break;
    }
  }
  if (!criticalDanger && !warningActive) noTone(BUZZER);
  delay(150);
}