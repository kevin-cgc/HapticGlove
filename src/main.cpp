#include <optional>
#include <Wire.h>

#define LED_BUILTIN 2

#include <Haptic_Driver.h>
#define DEFAULT_DRIVER_ADDR DEF_ADDR
#define NUM_DRIVERS 16
Haptic_Driver hap_drivers[NUM_DRIVERS];

#include <SparkFun_I2C_Mux_Arduino_Library.h>
#define NUM_MUX 2
QWIICMUX i2cmux[NUM_MUX];


#define DRIVER_PER_MUX 8
static_assert(NUM_DRIVERS / NUM_MUX == DRIVER_PER_MUX, "NUM_DRIVERS / NUM_MUX must be equal to DRIVER_PER_MUX");

struct MuxDriverIndex {
  int mux_idx;
  int port_idx;
};
MuxDriverIndex getMuxDriverIndex(int hap_idx) {
  return { mux_idx: hap_idx / DRIVER_PER_MUX, port_idx: hap_idx % DRIVER_PER_MUX };
}

/// @brief Rapidly set the vibrate override value (amplitude) for the driver currently selected by the mux
/// @param val input can be [0, 255] but is remapped to [0, 0x7f] so lsb is ignored
/// @return
bool fast_set_vibrate_override_val(uint8_t val) {
  uint8_t write_val = val >> 1;
  Wire.beginTransmission(DEFAULT_DRIVER_ADDR);
  Wire.write(TOP_CTL2);
  Wire.write(write_val);
  if (Wire.endTransmission() == 0) return true;
  else return false;
}
bool fast_set_all_drivers(const uint8_t driver_amplitudes[NUM_DRIVERS]) {
  bool success = true;
  // for (int mux_idx=0; mux_idx<NUM_MUX; mux_idx++) { // assume unsafe, disable all ports first adds 0.2ms delay
  //   success &= i2cmux[mux_idx].setPortState(0x00);
  // }
  // if (!success) return false;

  int last_mux_idx = 0;
  for (int hap_idx=0; hap_idx<NUM_DRIVERS; hap_idx++) {
    auto v = getMuxDriverIndex(hap_idx); auto mux_idx = v.mux_idx; auto port_idx = v.port_idx;
    if (mux_idx != last_mux_idx) {
      i2cmux[last_mux_idx].setPortState(0x00);
      last_mux_idx = mux_idx;
    }

    success &= i2cmux[mux_idx].setPort(port_idx);

    success &= fast_set_vibrate_override_val(driver_amplitudes[hap_idx]);
  }
  i2cmux[last_mux_idx].setPortState(0x00);
  return success;
}
bool zero_all_drivers() {
  const uint8_t zero_amplitudes[NUM_DRIVERS] = {0};
  return fast_set_all_drivers(zero_amplitudes);
}

// bool set_single_driver_amplitude(int hap_idx, int amplitude) {
//   auto v = getMuxDriverIndex(hap_idx); auto mux_idx = v.mux_idx; auto port_idx = v.port_idx;

//   bool success = true;
//   success &= i2cmux[mux_idx].setPort(port_idx);
//   // hap_drivers[hap_idx].clearIrq(hap_drivers[hap_idx].getIrqEvent()); dont do this, we should react appropriately to these event, could be important (overtemp, short, etc.)

//   // success &= hap_drivers[hap_idx].setVibrate(amplitude);
//   success &= fast_set_vibrate_override_val(amplitude);

//   success &= i2cmux[mux_idx].disablePort(port_idx);

//   return success;
// }

