#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <algorithm>
#include <Updater.h> // Include library for OTA firmware flashing

// Custom SD library and ESPboy includes
#include <ESPboy_SD.h> 
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"

#define TFT_CS_MCP  CSTFTPIN  // 8 (B0)
#define SD_CS_MCP   15        // 15 (B7)

#define VISIBLE_LINES 13
#define HEADER_HEIGHT 11
#define LINE_HEIGHT   9

ESPboyInit myESPboy;
int8_t isDAC;

struct FileItem {
  String name;
  bool isDir;
  uint32_t size;
};

std::vector<FileItem> fileList;
String currentPath = "/";
int selectedIndex = 0;
int topIndex = 0;

// ============================================================================
//   SPI BUS ARBITRATION
// ============================================================================

void selectTFT() {
  myESPboy.mcp.digitalWrite(TFT_CS_MCP, LOW);
}

void deselectTFT() {
  myESPboy.mcp.digitalWrite(TFT_CS_MCP, HIGH);
}

// ============================================================================
//   WIFI MANAGER INCLUSION
// ============================================================================
// Include after selectTFT and deselectTFT definitions
#include "lib/WiFiFileManagerSD.h"

// ============================================================================
//   FILE SYSTEM DIRECTORY HANDLING
// ============================================================================

void loadDirectory(String path) {
  fileList.clear();
  selectedIndex = 0;
  topIndex = 0;

  // Add "Up" navigation item if not in root
  if (path != "/") {
    FileItem upItem;
    upItem.name = "..";
    upItem.isDir = true;
    upItem.size = 0;
    fileList.push_back(upItem);
  }

  deselectTFT(); 
  // ИСПОЛЬЗУЕМ ESPboySDLib::File
  ESPboySDLib::File dir = ESPboySDLib::ESPboy_SD.open(path.c_str());

  if (dir && dir.isDirectory()) {
    dir.rewindDirectory();
    while (true) {
      // ИСПОЛЬЗУЕМ ESPboySDLib::File
      ESPboySDLib::File entry = dir.openNextFile();
      if (!entry) break;

      FileItem item;
      item.name = String(entry.name());
      item.isDir = entry.isDirectory();
      item.size = entry.size();
      entry.close();

      // Convert to uppercase for reliable FAT 8.3 short name comparison
      String checkName = item.name;
      checkName.toUpperCase();

      // Check for hidden or system files/folders (including macOS remnants)
      bool skipFile = false;
      if (checkName.startsWith(".")) skipFile = true;             // Mac/Linux hidden (.DS_Store, .Trash)
      else if (checkName.startsWith("~")) skipFile = true;        // Windows temp files
      else if (checkName.startsWith("SYSTEM~")) skipFile = true;  // Windows "System Volume Information"
      else if (checkName.startsWith("__MAC")) skipFile = true;    // macOS "__MACOSX" resource fork folder
      else if (checkName.startsWith("FSEVEN~")) skipFile = true;  // macOS ".fseventsd"
      else if (checkName.startsWith("SPOTLI~")) skipFile = true;  // macOS ".Spotlight-V100"
      else if (checkName.startsWith("TRASH~")) skipFile = true;   // Корзина macOS/Linux в формате FAT 8.3
      else if (checkName == "THUMBS.DB") skipFile = true;         // Windows thumbnail cache
      else if (checkName == "TRASHES") skipFile = true;           // macOS Trashes folder
      else if (checkName == "WPSETT~1") skipFile = true;          // Windows Phone/Indexer settings

      // Only add to list if it's not a system/hidden file
      if (!skipFile) {
        fileList.push_back(item);
      }
    }
    dir.close();
  }

  // Sort: ".." first, then folders, then files (alphabetically)
  std::sort(fileList.begin(), fileList.end(), [](const FileItem &a, const FileItem &b) {
    if (a.name == "..") return true;
    if (b.name == "..") return false;
    if (a.isDir != b.isDir) return a.isDir > b.isDir;
    return a.name < b.name;
  });
}

// ============================================================================
//   FIRMWARE FLASHING (OTA FROM SD)
// ============================================================================

