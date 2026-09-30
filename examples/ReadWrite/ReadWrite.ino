#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15; // Пин B7 на MCP23017
ESPboySDLib::File myFile;  // Строгое указание пространства имен

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("ReadWrite");
  Serial.print("Initializing SD card...");

  // Передаем пин и ссылку на расширитель MCP
  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (true);
  }
  Serial.println("initialization done.");

  // Открытие и запись
  myFile = ESPboySDLib::ESPboy_SD.open("test.txt", FILE_WRITE);
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");
    myFile.close();
    Serial.println("done.");
  } else {
    Serial.println("error opening test.txt");
  }

  // Чтение
  myFile = ESPboySDLib::ESPboy_SD.open("test.txt");
  if (myFile) {
    Serial.println("test.txt:");
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    myFile.close();
  } else {
    Serial.println("error opening test.txt");
  }
}

void loop() {
}