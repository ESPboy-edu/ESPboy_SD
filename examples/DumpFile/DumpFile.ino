#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("DumpFile");
  Serial.print("Initializing SD card...");

  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (true);
  }
  Serial.println("initialization done.");

  ESPboySDLib::File dataFile = ESPboySDLib::ESPboy_SD.open("datalog.txt");

  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  } else {
    Serial.println("error opening datalog.txt");
  }
}

void loop() {
}