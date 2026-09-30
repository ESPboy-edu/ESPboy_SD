/* Arduino Sd2Card Library
   Copyright (C) 2009 by William Greiman

   This file is part of the Arduino Sd2Card Library

   This Library is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the Arduino Sd2Card Library.  If not, see
   <http://www.gnu.org/licenses/>.
*/


#ifndef Sd2Card_h
#define Sd2Card_h

/**
   \file
   Sd2Card class
*/
#include "Sd2PinMap.h"
#include "SdInfo.h"
#include "../ESPboyMCP.h" // Подключаем класс MCP23017

/** Set SCK to max rate of F_CPU/2. See Sd2Card::setSckRate(). */
uint8_t const SPI_FULL_SPEED = 0;
/** Set SCK rate to F_CPU/4. See Sd2Card::setSckRate(). */
uint8_t const SPI_HALF_SPEED = 1;
/** Set SCK rate to F_CPU/8. Sd2Card::setSckRate(). */
uint8_t const SPI_QUARTER_SPEED = 2;

#define USE_SPI_LIB
#define MEGA_SOFT_SPI 0

#if MEGA_SOFT_SPI && (defined(__AVR_ATmega1280__)||defined(__AVR_ATmega2560__))
  #define SOFTWARE_SPI
#endif

#ifndef SOFTWARE_SPI
  #include <Arduino.h>

  #ifndef SDCARD_SS_PIN
    uint8_t const  SD_CHIP_SELECT_PIN = SS;
  #else
    uint8_t const  SD_CHIP_SELECT_PIN = SDCARD_SS_PIN;
  #endif

  #ifndef SDCARD_MOSI_PIN
    uint8_t const  SPI_MOSI_PIN = MOSI;
    uint8_t const  SPI_MISO_PIN = MISO;
    uint8_t const  SPI_SCK_PIN = SCK;
  #else
    uint8_t const  SPI_MOSI_PIN = SDCARD_MOSI_PIN;
    uint8_t const  SPI_MISO_PIN = SDCARD_MISO_PIN;
    uint8_t const  SPI_SCK_PIN = SDCARD_SCK_PIN;
  #endif

  #ifndef USE_SPI_LIB
    #define OPTIMIZE_HARDWARE_SPI
  #endif

#else
  uint8_t const SD_CHIP_SELECT_PIN = 10;
  uint8_t const SPI_MOSI_PIN = 11;
  uint8_t const SPI_MISO_PIN = 12;
  uint8_t const SPI_SCK_PIN = 13;
#endif

#define SD_PROTECT_BLOCK_ZERO 1
unsigned int const SD_INIT_TIMEOUT = 2000;
unsigned int const SD_ERASE_TIMEOUT = 10000;
unsigned int const SD_READ_TIMEOUT = 300;
unsigned int const SD_WRITE_TIMEOUT = 600;

// SD card errors
uint8_t const SD_CARD_ERROR_CMD0 = 0X1;
uint8_t const SD_CARD_ERROR_CMD8 = 0X2;
uint8_t const SD_CARD_ERROR_CMD17 = 0X3;
uint8_t const SD_CARD_ERROR_CMD24 = 0X4;
uint8_t const SD_CARD_ERROR_CMD25 = 0X05;
uint8_t const SD_CARD_ERROR_CMD58 = 0X06;
uint8_t const SD_CARD_ERROR_ACMD23 = 0X07;
uint8_t const SD_CARD_ERROR_ACMD41 = 0X08;
uint8_t const SD_CARD_ERROR_BAD_CSD = 0X09;
uint8_t const SD_CARD_ERROR_ERASE = 0X0A;
uint8_t const SD_CARD_ERROR_ERASE_SINGLE_BLOCK = 0X0B;
uint8_t const SD_CARD_ERROR_ERASE_TIMEOUT = 0X0C;
uint8_t const SD_CARD_ERROR_READ = 0X0D;
uint8_t const SD_CARD_ERROR_READ_REG = 0X0E;
uint8_t const SD_CARD_ERROR_READ_TIMEOUT = 0X0F;
uint8_t const SD_CARD_ERROR_STOP_TRAN = 0X10;
uint8_t const SD_CARD_ERROR_WRITE = 0X11;
uint8_t const SD_CARD_ERROR_WRITE_BLOCK_ZERO = 0X12;
uint8_t const SD_CARD_ERROR_WRITE_MULTIPLE = 0X13;
uint8_t const SD_CARD_ERROR_WRITE_PROGRAMMING = 0X14;
uint8_t const SD_CARD_ERROR_WRITE_TIMEOUT = 0X15;
uint8_t const SD_CARD_ERROR_SCK_RATE = 0X16;

