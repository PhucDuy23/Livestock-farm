#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <HTTPClient.h> 
#include <Firebase_ESP_Client.h>
// ===========================
// Select camera model in board_config.h
// ===========================

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Dii";
const char *password = "duydeptrai";
/*const char *ssid = "Dii";
const char *password = "duydeptrai";*/

#define BOT_TOKEN "8323152528:AAEzBS8szcuXmt4g77G4If5VNV29aC37bfE"   // Thay token của bạn
#define CHAT_ID   "7956012707"// Thay chat_id của bạn


WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
#include "board_config.h"
// Biến global để chia sẻ buffer ảnh với callback (bắt buộc cho v1.3.0)
camera_fb_t * global_fb = NULL;
size_t currentByte = 0;
size_t fb_length = 0;
uint8_t* fb_buffer = NULL;

// Callback: Kiểm tra còn dữ liệu không (trả bool)
bool isMoreDataAvailable() {
  return currentByte < fb_length;
}

// Callback: Lấy byte tiếp theo (trả uint8_t)
uint8_t getNextByte() {
  if (currentByte < fb_length) {
    return fb_buffer[currentByte++];
  }
  return 0;
}
// ← THÊM HÀM NÀY VÀO ĐÂY (chỉ 3 dòng thôi!)
int getBufferLen() {
  return 1024;
}

void startCameraServer();
void setupLedFlash();
unsigned long lastCaptureTime = 0;
const unsigned long interval = 1UL * 60UL * 1000UL;  // 5 phút

void sendPhoto() {

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    bot.sendMessage(CHAT_ID, "Camera capture failed", "");
    Serial.println("Camera capture failed");
    return;
  }

  // Lưu vào global để callback dùng
  global_fb = fb;
  fb_length = fb->len;
  fb_buffer = fb->buf;
  currentByte = 0;

  bot.sendMessage(CHAT_ID, "Đang chụp ảnh chuồng trại...", "");

  // Gọi hàm với ĐÚNG 7 tham số cho v1.3.0 (contentType = "image/jpeg")
  String sent = bot.sendPhotoByBinary(
  CHAT_ID,                    // chat_id
  "image/jpeg",               // contentType
  (int)fb->len,               // fileSize (phải ép kiểu int)
  isMoreDataAvailable,        // callback: còn dữ liệu không?
  getNextByte,                // callback: lấy byte tiếp theo
  nullptr,                    // GetNextBuffer (không dùng)
  getBufferLen                // ← Đây là hàm mới thêm ở trên (không có dấu ngoặc!)
);

  if (sent.indexOf("\"ok\":true") != -1) {  // kiểm tra chính xác JSON có "ok":true không
  bot.sendMessage(CHAT_ID, "Gửi ảnh thành công!");
  Serial.println("Photo sent successfully");
} else {
  bot.sendMessage(CHAT_ID, "Gửi ảnh thất bại!");
  Serial.println("Failed to send photo: " + sent);
}

  // Giải phóng buffer
  esp_camera_fb_return(fb);
  global_fb = NULL;
  fb_length = 0;
  fb_buffer = NULL;
  currentByte = 0;
}

