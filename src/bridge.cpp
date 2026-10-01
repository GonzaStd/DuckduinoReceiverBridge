/*
 * ESP-01 ESP-NOW -> UART Bridge
 *
 * Receives payload frames from the badge (ESP32) over ESP-NOW and streams
 * the raw payload bytes to the ProMicro through hardware UART as fast as
 * possible. No buffering of the whole file: each DATA frame is forwarded
 * the moment it arrives.
 *
 * Frame format (shared with RfSender on the ESP32 side):
 * [TYPE(1)][CHUNK_ID_HI(1)][CHUNK_ID_LO(1)][SIZE(1)][DATA(size)][CRC8(1)]
 *   TYPE: 0x01 = START, 0x02 = DATA, 0x03 = END
 */

extern "C" {
  #include <user_interface.h>
  #include <espnow.h>
}
#include <ESP8266WiFi.h>

#define ESPNOW_WIFI_CHANNEL 1
#define UART_BAUD           115200

#define FRAME_START 0x01
#define FRAME_DATA  0x02
#define FRAME_END   0x03

uint8_t computeCrc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    }
  }
  return crc;
}

void onEspNowReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len < 5) return; // TYPE + ID(2) + SIZE + CRC minimum

  uint8_t type = data[0];
  uint8_t size = data[3];

  if ((4 + size + 1) != len) return; // malformed frame, drop it

  uint8_t crcGot  = data[4 + size];
  uint8_t crcCalc = computeCrc8(data, 4 + size);
  if (crcCalc != crcGot) return; // corrupted frame, drop it silently

  switch (type) {
    case FRAME_START:
      Serial.print(F("\n--START--\n"));
      break;
    case FRAME_DATA:
      Serial.write(&data[4], size); // forward payload text immediately
      break;
    case FRAME_END:
      Serial.print(F("\n--END--\n"));
      break;
  }
}

void setup() {
  Serial.begin(UART_BAUD);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(ESPNOW_WIFI_CHANNEL);

  // Print this module's MAC so it can be copied into the badge's
  // HardwareConfig.h (ESP01_MAC_ADDR). Visible on the ProMicro's USB
  // serial monitor, since the ProMicro just relays this UART.
  Serial.print(F("\nESP-01 MAC: "));
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0) {
    Serial.println(F("ESP-NOW init failed"));
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onEspNowReceive);
}

void loop() {
  // Everything happens inside onEspNowReceive().
}