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




#define USE_SPI_LIB
#include <Arduino.h>
#include "Sd2Card.h"
#include "../ESPboyMCP.h"

//------------------------------------------------------------------------------
#ifndef SOFTWARE_SPI
#ifdef USE_SPI_LIB

  #ifndef SDCARD_SPI
    #define SDCARD_SPI SPI
  #endif

  #include <SPI.h>
  static SPISettings settings;
#endif

// functions for hardware SPI
static void spiSend(uint8_t b) {
  #ifndef USE_SPI_LIB
  SPDR = b;
  while (!(SPSR & (1 << SPIF)))
    ;
  #else
  SDCARD_SPI.transfer(b);
  #endif
}

static uint8_t spiRec(void) {
  #ifndef USE_SPI_LIB
  spiSend(0XFF);
  return SPDR;
  #else
  return SDCARD_SPI.transfer(0xFF);
  #endif
}
#else  // SOFTWARE_SPI

#define nop asm volatile ("nop\n\t")

uint8_t spiRec(void) {
  uint8_t data = 0;
  cli();
  fastDigitalWrite(SPI_MOSI_PIN, HIGH);

  for (uint8_t i = 0; i < 8; i++) {
    fastDigitalWrite(SPI_SCK_PIN, HIGH);
    nop;
    nop;
    data <<= 1;
    if (fastDigitalRead(SPI_MISO_PIN)) {
      data |= 1;
    }
    fastDigitalWrite(SPI_SCK_PIN, LOW);
  }
  sei();
  return data;
}

void spiSend(uint8_t data) {
  cli();
  for (uint8_t i = 0; i < 8; i++) {
    fastDigitalWrite(SPI_SCK_PIN, LOW);
    fastDigitalWrite(SPI_MOSI_PIN, data & 0X80);
    data <<= 1;
    fastDigitalWrite(SPI_SCK_PIN, HIGH);
  }
  nop;
  nop;
  nop;
  nop;
  fastDigitalWrite(SPI_SCK_PIN, LOW);
  sei();
}
#endif  // SOFTWARE_SPI

//------------------------------------------------------------------------------
uint8_t Sd2Card::cardCommand(uint8_t cmd, uint32_t arg) {
  readEnd();
  chipSelectLow();
  waitNotBusy(300);

  spiSend(cmd | 0x40);

  for (int8_t s = 24; s >= 0; s -= 8) {
    spiSend(arg >> s);
  }

  uint8_t crc = 0XFF;
  if (cmd == CMD0) {
    crc = 0X95;
  }
  if (cmd == CMD8) {
    crc = 0X87;
  }
  spiSend(crc);

  for (uint8_t i = 0; ((status_ = spiRec()) & 0X80) && i != 0XFF; i++)
    ;
  return status_;
}

//------------------------------------------------------------------------------
uint32_t Sd2Card::cardSize(void) {
  csd_t csd;
  if (!readCSD(&csd)) {
    return 0;
  }
  if (csd.v1.csd_ver == 0) {
    uint8_t read_bl_len = csd.v1.read_bl_len;
    uint16_t c_size = (csd.v1.c_size_high << 10)
                      | (csd.v1.c_size_mid << 2) | csd.v1.c_size_low;
    uint8_t c_size_mult = (csd.v1.c_size_mult_high << 1)
                          | csd.v1.c_size_mult_low;
    return (uint32_t)(c_size + 1) << (c_size_mult + read_bl_len - 7);
  } else if (csd.v2.csd_ver == 1) {
    uint32_t c_size = ((uint32_t)csd.v2.c_size_high << 16)
                      | (csd.v2.c_size_mid << 8) | csd.v2.c_size_low;
    return (c_size + 1) << 10;
  } else {
    error(SD_CARD_ERROR_BAD_CSD);
    return 0;
  }
}

//------------------------------------------------------------------------------
static uint8_t chip_select_asserted = 0;

