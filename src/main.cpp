#include "USB.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDKeyboard.h"
#include "esp_pm.h"
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <NimBLEDevice.h>

// ---------------------------------------------------------
// CẤU HÌNH CƠ BẢN
// ---------------------------------------------------------
#define LED_PIN 21
#define NUM_PIXELS 1
#define BOOT_BUTTON_PIN 0 // Nút BOOT trên mạch ESP32-S3 Zero
#define DEBUG_MODE 0 // Đổi thành 0 để tắt log Serial khi dự án đã hoàn thiện

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

// ---------------------------------------------------------
// ĐỐI TƯỢNG TOÀN CỤC
// ---------------------------------------------------------
Adafruit_NeoPixel pixel(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
USBHIDKeyboard Keyboard;
USBHIDConsumerControl Consumer;

static NimBLEUUID hidServiceUUID("1812");
static NimBLEUUID reportUUID("2A4D");
static NimBLEUUID batteryServiceUUID("180F");
static NimBLEUUID batteryLevelUUID("2A19");

// Đóng gói toàn bộ biến trạng thái vào 1 struct cho gọn gàng
struct BridgeState {
  bool doConnect = false;
  bool doScan = true; // Mặc định khởi động sẽ quét
  bool isConnected = false;
  unsigned long lastBlinkTime = 0;
  unsigned long connectedTime = 0;
  bool ledState = false;
  bool greenLedDone = false;
  unsigned long buttonPressStartTime = 0;
  bool isResettingBonds = false;
  NimBLEAdvertisedDevice *advDevice = nullptr;
} state;

// ---------------------------------------------------------
// HÀM BỔ TRỢ (LED & XỬ LÝ DỮ LIỆU)
// ---------------------------------------------------------

void updateLED() {
  if (!state.isConnected) {
    // Chớp ĐỎ mờ khi đang tìm kiếm (Chu kỳ 1000ms - chớp chậm để tiết kiệm
    // điện)
    if (millis() - state.lastBlinkTime >= 1000) {
      state.lastBlinkTime = millis();
      state.ledState = !state.ledState;
      pixel.setPixelColor(0, state.ledState ? pixel.Color(40, 0, 0)
                                            : pixel.Color(0, 0, 0));
      pixel.show();
    }
  } else if (!state.greenLedDone) {
    // Tự động tắt LED xanh sau 5 giây kết nối
    if (millis() - state.connectedTime >= 5000) {
      pixel.setPixelColor(0, pixel.Color(0, 0, 0));
      pixel.show();
      state.greenLedDone = true;
      DEBUG_PRINTLN("Đã tắt đèn LED Xanh.");
    }
  }
}

void notifyCallback(NimBLERemoteCharacteristic *pChar, uint8_t *pData,
                    size_t length, bool isNotify) {
  if (length == 8) {
    // 1. Phím bàn phím chuẩn
    KeyReport report;
    memcpy(&report, pData, 8);
    Keyboard.sendReport(&report);
  }
  // TẠM TẮT PHÍM MEDIA ĐỂ TEST GRUB
  // else if (length == 2) {
  //   uint16_t mediaKey;
  //   memcpy(&mediaKey, pData, 2);
  //   Consumer.press(mediaKey);
  //   Consumer.release();
  // }
}

void batteryNotifyCallback(NimBLERemoteCharacteristic *pChar, uint8_t *pData,
                           size_t length, bool isNotify) {
  if (length > 0) {
    DEBUG_PRINTF("Pin bàn phím: %d%%\n", pData[0]);
  }
}

// ---------------------------------------------------------
// CALLBACKS BLUETOOTH
// ---------------------------------------------------------

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient *pClient) override {
    state.isConnected = true;
    state.greenLedDone = false;
    state.connectedTime = millis();
    DEBUG_PRINTLN("Đã kết nối bàn phím!");

    // Bật LED xanh mờ một lần duy nhất khi vừa kết nối
    pixel.setPixelColor(0, pixel.Color(0, 40, 0));
    pixel.show();
  }

  void onDisconnect(NimBLEClient *pClient) override {
    state.isConnected = false;
    state.lastBlinkTime = 0; // Đặt 0 để kích hoạt chớp đỏ ngay lập tức
    DEBUG_PRINTLN("Mất kết nối. Bắt đầu quét lại...");
    state.doScan = true;
  }
};
static ClientCallbacks clientCallbacks;

