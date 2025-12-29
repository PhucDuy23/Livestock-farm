# livestock Farm - Hệ thống giám sát và điều khiển môi trường chuồng trại chăn nuôi heo thông minh

**ESP32 Pig Farm**\
![Sơ đồ khối hệ thống](Hardware\Images\system block diagram.jpg)

---

## Giới thiệu

Livestock Farm là đồ án xây dựng hệ thống giám sát và điều khiển môi
trường chuồng trại chăn nuôi heo thông minh dựa trên vi điều khiển
**ESP32** kết hợp công nghệ **IoT**.

### 🎯 Mục tiêu chính

- Tự động hóa việc giám sát nhiệt độ, độ ẩm, khí gas dễ cháy và khí
  amoniac (NH₃).\
- Xử lý khẩn cấp khi môi trường bất thường.\
- Thực hiện tắm tự động theo lịch.\
- Hiển thị thông tin tại chỗ và đồng bộ dữ liệu lên đám mây để theo
  dõi từ xa.

---

## 🚀 Tính năng chính

### 🔎 Giám sát môi trường realtime

- **DHT22**: đo nhiệt độ, độ ẩm\
- **MQ2**: phát hiện khí gas dễ cháy, khói\
- **MQ135**: phát hiện NH₃ và các khí độc\
- Hỗ trợ **hiệu chuẩn baseline tự động** khi khởi động

### 🚨 Cảnh báo & xử lý khẩn cấp

- Buzzer + nhấp nháy LCD khi vượt ngưỡng
- Tự động bật quạt, phun sương (bơm)
- Duy trì hoạt động thêm **5 phút** sau khi ổn định

### ⚙️ Điều khiển tự động & thủ công

- Tắm phun sương tự động **16:30 hằng ngày trong 15 phút** (kèm đèn)
- Điều khiển thủ công quạt, đèn bằng nút bấm

### 📟 Hiển thị tại chỗ

- LCD 16x2 --- 8 trang hiển thị:
  - Ngày giờ
  - Cảm biến
  - Trạng thái thiết bị
  - Lịch tắm
  - Thời gian nuôi

### ☁️ Kết nối đám mây

- Đồng bộ dữ liệu lên **Firebase Realtime Database** mỗi 10 giây\
- Theo dõi từ xa qua web/app

## 📩 Gửi cảnh báo qua Telegram

Hệ thống hỗ trợ gửi thông báo thời gian thực đến Telegram khi phát hiện
môi trường bất thường (nhiệt độ, độ ẩm, khí gas, NH₃ vượt ngưỡng) hoặc
khi có sự kiện quan trọng (mở cửa chuồng, bật/tắt thiết bị).
(Hardware\Images\Message_TeleBot.jpg)

### 🔐 An ninh truy cập

- **RFID MFRC522**\
- Servo điều khiển cửa (mở 5 giây khi quét thẻ đúng)

---

## 🛠 Cấu trúc phần cứng

- **Vi điều khiển chính**: ESP32 NodeMCU-32S\
- **Cảm biến**: DHT22, MQ2, MQ135\
- **Thiết bị điều khiển**: Relay (quạt, đèn, bơm), Servo, Buzzer\
- **Hiển thị**: LCD 16x2 I2C\
- **RFID**: MFRC522\
- **Kết nối**: WiFi (NTP + Firebase)
  ![Sơ đồ nguyên lý phần cứng](Hardware\Images\Schematic.jpg)
  ![Mạch PCB](Hardware\Images\PCB.jpg)

---

## 💻 Phần mềm

**Ngôn ngữ**: Arduino C++

### 📚 Thư viện chính

- ESP32Servo\
- DHT sensor library\
- MFRC522\
- LiquidCrystal_I2C\
- Firebase-ESP-Client\
- NTPClient

### 🧠 Logic chính

- Baseline cảm biến khí khi khởi động\
- Xử lý cảnh báo ưu tiên cao\
- Tắm tự động theo lịch\
- Gửi JSON lên Firebase

---

## ✅ Kết quả đạt được

- Hệ thống vận hành ổn định, phản hồi nhanh\
- Cảnh báo & xử lý khẩn cấp chính xác\
- Tắm tự động đúng giờ\
- Điều khiển thủ công mượt mà\
- Dữ liệu Firebase đồng bộ tốt\
- RFID + servo giúp bảo mật cửa chuồng

---

## ⚠️ Hạn chế & 🔮 Hướng phát triển

### Hạn chế

- Chưa tích hợp camera AI
- Chưa có app/dashboard chuyên dụng
- Chức năng mở cửa tự động đang tạm bỏ

### Hướng phát triển

- ESP32‑CAM + AI (YOLO) phát hiện bệnh
- App Flutter / Dashboard Web
- Năng lượng mặt trời + LoRa
- Blockchain truy xuất nguồn gốc

---

## 📂 Tài liệu & Code

- **Code nguồn**: main.ino\
- **Sơ đồ mạch**: (thêm Fritzing hoặc ảnh)\
- **Video demo**: (link YouTube khi có)

---

Pig Farm 4.0 -- Bước tiến nhỏ cho chăn nuôi heo Việt Nam theo hướng
**Nông nghiệp 4.0**! 🐷🌾

**Tác giả:** Đỗ Phúc Duy
**Trường:** Đại học Sư phạm Kỹ Thuật TP. Hồ Chí Minh
**Gmail:** dophucduy3@gmail.com
