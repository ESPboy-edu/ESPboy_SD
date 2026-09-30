// Wi-Fi SD File Manager for ESPboy (PROGMEM Optimized)
// Адаптация для ESPboy_SDlib и расширителя MCP23017

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESPboy_SD.h>
#include <uri/UriBraces.h>

#define APSSID "ESPboy"
#define APHOST "espboy"
#define APPSK  "87654321"

const char *ssid = APSSID;
const char *password = APPSK;
const char *host = APHOST;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

const char html_back_P[] PROGMEM = "<html><head><meta http-equiv=\"refresh\" content=\"1; URL=/\" /></head><body></body></html>";

// ИСПОЛЬЗУЕМ ESPboyFile вместо File
ESPboyFile file_upload;

extern void selectTFT();
extern void deselectTFT();

void handleRoot() {
  Serial.println(F("root requested, generating index"));
  myESPboy.myLED.setRGB(0, 255, 0);
  myESPboy.myLED.on();

  String output;
  output.reserve(2560); 

  output = F("<html><head>");
  output += F("<title>ESPboy SD file manager</title>");
  output += F("<meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\" />");
  output += F("<meta http-equiv=\"Pragma\" content=\"no-cache\" />");
  output += F("<meta http-equiv=\"Expires\" content=\"0\" />");
  output += F("</head><body>");
  output += F("<h2>ESPboy SD file manager</h2>");
  output += F("<b>Upload a file:</b><br/><br/>");
  output += F("<form action=\"up\" method=\"post\" enctype=\"multipart/form-data\" onsubmit=\"reftimeout()\">");
  output += F("<input type=\"file\" name=\"file\" id=\"file\" /><input type=\"submit\" name=\"submit\" value=\"OK\" />");
  output += F("</form><br/><b>Files on SD card:</b>");
  output += F("<br/><br/><button onclick=\"window.location.reload();\">Refresh</button><br/><br/>");
  output += F("<table cellpadding=\"8\" border=\"1\">");
  output += F("<tr><th>#</th><th>Name</th><th>Size</th><th>Actions</th></tr>");

  deselectTFT(); 
  // ИСПОЛЬЗУЕМ ESPboyFile
  ESPboyFile root = ESPboySDLib::ESPboy_SD.open("/");
  int file_num = 1;

  if (root) {
    root.rewindDirectory();
    while (true) {
      // ИСПОЛЬЗУЕМ ESPboyFile
      ESPboyFile entry = root.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        String fname = String(entry.name());
        output += F("<tr><td>");
        output += String(file_num++);
        output += F("</td><td>");
        output += fname;
        output += F("</td><td>");
        output += String(entry.size());
        output += F(" bytes</td><td><a href=\"dl/");
        output += fname;
        output += F("\">download</a> | <a href=\"rm/");
        output += fname;
        output += F("\" onclick=\"return rmconfirm()\">delete</a></td></tr>");
      }
      entry.close();
    }
    root.close();
  }
  selectTFT(); 

  output += F("</table>");
  output += F("<script>");
  output += F("function reftimeout() { setTimeout(function(){window.location.reload();},500); }");
  output += F("function rmconfirm() { return confirm('Sure to delete?'); }");
  output += F("</script></body></html>");

  server.send(200, "text/html", output);
  myESPboy.myLED.off();
}

void handleFileDelete() {
  myESPboy.myLED.setRGB(255, 0, 0);
  myESPboy.myLED.on();
  
  String path = server.pathArg(0);
  if (!path.startsWith("/")) path = "/" + path;
  
  Serial.print(F("delete requested for "));
  Serial.println(path);

  deselectTFT();
  if (!ESPboySDLib::ESPboy_SD.exists(path.c_str())) {
    selectTFT();
    server.send(404, "text/plain", F("404 File Not Found"));
  } else {
    ESPboySDLib::ESPboy_SD.remove(path.c_str());
    selectTFT();
    server.send_P(301, "text/html", html_back_P);
  }
  myESPboy.myLED.off();
}

String urldecode(String url) {
  String output = "";
  int ptr = 0;
  while (ptr < url.length()) {
    char c = url.charAt(ptr++);
    switch (c) {
      case '+': output += ' '; break;
      case '%': {
        char d = 0;
        for (int j = 0; j < 2; ++j) {
          char h = url.charAt(ptr++);
          char n = 0;
          if (h >= '0' && h <= '9') n = h - '0';
          if (h >= 'a' && h <= 'f') n = h - 'a' + 10;
          if (h >= 'A' && h <= 'F') n = h - 'F' + 10;
          d = (d << 4) | n;
        }
        output += d;
      } break;
      default: output += c;
    }
  }
  return output;
}

