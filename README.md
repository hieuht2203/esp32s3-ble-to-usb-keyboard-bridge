# ESP32-S3 Bluetooth-to-USB Keyboard Bridge
> **Biến bàn phím Bluetooth thành bàn phím USB cắm dây siêu nhạy, hỗ trợ chế độ BIOS (Boot Protocol).**
> **Turn any Bluetooth keyboard into a ultra-low latency wired USB keyboard, with BIOS (Boot Protocol) support.**

---

## 🇻🇳 TIẾNG VIỆT - HƯỚNG DẪN SỬ DỤNG

Dự án này sử dụng chip **ESP32-S3** (như mạch siêu nhỏ ESP32-S3 Zero) để làm cầu nối: Nhận tín hiệu từ bàn phím Bluetooth (BLE) và giả lập thành một bàn phím USB tiêu chuẩn cắm trực tiếp vào máy tính.

### 🌟 Tính năng nổi bật
* **Hỗ trợ BIOS / Boot Menu (GRUB):** Nhờ cơ chế chuyển đổi thông minh, khi tắt debug serial, thiết bị sẽ hoạt động như một bàn phím USB đơn năng chạy chuẩn *USB HID Boot Protocol*, tương thích hoàn toàn khi boot máy tính hoặc cài đặt BIOS.
* **Tự động kết nối lại (Auto-Reconnect):** Hỗ trợ đầy đủ cho bàn phím dùng địa chỉ MAC tĩnh và địa chỉ ngẫu nhiên thay đổi (RPA). Tự động nhận diện và kết nối lại ngay lập tức khi bạn chuyển chế độ trên bàn phím hoặc cắm lại nguồn ESP32.
* **Bảo mật và chống kết nối nhầm:** Lọc kết nối nghiêm ngặt. Khi đã ghép đôi với một bàn phím, thiết bị sẽ từ chối kết nối với các bàn phím lạ khác trong khu vực.
* **Tiết kiệm năng lượng và tản nhiệt thấp:** Tắt hoàn toàn sóng Wi-Fi để giảm nhiệt độ hoạt động và tiết kiệm điện năng.
* **Reset bộ nhớ dễ dàng:** Chỉ cần giữ nút BOOT trên mạch trong 3 giây để xóa toàn bộ lịch sử ghép đôi và kết nối bàn phím mới.

### 🛠️ Yêu cầu phần cứng
1. Mạch phát triển **ESP32-S3** hỗ trợ cổng USB native (như *ESP32-S3 Zero* hoặc *ESP32-S3 DevKit*).
2. Bàn phím Bluetooth hỗ trợ chuẩn **Bluetooth Low Energy (BLE)** (Bluetooth 5.0 trở lên).

### 💡 Trạng thái Đèn LED (NeoPixel - GPIO 21)
* **Chớp ĐỎ mờ (chu kỳ 1s):** Đang quét tìm kiếm bàn phím đã ghép đôi hoặc bàn phím mới.
* **Sáng XANH LÁ mờ (trong 5 giây đầu):** Kết nối thành công bàn phím. Đèn tự động tắt sau 5s để tiết kiệm điện.
* **Sáng VÀNG rực (trong 1 giây):** Đang xóa thông tin ghép đôi cũ để chuẩn bị restart.

### ⚙️ Cấu hình và Nạp Code
Dự án được quản lý bằng **PlatformIO** trong VS Code.

#### 1. Chế độ Thử nghiệm & Debug (Mở cổng COM ảo)
Để theo dõi log kết nối qua Monitor Serial:
* Trong file `src/main.cpp`: Thiết lập `#define DEBUG_MODE 1`
* Trong file `platformio.ini`: Thiết lập `-D ARDUINO_USB_CDC_ON_BOOT=1`
* *Lưu ý: Chế độ này sẽ KHÔNG dùng được trong BIOS do BIOS không nhận diện thiết bị USB phức hợp.*

