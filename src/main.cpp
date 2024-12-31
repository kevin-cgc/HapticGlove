#include <Haptic_Driver.h>
#include <SparkFun_I2C_Mux_Arduino_Library.h>
#include <Wire.h>

#define NUM_MUX 2
QWIICMUX i2cmux[NUM_MUX];

#define NUM_DRIVERS 16
Haptic_Driver hap_drivers[NUM_DRIVERS];

#define DRIVER_PER_MUX 8
static_assert(NUM_DRIVERS / NUM_MUX == DRIVER_PER_MUX, "NUM_DRIVERS / NUM_MUX must be equal to DRIVER_PER_MUX");

struct MuxDriverIndex {
  int mux_idx;
  int port_idx;
};
MuxDriverIndex getMuxDriverIndex(int hap_idx) {
  return { mux_idx: hap_idx / DRIVER_PER_MUX, port_idx: hap_idx % DRIVER_PER_MUX };
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  while (true)
    if (i2cmux[0].begin() && i2cmux[1].begin())
      break;  // Wait for both muxes to be ready

  for (int hap_idx=0; hap_idx<NUM_DRIVERS; hap_idx++) {
    auto v = getMuxDriverIndex(hap_idx); auto mux_idx = v.mux_idx; auto port_idx = v.port_idx;

    bool success = true;
    success &= i2cmux[mux_idx].setPort(port_idx);
    success &= hap_drivers[hap_idx].begin();
    success &= hap_drivers[hap_idx].enableFreqTrack(false);
    success &= hap_drivers[hap_idx].setOperationMode(OPERATION_MODES::INACTIVE);
    success &= hap_drivers[hap_idx].setVibrate(0);
    success &= i2cmux[mux_idx].disablePort(port_idx);
    if (!success) {
      Serial.println("ERROR: Failed to initialize driver " + (String)hap_idx);
    }
  }
  Serial.println("INFO: ESP32 based Haptic Glove Driver Board v1. Visit https://github.com/kevin-cgc/HapticGlove for more information.");
  Serial.println("INFO: Ready");
}

bool set_driver_amplitude(int hap_idx, int amplitude) {
  auto v = getMuxDriverIndex(hap_idx); auto mux_idx = v.mux_idx; auto port_idx = v.port_idx;

  bool success = true;
  success &= i2cmux[mux_idx].setPort(port_idx);
  hap_drivers[hap_idx].clearIrq(hap_drivers[hap_idx].getIrqEvent());

  if (amplitude == 0) {
    success &= hap_drivers[hap_idx].setOperationMode(OPERATION_MODES::INACTIVE);
  } else {
    success &= hap_drivers[hap_idx].setOperationMode(OPERATION_MODES::DRO_MODE);
    success &= hap_drivers[hap_idx].setVibrate(amplitude);
  }

  success &= i2cmux[mux_idx].disablePort(port_idx);

  return success;
}


const uint8_t COBS_DELIM = 0x88; // using unlikely byte as delim
#define HEADER_LEN 1 // 1 byte COBS overhead
#define FOOTER_LEN 1 // 1 byte delim
const int PACKET_LEN = HEADER_LEN + NUM_DRIVERS + FOOTER_LEN;

uint8_t packet_buf[PACKET_LEN] = {0};
uint8_t* driver_amplitudes = packet_buf + HEADER_LEN;

void loop() {
  if (Serial.available() >= PACKET_LEN) {
    Serial.readBytes(packet_buf, PACKET_LEN);

    // Check for desync
    if (packet_buf[PACKET_LEN-1] != COBS_DELIM) {
      while (Serial.read() != COBS_DELIM) {} // resync
      return;
    }

    // COBS decode
    uint32_t i=0;
    while (i < PACKET_LEN-1) {
      auto next_sym = packet_buf[i];
      packet_buf[i] = COBS_DELIM;
      if (next_sym == 0) { Serial.println("ERROR: invalid COBS"); return; } // avoid infinite loop if next_sym is zero, should not occur in valid messages though
      i += next_sym;
    }

    for (int hap_idx=0; hap_idx<NUM_DRIVERS; hap_idx++) {
      bool success = set_driver_amplitude(hap_idx, driver_amplitudes[hap_idx]);
      if (!success) {
        Serial.println("ERROR: Failed to set amplitude for driver " + (String)hap_idx);
      }
    }
  }
}