void flashFirmware(String filepath, String filename) {
  selectTFT();
  myESPboy.tft.fillScreen(TFT_BLACK);
  
  // Prompt user
  myESPboy.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  myESPboy.tft.drawString("Flash firmware?", 10, 30);
  myESPboy.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Truncate long filenames for display
  String dispName = filename;
  if (dispName.length() > 18) dispName = dispName.substring(0, 16) + "..";
  myESPboy.tft.drawString(dispName, 10, 40);
  
  myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
  myESPboy.tft.drawString("A-YES / B-NO", 28, 70);

  // Wait until all buttons are released
  while (myESPboy.getKeys()) delay(10);

  // Wait for user choice
  bool doFlash = false;
  while (true) {
    uint8_t k = myESPboy.getKeys();
    if (k & PAD_ACT) { doFlash = true; break; }
    if (k & PAD_ESC) { doFlash = false; break; }
    delay(10);
  }

  while (myESPboy.getKeys()) delay(10);

  // If user cancelled, redraw browser and exit
  if (!doFlash) {
    drawBrowserFull();
    return;
  }

  // Start flashing process
  deselectTFT();
  
  // ИСПОЛЬЗУЕМ ESPboySDLib::File
  ESPboySDLib::File f = ESPboySDLib::ESPboy_SD.open(filepath.c_str(), FILE_READ);
  
  if (!f) {
    selectTFT();
    myESPboy.tft.fillScreen(TFT_BLACK);
    myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    myESPboy.tft.drawString("Error opening file!", 10, 50);
    delay(2000);
    drawBrowserFull();
    return;
  }

  size_t fileSize = f.size();
  
  // Check if there is enough space for the update
  if (!Update.begin(fileSize, U_FLASH)) {
    selectTFT();
    myESPboy.tft.fillScreen(TFT_BLACK);
    myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    myESPboy.tft.drawString("Not enough space!", 10, 40);
    myESPboy.tft.drawString("Err: " + String(Update.getError()), 10, 60);
    delay(3000);
    deselectTFT();
    f.close();
    drawBrowserFull();
    return;
  }

  selectTFT();
  myESPboy.tft.fillScreen(TFT_BLACK);
  myESPboy.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  myESPboy.tft.drawString("Uploading...", 10, 40);
  
  // Draw progress bar outline
  myESPboy.tft.drawRect(14, 60, 100, 10, TFT_WHITE);

  uint8_t buf[256];
  size_t written = 0;
  int lastProgress = -1;

  // Flash chunk by chunk
  while (f.available()) {
    deselectTFT(); // Release bus for SD read
    size_t len = f.read(buf, sizeof(buf));
    
    // Write to internal flash
    Update.write(buf, len);
    written += len;

    // Calculate progress (0 to 96 pixels wide)
    int progress = (written * 96) / fileSize;
    if (progress > lastProgress) {
      selectTFT(); // Take bus for TFT draw
      myESPboy.tft.fillRect(16, 62, progress, 6, TFT_GREEN);
      lastProgress = progress;
    }
  }

  deselectTFT();
  f.close();
  bool success = Update.end();

  // Show result
  selectTFT();
  myESPboy.tft.fillScreen(TFT_BLACK);
  
  if (success) {
    myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    myESPboy.tft.drawString("Flashing,", 5, 40);
    myESPboy.tft.drawString("please wait...", 5, 50);
    myESPboy.tft.drawString("Then reboot.", 5, 70);
    delay(1500);
    ESP.restart(); // Restart the board into new firmware
  } else {
    myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    myESPboy.tft.drawString("Update Failed!", 10, 40);
    myESPboy.tft.drawString("Err: " + String(Update.getError()), 10, 60);
    delay(3000);
    drawBrowserFull();
  }
}

// ============================================================================
//   SMART GUI REDRAW
// ============================================================================

