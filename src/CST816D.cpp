#include "CST816D.h"

CST816D::CST816D(int8_t sda_pin,
                 int8_t scl_pin,
                 int8_t rst_pin,
                 int8_t int_pin)
    : _sda(sda_pin),
      _scl(scl_pin),
      _rst(rst_pin),
      _int(int_pin) {
}

void CST816D::begin() {

    // Spotpear ESP32-C3 için I2C pinleri:
    // SDA = GPIO11
    // SCL = GPIO7
    if (_sda >= 0 && _scl >= 0) {
        Wire.begin(_sda, _scl);
    } else {
        Wire.begin();
    }

    Wire.setClock(400000);

    // Touch reset
    if (_rst >= 0) {
        pinMode(_rst, OUTPUT);

        digitalWrite(_rst, LOW);
        delay(10);

        digitalWrite(_rst, HIGH);
        delay(300);
    }

    // INT bu projede kullanılmıyor.
    // Dokunmayı I2C üzerinden doğrudan okuyoruz.
    if (_int >= 0) {
        pinMode(_int, INPUT);
    }

    // CST816D'nin otomatik düşük güç moduna geçmesini engelle.
    writeRegister(0xFE, 0xFF);
}

bool CST816D::getTouch(uint16_t* x,
                       uint16_t* y,
                       uint8_t* gesture) {

    if (!x || !y || !gesture) {
        return false;
    }

    // Register 0x02:
    // dokunma/finger bilgisi
    uint8_t finger = readRegister(0x02);

    // Gesture register
    *gesture = readRegister(0x01);

    // Gesture kodlarını koruyoruz.
    // 0x00 ise dokunma yok kabul edilir.
    if (finger == 0) {
        return false;
    }

    // X/Y bilgisi 0x03-0x06 registerlarından gelir.
    uint8_t data[4];

    if (!readRegisters(0x03, data, 4)) {
        return false;
    }

    *x = ((uint16_t)(data[0] & 0x0F) << 8) | data[1];
    *y = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];

    return true;
}

uint8_t CST816D::readRegister(uint8_t reg) {

    Wire.beginTransmission(CST816D_I2C_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return 0;
    }

    uint8_t count =
        Wire.requestFrom((int)CST816D_I2C_ADDR, 1);

    if (count != 1 || !Wire.available()) {
        return 0;
    }

    return Wire.read();
}

bool CST816D::readRegisters(uint8_t reg,
                            uint8_t* data,
                            size_t len) {

    Wire.beginTransmission(CST816D_I2C_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    uint8_t count =
        Wire.requestFrom((int)CST816D_I2C_ADDR, (int)len);

    if (count != len) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (!Wire.available()) {
            return false;
        }

        data[i] = Wire.read();
    }

    return true;
}

void CST816D::writeRegister(uint8_t reg,
                            uint8_t value) {

    Wire.beginTransmission(CST816D_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}
