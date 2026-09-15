#ifndef CST816D_CUSTOM_H
#define CST816D_CUSTOM_H

#include <Arduino.h>
#include <Wire.h>

#define CST816D_I2C_ADDR 0x15

// CST816D gesture kodları
#define GestureNone       0x00
#define SlideDown         0x01
#define SlideUp           0x02
#define SlideLeft         0x03
#define SlideRight        0x04
#define SingleTap         0x05
#define DoubleTap         0x0B
#define LongPress         0x0C

class CST816D {
public:
    CST816D(int8_t sda_pin = -1,
            int8_t scl_pin = -1,
            int8_t rst_pin = -1,
            int8_t int_pin = -1);

    void begin();

    bool getTouch(uint16_t* x,
                  uint16_t* y,
                  uint8_t* gesture);

private:
    int8_t _sda;
    int8_t _scl;
    int8_t _rst;
    int8_t _int;

    uint8_t readRegister(uint8_t reg);
    bool readRegisters(uint8_t reg, uint8_t* data, size_t len);
    void writeRegister(uint8_t reg, uint8_t value);
};

#endif