void Sd2Card::chipSelectHigh(void) {
  if (mcp_) {
    mcp_->digitalWrite(chipSelectPin_, HIGH);
  } else {
    digitalWrite(chipSelectPin_, HIGH);
  }

  #ifdef USE_SPI_LIB
  if (chip_select_asserted) {
    chip_select_asserted = 0;
    SDCARD_SPI.endTransaction();
  }
  #endif
}

//------------------------------------------------------------------------------
void Sd2Card::chipSelectLow(void) {
  #ifdef USE_SPI_LIB
  if (!chip_select_asserted) {
    chip_select_asserted = 1;
    SDCARD_SPI.beginTransaction(settings);
  }
  #endif

  if (mcp_) {
    mcp_->digitalWrite(chipSelectPin_, LOW);
  } else {
    digitalWrite(chipSelectPin_, LOW);
  }
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::erase(uint32_t firstBlock, uint32_t lastBlock) {
  if (!eraseSingleBlockEnable()) {
    error(SD_CARD_ERROR_ERASE_SINGLE_BLOCK);
    goto fail;
  }
  if (type_ != SD_CARD_TYPE_SDHC) {
    firstBlock <<= 9;
    lastBlock <<= 9;
  }
  if (cardCommand(CMD32, firstBlock)
      || cardCommand(CMD33, lastBlock)
      || cardCommand(CMD38, 0)) {
    error(SD_CARD_ERROR_ERASE);
    goto fail;
  }
  if (!waitNotBusy(SD_ERASE_TIMEOUT)) {
    error(SD_CARD_ERROR_ERASE_TIMEOUT);
    goto fail;
  }
  chipSelectHigh();
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::eraseSingleBlockEnable(void) {
  csd_t csd;
  return readCSD(&csd) ? csd.v1.erase_blk_en : 0;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::init(uint8_t sckRateID, uint8_t chipSelectPin, ESPboyMCP *mcp) {
  errorCode_ = inBlock_ = partialBlockRead_ = type_ = 0;
  chipSelectPin_ = chipSelectPin;
  mcp_ = mcp;

  unsigned int t0 = millis();
  uint32_t arg;

  // Настройка режимов пина с учетом MCP
  if (mcp_) {
    mcp_->pinMode(chipSelectPin_, OUTPUT);
    mcp_->digitalWrite(chipSelectPin_, HIGH);
  } else {
    pinMode(chipSelectPin_, OUTPUT);
    digitalWrite(chipSelectPin_, HIGH);
  }

  #ifndef SOFTWARE_SPI
  #ifndef USE_SPI_LIB
  pinMode(SPI_MISO_PIN, INPUT);
  pinMode(SPI_MOSI_PIN, OUTPUT);
  pinMode(SPI_SCK_PIN, OUTPUT);
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
  SPSR &= ~(1 << SPI2X);
  #else
  SDCARD_SPI.begin();
  settings = SPISettings(250000, MSBFIRST, SPI_MODE0);
  #endif
  #endif

  #ifdef USE_SPI_LIB
  SDCARD_SPI.beginTransaction(settings);
  #endif
  for (uint8_t i = 0; i < 10; i++) {
    spiSend(0XFF);
  }
  #ifdef USE_SPI_LIB
  SDCARD_SPI.endTransaction();
  #endif

  chipSelectLow();

  while ((status_ = cardCommand(CMD0, 0)) != R1_IDLE_STATE) {
    unsigned int d = millis() - t0;
    if (d > SD_INIT_TIMEOUT) {
      error(SD_CARD_ERROR_CMD0);
      goto fail;
    }
  }

  if ((cardCommand(CMD8, 0x1AA) & R1_ILLEGAL_COMMAND)) {
    type(SD_CARD_TYPE_SD1);
  } else {
    for (uint8_t i = 0; i < 4; i++) {
      status_ = spiRec();
    }
    if (status_ != 0XAA) {
      error(SD_CARD_ERROR_CMD8);
      goto fail;
    }
    type(SD_CARD_TYPE_SD2);
  }

  arg = type() == SD_CARD_TYPE_SD2 ? 0X40000000 : 0;

  while ((status_ = cardAcmd(ACMD41, arg)) != R1_READY_STATE) {
    unsigned int d = millis() - t0;
    if (d > SD_INIT_TIMEOUT) {
      error(SD_CARD_ERROR_ACMD41);
      goto fail;
    }
  }

  if (type() == SD_CARD_TYPE_SD2) {
    if (cardCommand(CMD58, 0)) {
      error(SD_CARD_ERROR_CMD58);
      goto fail;
    }
    if ((spiRec() & 0XC0) == 0XC0) {
      type(SD_CARD_TYPE_SDHC);
    }
    for (uint8_t i = 0; i < 3; i++) {
      spiRec();
    }
  }
  chipSelectHigh();

  #ifndef SOFTWARE_SPI
  return setSckRate(sckRateID);
  #else
  return true;
  #endif

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
void Sd2Card::partialBlockRead(uint8_t value) {
  readEnd();
  partialBlockRead_ = value;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::readBlock(uint32_t block, uint8_t* dst) {
  return readData(block, 0, 512, dst);
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::readData(uint32_t block,
                          uint16_t offset, uint16_t count, uint8_t* dst) {
  if (count == 0) {
    return true;
  }
  if ((count + offset) > 512) {
    goto fail;
  }
  if (!inBlock_ || block != block_ || offset < offset_) {
    block_ = block;
    if (type() != SD_CARD_TYPE_SDHC) {
      block <<= 9;
    }
    if (cardCommand(CMD17, block)) {
      error(SD_CARD_ERROR_CMD17);
      goto fail;
    }
    if (!waitStartBlock()) {
      goto fail;
    }
    offset_ = 0;
    inBlock_ = 1;
  }

  for (; offset_ < offset; offset_++) {
    spiRec();
  }
  for (uint16_t i = 0; i < count; i++) {
    dst[i] = spiRec();
  }

  offset_ += count;
  if (!partialBlockRead_ || offset_ >= 512) {
    readEnd();
  }
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
void Sd2Card::readEnd(void) {
  if (inBlock_) {
    while (offset_++ < 514) {
      spiRec();
    }
    chipSelectHigh();
    inBlock_ = 0;
  }
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::readRegister(uint8_t cmd, void* buf) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  if (cardCommand(cmd, 0)) {
    error(SD_CARD_ERROR_READ_REG);
    goto fail;
  }
  if (!waitStartBlock()) {
    goto fail;
  }
  for (uint16_t i = 0; i < 16; i++) {
    dst[i] = spiRec();
  }
  spiRec();
  spiRec();
  chipSelectHigh();
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::setSckRate(uint8_t sckRateID) {
  if (sckRateID > 6) {
    error(SD_CARD_ERROR_SCK_RATE);
    return false;
  }
  #ifndef USE_SPI_LIB
  if ((sckRateID & 1) || sckRateID == 6) {
    SPSR &= ~(1 << SPI2X);
  } else {
    SPSR |= (1 << SPI2X);
  }
  SPCR &= ~((1 << SPR1) | (1 << SPR0));
  SPCR |= (sckRateID & 4 ? (1 << SPR1) : 0)
          | (sckRateID & 2 ? (1 << SPR0) : 0);
  #else
  switch (sckRateID) {
    case 0:  settings = SPISettings(25000000, MSBFIRST, SPI_MODE0); break;
    case 1:  settings = SPISettings(4000000, MSBFIRST, SPI_MODE0); break;
    case 2:  settings = SPISettings(2000000, MSBFIRST, SPI_MODE0); break;
    case 3:  settings = SPISettings(1000000, MSBFIRST, SPI_MODE0); break;
    case 4:  settings = SPISettings(500000, MSBFIRST, SPI_MODE0); break;
    case 5:  settings = SPISettings(250000, MSBFIRST, SPI_MODE0); break;
    default: settings = SPISettings(125000, MSBFIRST, SPI_MODE0);
  }
  #endif
  return true;
}

#ifdef USE_SPI_LIB
uint8_t Sd2Card::setSpiClock(uint32_t clock) {
  settings = SPISettings(clock, MSBFIRST, SPI_MODE0);
  return true;
}
#endif

//------------------------------------------------------------------------------
uint8_t Sd2Card::waitNotBusy(unsigned int timeoutMillis) {
  unsigned int t0 = millis();
  unsigned int d;
  do {
    if (spiRec() == 0XFF) {
      return true;
    }
    d = millis() - t0;
  } while (d < timeoutMillis);
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::waitStartBlock(void) {
  unsigned int t0 = millis();
  while ((status_ = spiRec()) == 0XFF) {
    unsigned int d = millis() - t0;
    if (d > SD_READ_TIMEOUT) {
      error(SD_CARD_ERROR_READ_TIMEOUT);
      goto fail;
    }
  }
  if (status_ != DATA_START_BLOCK) {
    error(SD_CARD_ERROR_READ);
    goto fail;
  }
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::writeBlock(uint32_t blockNumber, const uint8_t* src, uint8_t blocking) {
  #if SD_PROTECT_BLOCK_ZERO
  if (blockNumber == 0) {
    error(SD_CARD_ERROR_WRITE_BLOCK_ZERO);
    goto fail;
  }
  #endif

  if (type() != SD_CARD_TYPE_SDHC) {
    blockNumber <<= 9;
  }
  if (cardCommand(CMD24, blockNumber)) {
    error(SD_CARD_ERROR_CMD24);
    goto fail;
  }
  if (!writeData(DATA_START_BLOCK, src)) {
    goto fail;
  }
  if (blocking) {
    if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
      error(SD_CARD_ERROR_WRITE_TIMEOUT);
      goto fail;
    }
    if (cardCommand(CMD13, 0) || spiRec()) {
      error(SD_CARD_ERROR_WRITE_PROGRAMMING);
      goto fail;
    }
  }
  chipSelectHigh();
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::writeData(const uint8_t* src) {
  if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
    error(SD_CARD_ERROR_WRITE_MULTIPLE);
    chipSelectHigh();
    return false;
  }
  return writeData(WRITE_MULTIPLE_TOKEN, src);
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::writeData(uint8_t token, const uint8_t* src) {
  spiSend(token);
  for (uint16_t i = 0; i < 512; i++) {
    spiSend(src[i]);
  }
  spiSend(0xff);
  spiSend(0xff);

  status_ = spiRec();
  if ((status_ & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
    error(SD_CARD_ERROR_WRITE);
    chipSelectHigh();
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::writeStart(uint32_t blockNumber, uint32_t eraseCount) {
  #if SD_PROTECT_BLOCK_ZERO
  if (blockNumber == 0) {
    error(SD_CARD_ERROR_WRITE_BLOCK_ZERO);
    goto fail;
  }
  #endif
  if (cardAcmd(ACMD23, eraseCount)) {
    error(SD_CARD_ERROR_ACMD23);
    goto fail;
  }
  if (type() != SD_CARD_TYPE_SDHC) {
    blockNumber <<= 9;
  }
  if (cardCommand(CMD25, blockNumber)) {
    error(SD_CARD_ERROR_CMD25);
    goto fail;
  }
  return true;

fail:
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::writeStop(void) {
  if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
    goto fail;
  }
  spiSend(STOP_TRAN_TOKEN);
  if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
    goto fail;
  }
  chipSelectHigh();
  return true;

fail:
  error(SD_CARD_ERROR_STOP_TRAN);
  chipSelectHigh();
  return false;
}

//------------------------------------------------------------------------------
uint8_t Sd2Card::isBusy(void) {
  chipSelectLow();
  byte b = spiRec();
  chipSelectHigh();

  return (b != 0XFF);
}