void setup() {
  Serial.begin(921600);
  Serial.println("INFO: Starting...");
  Wire.begin();
  // Both ESP32 and TCA9548A have been characterized for 400kHz
  // Both also have employee suggestions via forums that 1MHz should work but is not guaranteed
  Wire.setClock(400000);

  pinMode(LED_BUILTIN, OUTPUT);

   // Wait for both muxes to be ready and zeroed
  while (true) if (i2cmux[0].begin(0x70, Wire) && i2cmux[1].begin(0x71, Wire)) break;

  for (int hap_idx=0; hap_idx<NUM_DRIVERS; hap_idx++) {
    auto v = getMuxDriverIndex(hap_idx); auto mux_idx = v.mux_idx; auto port_idx = v.port_idx;

    bool success = true;
    success &= i2cmux[mux_idx].setPort(port_idx);
    success &= hap_drivers[hap_idx].begin(Wire);
    success &= hap_drivers[hap_idx].enableFreqTrack(false);
    success &= hap_drivers[hap_idx].setMotor(hapticSettings {
      .motorType = LRA_TYPE,
      .nomVolt = 2.500, // from Vibronics config uses 0x6B (2.5038V)
      .absVolt = 2.500, // same as above
      .currMax = 170, // 0x14 (172.6 mA)
      .impedance = 12.0,  // 0xB2 (11.94 ohms)
      .lraFreq = 170.0, // 0x22, 0x3C (169.993 hz)
    });
    success &= hap_drivers[hap_idx].setOperationMode(OPERATION_MODES::DRO_MODE); // During playback, if output driving strength is maintained at 0 %, DA7280 will disable its output stage to save power. Drive is re-enabled automatically, with one LRA half-period delay, when a non-zero OVERRIDE_VAL value I2C input is received.
    success &= hap_drivers[hap_idx].setVibrate(0);
    success &= i2cmux[mux_idx].disablePort(port_idx);
    if (!success) {
      Serial.println("ERROR: Failed to initialize driver " + (String)hap_idx);
    }
  }


  // iterate all drivers to 100% for 1s each
  uint8_t test_driver_amplitudes[NUM_DRIVERS] = {0};
  for (int hap_idx=0; hap_idx<NUM_DRIVERS; hap_idx++) {
    Serial.println("INFO: Testing driver " + (String)hap_idx + "...");
    test_driver_amplitudes[hap_idx] = 0xFF;
    fast_set_all_drivers(test_driver_amplitudes);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH); // <5ms flash
    test_driver_amplitudes[hap_idx] = 0;
  }
  fast_set_all_drivers(test_driver_amplitudes);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("INFO: ESP32 based Haptic Glove Driver Board v1. Visit https://github.com/kevin-cgc/HapticGlove for more information.");
  Serial.println("INFO: Ready LED");
}

const uint8_t COBS_DELIM = 0x88; // using unlikely byte as delim
#define HEADER_LEN 1 // 1 byte COBS overhead
#define FOOTER_LEN 1 // 1 byte delim
const int PACKET_LEN = HEADER_LEN + NUM_DRIVERS + FOOTER_LEN;

uint8_t packet_buf[PACKET_LEN] = {0};
uint8_t* driver_amplitudes = packet_buf + HEADER_LEN;
const char ACK_PACKET[] = "OKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOKOK\r\n"; // CP2102 (usb-uart IC) sends packets once they hit 64 bytes (https://community.silabs.com/s/article/usb-latency?language=en_US)
static_assert(sizeof(ACK_PACKET) == 64+1, "ACK_PACKET must be 64 bytes long (plus null terminator)");
std::optional<unsigned long> last_packet_time = std::nullopt;

void loop() {
  if (Serial.available() >= PACKET_LEN) {
    digitalWrite(LED_BUILTIN, HIGH);
    last_packet_time = millis();

    Serial.readBytes(packet_buf, PACKET_LEN);

    // Serial.print(ACK_PACKET);
    // return;

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

    bool success = fast_set_all_drivers(driver_amplitudes);
    if (!success) Serial.println("ERROR: Failed to set driver amplitudes.");
    else Serial.print(ACK_PACKET);

  } else if (last_packet_time.has_value() && millis() - last_packet_time.value() > 1000) { // on timeout, zero all drivers
    zero_all_drivers();
    last_packet_time = std::nullopt;
  }

  digitalWrite(LED_BUILTIN, LOW);
}
