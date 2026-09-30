#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15; // Pin B7 on MCP23017

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("Datalogger");
  Serial.print("Initializing SD card...");

  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (true);
  }
  Serial.println("initialization done.");
}

void loop() {
  String dataString = "";

  // Read ESPboy buttons instead of analog sensors
  uint8_t keys = myESPboy.getKeys();
  dataString += "Keys state: ";
  dataString += String(keys);
  dataString += ", Uptime: ";
  dataString += String(millis());

  // Open the file with strict namespace resolution
  ESPboySDLib::File dataFile = ESPboySDLib::ESPboy_SD.open("datalog.txt", FILE_WRITE);

  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.println(dataString);
  } else {
    Serial.println("error opening datalog.txt");
  }
  
  delay(1000); // Log every second
}