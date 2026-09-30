#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15;
ESPboySDLib::File myFile;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("Files Test");
  Serial.print("Initializing SD card...");

  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  if (ESPboySDLib::ESPboy_SD.exists("example.txt")) {
    Serial.println("example.txt exists.");
  } else {
    Serial.println("example.txt doesn't exist.");
  }

  Serial.println("Creating example.txt...");
  myFile = ESPboySDLib::ESPboy_SD.open("example.txt", FILE_WRITE);
  myFile.close();

  if (ESPboySDLib::ESPboy_SD.exists("example.txt")) {
    Serial.println("example.txt exists.");
  } else {
    Serial.println("example.txt doesn't exist.");
  }

  Serial.println("Removing example.txt...");
  ESPboySDLib::ESPboy_SD.remove("example.txt");

  if (ESPboySDLib::ESPboy_SD.exists("example.txt")) {
    Serial.println("example.txt exists.");
  } else {
    Serial.println("example.txt doesn't exist.");
  }
}

void loop() {
}