// card types
uint8_t const SD_CARD_TYPE_SD1 = 1;
uint8_t const SD_CARD_TYPE_SD2 = 2;
uint8_t const SD_CARD_TYPE_SDHC = 3;

class Sd2Card {
  public:
    /** Construct an instance of Sd2Card. */
    Sd2Card(void) : mcp_(NULL), errorCode_(0), inBlock_(0), partialBlockRead_(0), type_(0) {}
    uint32_t cardSize(void);
    uint8_t erase(uint32_t firstBlock, uint32_t lastBlock);
    uint8_t eraseSingleBlockEnable(void);
    
    uint8_t errorCode(void) const {
      return errorCode_;
    }
    
    uint8_t errorData(void) const {
      return status_;
    }

    uint8_t init(void) {
      return init(SPI_FULL_SPEED, SD_CHIP_SELECT_PIN, NULL);
    }

    uint8_t init(uint8_t sckRateID) {
      return init(sckRateID, SD_CHIP_SELECT_PIN, NULL);
    }

    // Перегруженный метод инициализации с поддержкой ESPboyMCP
    uint8_t init(uint8_t sckRateID, uint8_t chipSelectPin, ESPboyMCP *mcp = NULL);

    void partialBlockRead(uint8_t value);
    uint8_t partialBlockRead(void) const {
      return partialBlockRead_;
    }
    uint8_t readBlock(uint32_t block, uint8_t* dst);
    uint8_t readData(uint32_t block,
                     uint16_t offset, uint16_t count, uint8_t* dst);

    uint8_t readCID(cid_t* cid) {
      return readRegister(CMD10, cid);
    }

    uint8_t readCSD(csd_t* csd) {
      return readRegister(CMD9, csd);
    }
    void readEnd(void);
    uint8_t setSckRate(uint8_t sckRateID);
    #ifdef USE_SPI_LIB
    uint8_t setSpiClock(uint32_t clock);
    #endif

    uint8_t type(void) const {
      return type_;
    }
    uint8_t writeBlock(uint32_t blockNumber, const uint8_t* src, uint8_t blocking = 1);
    uint8_t writeData(const uint8_t* src);
    uint8_t writeStart(uint32_t blockNumber, uint32_t eraseCount);
    uint8_t writeStop(void);
    uint8_t isBusy(void);

  private:
    ESPboyMCP* mcp_; // Указатель на экземпляр MCP23017
    uint32_t block_;
    uint8_t chipSelectPin_;
    uint8_t errorCode_;
    uint8_t inBlock_;
    uint16_t offset_;
    uint8_t partialBlockRead_;
    uint8_t status_;
    uint8_t type_;

    uint8_t cardAcmd(uint8_t cmd, uint32_t arg) {
      cardCommand(CMD55, 0);
      return cardCommand(cmd, arg);
    }
    uint8_t cardCommand(uint8_t cmd, uint32_t arg);
    void error(uint8_t code) {
      errorCode_ = code;
    }
    uint8_t readRegister(uint8_t cmd, void* buf);
    uint8_t sendWriteCommand(uint32_t blockNumber, uint32_t eraseCount);
    
    void chipSelectHigh(void);
    void chipSelectLow(void);
    
    void type(uint8_t value) {
      type_ = value;
    }
    uint8_t waitNotBusy(unsigned int timeoutMillis);
    uint8_t writeData(uint8_t token, const uint8_t* src);
    uint8_t waitStartBlock(void);
};

#endif  // Sd2Card_h