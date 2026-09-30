#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15;
const char filename[] = "datalog.txt";

ESPboySDLib::File myFile;
String dataBuffer;
unsigned long lastMillis = 0;

void setup() {
  Serial.begin(9600);
  dataBuffer.reserve(1024);
  while (!Serial);

  myESPboy.begin("NonBlock");
  Serial.print("Initializing SD card...");

  if (!ESPboySDLib::ESPboy_SD.begin(chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed!");
    while (true);
  }
  Serial.println("initialization done.");

  myFile = ESPboySDLib::ESPboy_SD.open(filename, FILE_WRITE);
  if (!myFile) {
    Serial.print("error opening ");
    Serial.println(filename);
    while (true);
  }

  myFile.println();
  myFile.println("Hello World!");
  Serial.println("Starting to write to file...");
}

void loop() {
  unsigned long now = millis();
  if ((now - lastMillis) >= 10) {
    dataBuffer += "Hello ";
    dataBuffer += now;
    dataBuffer += "\r\n";
    Serial.print("Unsaved data buffer length (in bytes): ");
    Serial.println(dataBuffer.length());
    lastMillis = now;
  }

  unsigned int chunkSize = myFile.availableForWrite();
  if (chunkSize && dataBuffer.length() >= chunkSize) {
    // Blink ESPboy's RGB LED (Blue) when writing to SD
    myESPboy.myLED.setRGB(0, 0, 255);
    myESPboy.myLED.on();
    
    myFile.write((const uint8_t*)dataBuffer.c_str(), chunkSize);
    
    myESPboy.myLED.off();
    dataBuffer.remove(0, chunkSize);
  }
}