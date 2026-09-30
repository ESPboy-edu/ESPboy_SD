#include <SPI.h>
#include <ESPboy_SD.h>
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

ESPboyInit myESPboy;
const int chipSelect = 15;

// Utility classes are in global scope inside SdFat.h
Sd2Card card;
SdVolume volume;
SdFile root;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  myESPboy.begin("CardInfo");
  Serial.print("\nInitializing SD card...");

  // Use the modified init() method passing MCP pointer
  if (!card.init(SPI_HALF_SPEED, chipSelect, &myESPboy.mcp)) {
    Serial.println("initialization failed.");
    while (1);
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  Serial.print("Card type:         ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1: Serial.println("SD1"); break;
    case SD_CARD_TYPE_SD2: Serial.println("SD2"); break;
    case SD_CARD_TYPE_SDHC: Serial.println("SDHC"); break;
    default: Serial.println("Unknown");
  }

  if (!volume.init(&card)) {
    Serial.println("Could not find FAT16/FAT32 partition.");
    while (1);
  }

  Serial.print("Clusters:          ");
  Serial.println(volume.clusterCount());
  Serial.print("Blocks x Cluster:  ");
  Serial.println(volume.blocksPerCluster());
  Serial.print("Total Blocks:      ");
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  uint32_t volumesize;
  Serial.print("Volume type is:    FAT");
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    
  volumesize *= volume.clusterCount();       
  volumesize /= 2;                           
  Serial.print("Volume size (KB):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (MB):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (GB):  ");
  Serial.println((float)volumesize / 1024.0);

  Serial.println("\nFiles found on the card:");
  root.openRoot(&volume);
  root.ls(LS_R | LS_DATE | LS_SIZE);
  root.close();
}

void loop(void) {
}