bool readSensorsFromFirebase(float &t, float &h, int &mq2, int &mq135) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();   // Bỏ kiểm tra SSL

  HTTPClient http;
  String url = "https://livestockfarm-aa6e1-default-rtdb.firebaseio.com/FarmStatus/Sensors.json";

  http.begin(secureClient, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("JSON parse failed");
    return false;
  }

  t     = doc["Temperature"].as<float>();
  h     = doc["Humidity"].as<float>();
  mq2   = doc["MQ2"].as<int>();
  mq135 = doc["MQ135"].as<int>();

  return true;
}
////
bool readDevicesFromFirebase( bool &fan, bool &lamp, bool &pump) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  String url = "https://livestockfarm-aa6e1-default-rtdb.firebaseio.com/FarmStatus/Devices.json";

  http.begin(secureClient, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ Read Devices HTTP error: " + String(httpCode));
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  Serial.println("📦 Devices JSON: " + payload); // DEBUG CỰC QUAN TRỌNG

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("❌ Devices JSON parse failed");
    return false;
  }
  fan  = doc["Fan"]  | false;
  lamp = doc["Lamp"] | false;
  pump = doc["Pump"] | false;

  return true;
}
////
bool readAlertsFromFirebase(bool &gas, bool &pollution, bool &temp) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  String url = "https://livestockfarm-aa6e1-default-rtdb.firebaseio.com/FarmStatus/Alerts.json";

  http.begin(secureClient, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ Read Alerts HTTP error: " + String(httpCode));
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("❌ Alerts JSON parse failed");
    return false;
  }

  gas        = doc["Gas_alert"] | false;
  pollution = doc["Pollution_alert"] | false;
  temp       = doc["Temperature_alert"] | false;

  return true;
}
/////
bool readTodayWateredFromFirebase(bool &todayWatered) {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  String url = "https://livestockfarm-aa6e1-default-rtdb.firebaseio.com/FarmStatus/AutoSchedule/TodayWatered.json";

  http.begin(secureClient, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ Read TodayWatered HTTP error");
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  todayWatered = (payload.indexOf("true") != -1);
  return true;
}

void sendStatusToTelegram() {
  float t, h;
  int mq2, mq135;
  bool lamp, fan, pump;
  bool todayWatered;

  if (!readSensorsFromFirebase(t, h, mq2, mq135)) {
    bot.sendMessage(CHAT_ID, "❌ Không đọc được Sensors", "");
    return;
  }

  if (!readDevicesFromFirebase(fan, lamp, pump)) {
    bot.sendMessage(CHAT_ID, "❌ Không đọc được Devices", "");
    return;
  }
  readTodayWateredFromFirebase(todayWatered);
  String msg;
  msg += "🐷 LIVESTOCK FARM 4.0 🐷\n\n";

  msg += "📊 CẢM BIẾN\n";
  msg += "🌡 Nhiệt độ: " + String(t,1) + " °C\n";
  msg += "💧 Độ ẩm: " + String(h,1) + " %\n";
  msg += "🔥 MQ2: " + String(mq2) + "\n";
  msg += "💨 MQ135: " + String(mq135) + "\n\n";

  msg += "🔌 THIẾT BỊ\n";
  msg += "💡 Đèn: " + String(lamp ? "BẬT" : "TẮT") + "\n";
  msg += "🌀 Quạt: " + String(fan ? "BẬT" : "TẮT") + "\n";
  msg += "🚿 Bơm: " + String(pump ? "BẬT" : "TẮT") + "\n";
  msg += "🚿 TẮM VẬT NUÔI\n";
  msg += String(todayWatered ? "✅ Đã tắm hôm nay" : "❌ Chưa tắm hôm nay");
  bot.sendMessage(CHAT_ID, msg, "");
}
void checkAndSendAlerts() {
  bool gasAlert, pollutionAlert, tempAlert;

  if (!readAlertsFromFirebase(gasAlert, pollutionAlert, tempAlert)) {
    Serial.println("❌ Không đọc được Alerts từ Firebase");
    return;
  }

  // Nếu không có cảnh báo nào → không gửi tin nhắn
  if (!gasAlert && !pollutionAlert && !tempAlert) return;

  String msg = "⚠️ CẢNH BÁO CHUỒNG TRẠI ⚠️\n\n";

  if (gasAlert) msg += "🔥 Gas vượt ngưỡng!\n";
  if (pollutionAlert) msg += "☠ Ô nhiễm không khí!\n";
  if (tempAlert) msg += "🌡 Nhiệt độ cao!\n";

  bot.sendMessage(CHAT_ID, msg, "");
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  //startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  client.setInsecure();  // Bắt buộc với ESP32
  startCameraServer();

  bot.sendMessage(CHAT_ID, "Camera đã khởi động! Sẽ chụp mỗi 5 phút", "");
  lastCaptureTime = millis();
}

void loop() {
  if (millis() - lastCaptureTime >= interval) {
    sendPhoto();
    lastCaptureTime = millis();
  }
  delay(1000);

  static unsigned long lastSend = 0;

if (millis() - lastSend > 60000) {   // 1 phút
  lastSend = millis();
  sendStatusToTelegram();
}

static unsigned long lastAlertCheck = 0;

if (millis() - lastAlertCheck > 5000) {   // kiểm tra alert mỗi 5 giây
  lastAlertCheck = millis();
  checkAndSendAlerts();
}
}