class SecurityCallbacks : public NimBLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    DEBUG_PRINTLN("Yêu cầu nhập PassKey (Mã PIN)! Đang gửi mã: 123456");
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    DEBUG_PRINTF("Mã PIN của thiết bị là: %d\n", pass_key);
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    DEBUG_PRINTF("Xác nhận mã PIN: %d\n", pass_key);
    return true;
  }

  bool onSecurityRequest() override {
    DEBUG_PRINTLN("Yêu cầu bảo mật từ thiết bị.");
    return true;
  }

  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    if (!desc->sec_state.encrypted) {
      DEBUG_PRINTLN("Lỗi: Kết nối không được mã hóa!");
    } else {
      DEBUG_PRINTLN("Hoàn tất xác thực bảo mật!");
    }
  }
};

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
    bool isKeyboard = false;

    // 1. Kiểm tra Appearance (0x03C1 = 961 là Bàn phím)
    if (advertisedDevice->haveAppearance() &&
        advertisedDevice->getAppearance() == 961) {
      isKeyboard = true;
    }

    // 2. Kiểm tra Service UUID (1812 là chuẩn HID)
    if (advertisedDevice->haveServiceUUID() &&
        advertisedDevice->isAdvertisingService(NimBLEUUID("1812"))) {
      isKeyboard = true;
    }

    // 3. Nếu đã có thiết bị được ghép đôi trong NVS, kiểm tra khớp địa chỉ MAC.
    // Cách này giúp nhận diện bàn phím khi nó kết nối lại (reconnect mode) -
    // lúc này bàn phím thường lược bỏ UUID 1812 và Appearance trong bản tin
    // quảng cáo.
    int numBonds = NimBLEDevice::getNumBonds();
    if (numBonds > 0) {
      for (int i = 0; i < numBonds; i++) {
        if (NimBLEDevice::getBondedAddress(i) ==
            advertisedDevice->getAddress()) {
          isKeyboard = true;
          DEBUG_PRINTLN("Nhận diện bàn phím đã ghép đôi qua địa chỉ MAC.");
          break;
        }
      }
    }

    if (isKeyboard) {
      DEBUG_PRINTF("Tìm thấy bàn phím phù hợp: %s (%s)\n",
                   advertisedDevice->getName().c_str(),
                   advertisedDevice->getAddress().toString().c_str());

      NimBLEDevice::getScan()->stop();
      state.advDevice = advertisedDevice;
      state.doConnect = true;
    }
  }
};

// ---------------------------------------------------------
// LOGIC KẾT NỐI CHÍNH
// ---------------------------------------------------------

bool connectToServer() {
  // 1. Dọn dẹp Client cũ để chống tràn RAM (Memory Leak)
  while (NimBLEClient *pOldClient = NimBLEDevice::getDisconnectedClient()) {
    NimBLEDevice::deleteClient(pOldClient);
  }

  // 2. Kiểm tra giới hạn & Tạo Client mới
  if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
    DEBUG_PRINTLN("Lỗi: Quá giới hạn kết nối BLE!");
    return false;
  }

  NimBLEClient *pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&clientCallbacks, false);

  // Đặt tham số kết nối BLE tối ưu cho độ trễ thấp
  pClient->setConnectionParams(9, 12, 0, 200);

  // 3. Thực hiện kết nối vật lý
  if (!pClient->connect(state.advDevice)) {
    DEBUG_PRINTLN("Kết nối thất bại. Đang dọn dẹp...");
    NimBLEDevice::deleteClient(pClient);
    return false;
  }

  // 4. Bảo mật & Ghép đôi (Xác thực ẩn)
  pClient->secureConnection();

  // Chờ cho quá trình xác thực/mã hóa hoàn tất (tối đa 2 giây)
  unsigned long secStart = millis();
  while (pClient->getConnInfo().isEncrypted() == false &&
         (millis() - secStart < 2000)) {
    delay(50);
  }

  // Nếu ESP32 đã từng ghép đôi với bàn phím trước đó,
  // nhưng kết nối hiện tại không được xác nhận là đã ghép đôi (isBonded ==
  // false), ta sẽ ngắt kết nối ngay lập tức để tránh kết nối nhầm sang thiết bị
  // lạ.
  int numBonds = NimBLEDevice::getNumBonds();
  DEBUG_PRINTF("Số lượng ghép đôi hiện tại trong NVS: %d\n", numBonds);
  if (numBonds > 0) {
    if (!pClient->getConnInfo().isBonded()) {
      DEBUG_PRINTLN("Cảnh báo: Thiết bị kết nối không phải là bàn phím đã ghép "
                    "đôi! Đang ngắt kết nối...");
      pClient->disconnect();
      NimBLEDevice::deleteClient(pClient);
      return false;
    }
    DEBUG_PRINTLN("Thiết bị được xác nhận đã ghép đôi (Bonded).");
  }

  // 5. Đăng ký nhận sự kiện gõ phím (HID Service)
  NimBLERemoteService *pHidService = pClient->getService(hidServiceUUID);
  if (pHidService) {
    for (auto &charac : *pHidService->getCharacteristics(true)) {
      if (charac->getUUID() == reportUUID && charac->canNotify()) {
        charac->subscribe(true, notifyCallback);
      }
    }
  }

  // 6. Đăng ký nhận mức pin (Battery Service)
  NimBLERemoteService *pBattService = pClient->getService(batteryServiceUUID);
  if (pBattService) {
    NimBLERemoteCharacteristic *pChar =
        pBattService->getCharacteristic(batteryLevelUUID);
    if (pChar && pChar->canNotify()) {
      pChar->subscribe(true, batteryNotifyCallback);

      // Lấy mức pin ngay lập tức lần đầu
      std::string value = pChar->readValue();
      if (value.length() > 0) {
        DEBUG_PRINTF("Pin hiện tại: %d%%\n", (uint8_t)value[0]);
      }
    }
  }

  DEBUG_PRINTLN("Bridge đã sẵn sàng hoạt động!");
  return true;
}