void handleFileDownload() {
  myESPboy.myLED.setRGB(255, 255, 0);
  myESPboy.myLED.on();
  
  String path = urldecode(server.pathArg(0));
  if (!path.startsWith("/")) path = "/" + path;
  
  Serial.print(F("download requested for "));
  Serial.println(path);

  deselectTFT();
  if (!ESPboySDLib::ESPboy_SD.exists(path.c_str())) {
    selectTFT();
    server.send(404, "text/plain", F("404 File Not Found"));
  } else {
    // ИСПОЛЬЗУЕМ ESPboyFile
    ESPboyFile f = ESPboySDLib::ESPboy_SD.open(path.c_str(), FILE_READ);
    if (f) {
      int file_size = f.size();
      selectTFT(); 
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.send(200, "application/octet-stream", "");

      unsigned char buf[1024];
      while (file_size > 0) {
        int block_size = (file_size > (int)sizeof(buf)) ? sizeof(buf) : file_size;
        
        deselectTFT(); 
        f.read(buf, block_size);
        selectTFT();   

        server.sendContent((const char*)buf, block_size);
        file_size -= block_size;
      }
      deselectTFT();
      f.close();
      selectTFT();
    } else {
      selectTFT();
      server.send(500, "text/plain", F("500 Internal Server Error"));
    }
  }
  myESPboy.myLED.off();
}

void handleFileUpload() {
  myESPboy.myLED.setRGB(0, 0, 255);
  myESPboy.myLED.on();
  HTTPUpload& upload = server.upload();

  switch (upload.status) {
    case UPLOAD_FILE_START: {
      String filename = upload.filename;
      if (!filename.startsWith("/")) filename = "/" + filename;
      
      Serial.print(F("upload requested for "));
      Serial.println(filename);

      deselectTFT();
      if (ESPboySDLib::ESPboy_SD.exists(filename.c_str())) {
        ESPboySDLib::ESPboy_SD.remove(filename.c_str()); 
      }
      file_upload = ESPboySDLib::ESPboy_SD.open(filename.c_str(), FILE_WRITE);
      selectTFT();
      break;
    }
    case UPLOAD_FILE_WRITE: {
      if (file_upload) {
        deselectTFT();
        file_upload.write(upload.buf, upload.currentSize);
        selectTFT();
      }
      break;
    }
    case UPLOAD_FILE_END: {
      if (file_upload) {
        deselectTFT();
        file_upload.close();
        selectTFT();
        Serial.println(F("upload finished"));
      }
      break;
    }
  }
  myESPboy.myLED.off();
}

void serverSetup() {
  Serial.println();
  Serial.print(F("Configuring access point..."));
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(myIP);
  
  server.on("/", handleRoot);
  server.on(UriBraces("/rm/{}"), handleFileDelete);
  server.on(UriBraces("/dl/{}"), handleFileDownload);
  server.on("/up", HTTP_POST, []() {
    server.send_P(200, "text/html", html_back_P);
  }, handleFileUpload);
  
  MDNS.begin(host);
  httpUpdater.setup(&server);
  server.begin();
  MDNS.addService("http", "tcp", 80);
  delay(50);
  Serial.println(F("HTTP server started"));
}

void serverLoop() {
  server.handleClient();
}

void WiFiSDFileManager() {
  WiFi.forceSleepWake();
  serverSetup();

  selectTFT();
  myESPboy.tft.fillScreen(0x0000);
  myESPboy.tft.setTextSize(1);
  myESPboy.tft.setTextColor(0xffff);
  myESPboy.tft.setCursor(0, 10);
  myESPboy.tft.print(F("\n\n SSID "));
  myESPboy.tft.print(F(APSSID));
  myESPboy.tft.print(F("\n Password "));
  myESPboy.tft.print(F(APPSK));
  myESPboy.tft.print(F("\n\n Go to \n http://192.168.4.1"));
  myESPboy.tft.print(F("\n in a web browser"));
  myESPboy.tft.print(F("\n\n Press any button to\n reboot"));

  Serial.print(F("FreeHeap:"));
  Serial.println(ESP.getFreeHeap());
  delay(1000);
  
  while (1) {
    serverLoop();
    if(myESPboy.getKeys()) ESP.reset();
    delay(10); 
  }
}