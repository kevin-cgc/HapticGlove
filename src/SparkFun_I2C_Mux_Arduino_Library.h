/*
  This is an Arduino library written for the TCA9548A/PCA9548A 8-bit multiplexer.
  By Nathan Seidle @ SparkFun Electronics, May 16th, 2020

  The TCA9548A/PCA9548A allows for up to 8 devices to be attached to a single
  I2C bus. This is helpful for I2C devices that have a single I2C address.

  https://github.com/sparkfun/SparkFun_I2C_Mux_Arduino_Library

  SparkFun labored with love to create this code. Feel like supporting open
  source? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14685

  The MIT License (MIT)
  Copyright (c) 2016 SparkFun Electronics
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef SparkFun_I2C_Mux_Arduino_Library_h
#define SparkFun_I2C_Mux_Arduino_Library_h

#include "Arduino.h"
#include <Wire.h>

#define QWIIC_MUX_DEFAULT_ADDRESS 0x70

class QWIICMUX
{
public:
  bool begin(uint8_t deviceAddress = QWIIC_MUX_DEFAULT_ADDRESS, TwoWire &wirePort = Wire); //Check communication and initialize device
  bool isConnected();                                                                      //Returns true if device acks at the I2C address
  bool setPort(uint8_t portNumber);                                                        //Enable a single port. All other ports disabled.
  bool setPortState(uint8_t portBits);                                                     //Overwrite port register with all 8 bits. Allows multiple bit writing in one call.
  uint8_t getPort();                                                                       //Returns the bit position of the first enabled port. Useful for IDing which port number is enabled.
  uint8_t getPortState();                                                                  //Returns current 8-bit wide state. May have multiple bits set in 8-bit field.
  bool enablePort(uint8_t portNumber);                                                     //Enable a single port without affecting other bits
  bool disablePort(uint8_t portNumber);                                                    //Disable a single port without affecting other bits

private:
  TwoWire *_i2cPort;                                  //This stores the user's requested i2c port
  uint8_t _deviceAddress = QWIIC_MUX_DEFAULT_ADDRESS; //Default unshifted 7-bit address
};
#endif
