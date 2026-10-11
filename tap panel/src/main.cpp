#include <Arduino.h>
#include <Wire.h>


#define NUM_BUTTONS   3   // кол-во кнопок, можно изменять
#define DEBOUNCE   80


#define PCA9555_ADDR    0x20    // кнопки
#define PCA9532_ADDR    0x60    // светодиоды
#define EEPROM_ADDR   0x50    // память, под 24c02


// регистры PCA9555
#define PCA9555_INPUT0  0x00
#define PCA9555_CONFIG0 0x06
#define PCA9555_CONFIG1 0x07


// регистры PCA9532
#define PCA9532_LS0   0x06
#define PCA9532_LS1   0x07
#define PCA9532_LS2   0x08
#define PCA9532_LS3   0x09


// адрес в eeprom где храним состояние
#define EEPROM_STATE_ADDR   0x00


// массив под кол-во кнопок, храним 1 и 0.
uint8_t ledStates[NUM_BUTTONS];

// для антидребезга кнопок
uint8_t lastReading[NUM_BUTTONS];
uint8_t stableState[NUM_BUTTONS];
uint32_t lastChangeTime[NUM_BUTTONS];



// записать байт в регистр
bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

// прочитать байт из регистра
uint8_t i2cRead(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);  // без STOP чтобы сразу читать

    Wire.requestFrom((uint8_t)addr, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}



void pca9555Init() {
    i2cWrite(PCA9555_ADDR, PCA9555_CONFIG0, 0xFF);
    i2cWrite(PCA9555_ADDR, PCA9555_CONFIG1, 0xFF);
}


uint8_t readButtons() {
    uint8_t portValue = i2cRead(PCA9555_ADDR, PCA9555_INPUT0);
    portValue = ~portValue; // при нажатии кнопки побитовое инвертирование
    portValue = portValue & ((1 << NUM_BUTTONS) - 1);  // сдвигаем, что бы отображать только используемое кол-во кнопок
    return portValue;
}




void pca9532Init() {
    i2cWrite(PCA9532_ADDR, PCA9532_LS0, 0x00);
    i2cWrite(PCA9532_ADDR, PCA9532_LS1, 0x00);
    i2cWrite(PCA9532_ADDR, PCA9532_LS2, 0x00);
    i2cWrite(PCA9532_ADDR, PCA9532_LS3, 0x00);
}

void updateLeds() {
    // по 2 бита на светодиод 00 выкл, 01 вкл.
    uint8_t ls0 = 0;
    uint8_t ls1 = 0;
    uint8_t ls2 = 0;
    uint8_t ls3 = 0;

    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t bits = ledStates[i] ? 0x01 : 0x00;
        uint8_t shift = (i % 4) * 2;

        if ( i < 4 ) {
          ls0 |= (bits << shift);
        }
        else if ( i < 8 ) {
          ls1 |= (bits << shift);
        }
        else if ( i < 12 ) {
          ls2 |= (bits << shift);
        }
        else {
          ls3 |= (bits << shift);
        }
    }

    i2cWrite(PCA9532_ADDR, PCA9532_LS0, ls0);
    i2cWrite(PCA9532_ADDR, PCA9532_LS1, ls1);
    i2cWrite(PCA9532_ADDR, PCA9532_LS2, ls2);
    i2cWrite(PCA9532_ADDR, PCA9532_LS3, ls3);
}



void saveToEeprom() {
    uint8_t value = 0;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if ( ledStates[i] ) {
          value |= (1 << i);
        }
    }
    i2cWrite(EEPROM_ADDR, EEPROM_STATE_ADDR, value);
    delay(5);  // eeprom нужно время на запись
}

void loadFromEeprom() {
    uint8_t value = i2cRead(EEPROM_ADDR, EEPROM_STATE_ADDR);
    for (int i = 0; i < NUM_BUTTONS; i++) {
        ledStates[i] = (value >> i) & 0x01;
    }
}


// для поиска микрух, памяти
/* void scanI2C() {
    Serial.println("poisk I2C addr");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Found 0x");
            Serial.println(addr, HEX);
        }
    }
    Serial.println("done");
}*/



void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Startuem");

    Wire.begin();
    /*scanI2C();        для поиска устройств на шине */

    pca9555Init();
    pca9532Init();

    loadFromEeprom();
    updateLeds();

    // начальное состояние кнопок
    uint8_t startButtons = readButtons();
    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t b = (startButtons >> i) & 0x01;
        lastReading[i] = b;
        stableState[i] = b;
        lastChangeTime[i] = millis();
    }
}



void loop() {
    uint8_t currentButtons = readButtons();
    uint32_t now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t reading = (currentButtons >> i) & 0x01;

        if (reading != lastReading[i]) {
            lastReading[i] = reading;
            lastChangeTime[i] = now;
        }

        if ((now - lastChangeTime[i]) > DEBOUNCE) {
            if (reading != stableState[i]) {
                stableState[i] = reading;

                // Только при нажатии
                if (reading == 1) {
                    ledStates[i] = !ledStates[i];  // переключаем
                    updateLeds();
                    saveToEeprom();
                }
            }
        }
    }

    delay(10);
}