#### 2. Chế độ Hoàn thiện / Sử dụng Thực tế (Chạy trong BIOS)
Để sử dụng làm bàn phím hàng ngày (nhận ngay từ màn hình BIOS):
* Trong file `src/main.cpp`: Thiết lập `#define DEBUG_MODE 0`
* Trong file `platformio.ini`: Thiết lập `-D ARDUINO_USB_CDC_ON_BOOT=0`
* Tiến hành nạp code. Thiết bị sẽ hoạt động như bàn phím USB thuần túy.

#### 🔄 Cách Ghép Đôi Bàn Phím Mới
1. Nhấn và **giữ nút BOOT** trên ESP32 trong **3 giây** cho đến khi LED sáng màu VÀNG rực. Thiết bị sẽ xóa bộ nhớ và tự khởi động lại.
2. Bật chế độ ghép đôi (Pairing Mode) trên bàn phím của bạn.
3. ESP32 sẽ tự động dò tìm, kết nối và lưu khóa bảo mật (Bonding) của bàn phím mới vào bộ nhớ Flash.

---

## 🇬🇧 ENGLISH - USER MANUAL

This project uses the **ESP32-S3** chip (such as the ultra-compact ESP32-S3 Zero) to act as a bridge: receiving signals from a Bluetooth LE (BLE) keyboard and emulating a standard USB keyboard connected directly to a PC.

### 🌟 Features
* **BIOS / Boot Menu (GRUB) Support:** When debug serial is disabled, the device runs as a single-interface USB keyboard using the *USB HID Boot Protocol*, allowing you to use it in BIOS and boot selection screens.
* **Smart Auto-Reconnect:** Fully supports keyboards using static MAC addresses or Resolvable Private Addresses (RPA). Instantly reconnects when switching keyboard modes or power-cycling the ESP32.
* **Strict Bond Matching:** Once bonded to a keyboard, the bridge automatically rejects connections from any other random Bluetooth devices in the area.
* **Optimized Power & Thermal Management:** WiFi is completely shut down to minimize heat and power consumption.
* **One-Button Bond Reset:** Simply press and hold the physical BOOT button for 3 seconds to clear the pairing history and pair with a new keyboard.

### 🛠️ Hardware Requirements
1. An **ESP32-S3** development board with native USB OTG pins (e.g., *ESP32-S3 Zero* or *ESP32-S3 DevKitC-1*).
2. A Bluetooth keyboard supporting **Bluetooth Low Energy (BLE)** (Bluetooth 5.0+).

### 💡 LED Status Indicators (NeoPixel - GPIO 21)
* **Blinking Dim Red (1s interval):** Scanning/searching for the bonded keyboard or a new keyboard.
* **Solid Dim Green (for 5s):** Successfully connected to the keyboard. Automatically turns off after 5 seconds to conserve energy.
* **Solid Bright Yellow (for 1s):** Clearing bonding history, followed by a system restart.

### ⚙️ Compilation & Flashing Instructions
This project is configured using **PlatformIO** in VS Code.

#### 1. Development & Debug Mode (COM Port Active)
To view connection logs via the Serial Monitor:
* In `src/main.cpp`: Set `#define DEBUG_MODE 1`
* In `platformio.ini`: Set `-D ARDUINO_USB_CDC_ON_BOOT=1`
* *Note: The keyboard will NOT work in BIOS in this mode because BIOS cannot handle composite USB devices.*

#### 2. Production Mode (BIOS Support Active)
For daily use (keyboard working immediately in BIOS/GRUB):
* In `src/main.cpp`: Set `#define DEBUG_MODE 0`
* In `platformio.ini`: Set `-D ARDUINO_USB_CDC_ON_BOOT=0`
* Flash the code. The device will emulate a pure USB keyboard.

#### 🔄 Pairing a New Keyboard
1. Press and **hold the BOOT button** on the ESP32 for **3 seconds** until the LED turns bright yellow. The device will clear its bond memory and restart.
2. Put your Bluetooth keyboard into pairing mode.
3. The ESP32 will automatically scan, connect, and persist the bonding key of the new keyboard in its Flash storage.