void drawLine(int itemIdx, int screenLine) {
  selectTFT();
  int y = HEADER_HEIGHT + 2 + (screenLine * LINE_HEIGHT);
  
  myESPboy.tft.fillRect(0, y - 1, 128, LINE_HEIGHT, TFT_BLACK);

  if (itemIdx >= (int)fileList.size()) return;

  bool isSelected = (itemIdx == selectedIndex);

  if (isSelected) {
    myESPboy.tft.fillRect(0, y - 1, 128, LINE_HEIGHT, TFT_DARKCYAN);
    myESPboy.tft.setTextColor(TFT_YELLOW, TFT_DARKCYAN);
  } else {
    if (fileList[itemIdx].isDir) myESPboy.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    else myESPboy.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  String nameStr = fileList[itemIdx].name;
  if (fileList[itemIdx].isDir && nameStr != "..") nameStr = "[" + nameStr + "]";
  if (nameStr.length() > 20) nameStr = nameStr.substring(0, 18) + "..";

  myESPboy.tft.drawString(nameStr, 4, y);
}

void drawBrowserFull() {
  selectTFT();
  myESPboy.tft.fillScreen(TFT_BLACK);

  myESPboy.tft.fillRect(0, 0, 128, HEADER_HEIGHT, TFT_NAVY);
  myESPboy.tft.setTextColor(TFT_WHITE, TFT_NAVY);
  myESPboy.tft.setTextSize(1);

  String displayPath = currentPath;
  if (displayPath.length() > 20) {
    displayPath = "..." + displayPath.substring(displayPath.length() - 17);
  }
  myESPboy.tft.drawString(displayPath, 2, 2);

  if (fileList.empty()) {
    myESPboy.tft.setTextColor(TFT_SILVER, TFT_BLACK);
    myESPboy.tft.drawString("<Empty Folder>", 25, 60);
    return;
  }

  for (int i = 0; i < VISIBLE_LINES; i++) {
    drawLine(topIndex + i, i);
  }
}

// Updated cursor logic: now checks if topIndex changed to trigger full list redraw
void updateCursor(int oldIdx, int newIdx, int oldTop, int newTop) {
  if (oldTop != newTop) {
    // Scrolling occurred, redraw all visible lines to shift them
    for (int i = 0; i < VISIBLE_LINES; i++) {
      drawLine(newTop + i, i);
    }
  } else {
    // No scrolling, smart redraw of just the two affected lines
    drawLine(oldIdx, oldIdx - newTop);
    drawLine(newIdx, newIdx - newTop);
  }
}

// ============================================================================
//   NAVIGATION
// ============================================================================

void goUpDir() {
  if (currentPath == "/") return;
  int lastSlash = currentPath.lastIndexOf('/');
  if (lastSlash == 0) currentPath = "/";
  else currentPath = currentPath.substring(0, lastSlash);
  
  loadDirectory(currentPath);
  drawBrowserFull();
}

void goRootDir() {
  if (currentPath == "/") return;
  currentPath = "/";
  
  loadDirectory(currentPath);
  drawBrowserFull();
}

// ============================================================================
//   SETUP
// ============================================================================

void setup() {
  myESPboy.begin("SD Browser & Flash");
  isDAC = myESPboy.mcp.writeDAC(4096, false);

  selectTFT();
  myESPboy.tft.fillScreen(TFT_BLACK);
  myESPboy.tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  myESPboy.tft.drawString("Mounting SD...", 25, 55);

  deselectTFT(); 

  if (!ESPboySDLib::ESPboy_SD.begin(SD_CS_MCP, &myESPboy.mcp)) {
    selectTFT();
    myESPboy.tft.fillScreen(TFT_BLACK);
    myESPboy.tft.setTextColor(TFT_RED, TFT_BLACK);
    myESPboy.tft.drawString("SD Init Failed!", 18, 50);
    while (true) delay(100);
  }

  loadDirectory(currentPath);
  drawBrowserFull();
}

// ============================================================================
//   LOOP
// ============================================================================

void loop() {
  static uint32_t lastBtnTime = 0;
  uint8_t keys = myESPboy.getKeys();

  if (keys && (millis() - lastBtnTime > 130)) {
    lastBtnTime = millis();
    int oldSelectedIndex = selectedIndex;
    int oldTopIndex = topIndex;

    // DOWN
    if (keys & PAD_DOWN) {
      if (selectedIndex < (int)fileList.size() - 1) {
        selectedIndex++;
        if (selectedIndex >= topIndex + VISIBLE_LINES) topIndex++;
        updateCursor(oldSelectedIndex, selectedIndex, oldTopIndex, topIndex);
      }
    }

    // UP
    if (keys & PAD_UP) {
      if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < topIndex) topIndex--;
        updateCursor(oldSelectedIndex, selectedIndex, oldTopIndex, topIndex);
      }
    }

    // LEFT - Quick return to root
    if (keys & PAD_LEFT) {
      goRootDir();
    }

    // RIGHT - Launch Wi-Fi File Manager
    if (keys & PAD_RIGHT) {
      WiFiSDFileManager(); 
      // ESP resets upon exiting WiFiSDFileManager
    }

    // ACT (A) - Enter folder or interact with file
    if (keys & PAD_ACT) {
      if (!fileList.empty()) {
        String itemName = fileList[selectedIndex].name;
        
        if (itemName == "..") {
          goUpDir();
        } 
        else if (fileList[selectedIndex].isDir) {
          if (currentPath.endsWith("/")) currentPath += itemName;
          else currentPath += "/" + itemName;
          loadDirectory(currentPath);
          drawBrowserFull();
        } 
        else {
          // File selected - check if it's a binary file
          String lowerName = itemName;
          lowerName.toLowerCase();
          
          if (lowerName.endsWith(".bin")) {
            String fullPath = currentPath;
            if (!fullPath.endsWith("/")) fullPath += "/";
            fullPath += itemName;
            
            flashFirmware(fullPath, itemName);
          } else {
            // General file action (just beep for now)
            myESPboy.playTone(300, 100);
          }
        }
      }
    }

    // ESC (B) - Go up one directory level
    if (keys & PAD_ESC) {
      goUpDir();
    }
  }

  delay(10);
}