// ---------------------------------------------------------
// SETUP & LOOP
// ---------------------------------------------------------

void setup() {
  // Khởi tạo LED báo hiệu ngay lập tức
  pixel.begin();
  pixel.setBrightness(
      20); // Giảm từ 75 xuống 20 để tiết kiệm điện và giảm nhiệt
  pixel.setPixelColor(0, pixel.Color(40, 0, 0)); // Đỏ mờ khi khởi động
  pixel.show();

  // Tối ưu năng lượng (Tắt WiFi, cấu hình CPU)
  esp_wifi_stop();
  // esp_pm_config_esp32s3_t pm_config = {
  //     .max_freq_mhz = 80, .min_freq_mhz = 40, .light_sleep_enable = false};
  // esp_pm_configure(&pm_config);

  // Cấu hình chân nút nhấn BOOT (GPIO 0) làm ngõ vào kéo lên nguồn
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

#if DEBUG_MODE
  Serial.begin(115200);
#endif

  // Khởi tạo giao diện giả lập USB
  Keyboard.begin();
  // TẠM TẮT CONSUMER ĐỂ ÉP CHUẨN BOOT PROTOCOL THUẦN TÚY CHO GRUB
  // Consumer.begin();
  USB.begin();

  // Khởi tạo Bluetooth LE
  NimBLEDevice::init("ESP32-S3-Bridge");
  NimBLEDevice::setSecurityAuth(true, true, true);

  // Trả lại chế độ Just Works (Không yêu cầu nhập PIN)
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  NimBLEScan *pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  // Bật lại Active Scan và quét nhanh để nhận bàn phím ngay lập tức khi PC vừa
  // bật (Vào BIOS)
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(50); // Duty cycle 50% là đủ nhanh mà không quá nóng
  // Vòng lặp loop() sẽ tự động ra lệnh quét nhờ biến state.doScan = true
}

void checkResetButton() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (state.buttonPressStartTime == 0) {
      state.buttonPressStartTime = millis();
    } else if (millis() - state.buttonPressStartTime >= 3000) {
      if (!state.isResettingBonds) {
        state.isResettingBonds = true;
        DEBUG_PRINTLN("Đang xóa toàn bộ thông tin ghép nối bàn phím...");

        // Sáng LED vàng rực để thông báo cho người dùng biết đang reset
        pixel.setPixelColor(0, pixel.Color(120, 100, 0));
        pixel.show();

        // Xóa tất cả bonds lưu trong flash
        NimBLEDevice::deleteAllBonds();
        delay(1000); // Giữ đèn vàng sáng trong 1 giây trước khi restart

        DEBUG_PRINTLN("Đã xóa xong. Đang khởi động lại...");
        ESP.restart();
      }
    }
  } else {
    state.buttonPressStartTime = 0;
  }
}

void loop() {
  // 0. Kiểm tra trạng thái nút nhấn xóa ghép nối
  checkResetButton();

  // 1. Xử lý yêu cầu kết nối
  if (state.doConnect) {
    if (!connectToServer()) {
      state.doScan = true; // Kết nối hỏng -> quét lại
    }
    state.doConnect = false;
  }

  // 2. Xử lý yêu cầu quét thiết bị (Bất đồng bộ - Không chặn luồng)
  if (state.doScan) {
    DEBUG_PRINTLN("Đang dò tìm bàn phím...");
    NimBLEDevice::getScan()->start(0, nullptr, false);
    state.doScan = false;
  }

  // 3. Cập nhật trạng thái đèn báo hiệu
  updateLED();

  // 4. Nghỉ ngơi CPU (Giảm tải nhiệt độ & điện năng)
  // Khi đã kết nối: delay 100ms (BLE notify callback xử lý độc lập, không bị
  // ảnh hưởng) Khi đang scan: delay 50ms (scan chạy nền, loop chỉ canh cờ
  // doConnect)
  delay(state.isConnected ? 100 : 50);
}
