#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15;
ESPboySDLib::File root;

// Явное указание типа в аргументах функции
void printDirectory(ESPboySDLib::File dir, int numTabs) {
  while (true) {
    ESPboySDLib::File entry = dir.openNextFile();
    if (!entry) {
      break; // Больше нет файлов
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("List Files");
  Serial.print("Initializing SD card...");

  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (true);
  }
  Serial.println("initialization done.");

  root = ESPboySDLib::ESPboy_SD.open("/");
  printDirectory(root, 0);
  Serial.println("done!");
}

void loop() {
}