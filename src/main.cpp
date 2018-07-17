#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <FS.h>

// wifi connect settings
const char *ssid = "SSID-NAME";
const char *pass = "SOMERANDOMPASSWORD";

// wifi accesspoint settings
const char *apSsid = "JalousieAP";
const char *apPass = "SimplePassword";

// true = create access point after connection timeout
// false = keep trying to connect to given wifi
const bool wifiApFallback = true;

// time in seconds before we create an access point
// only necessary if wifiApFallback = true
const int wifiConnectionTimeout = 10;

// misc settings
const int serialBoud = 115200;
const int httpServerPort = 80;

int pin[2] = {D2, D3};

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(httpServerPort);
File fsUploadFile;

void initializeSerial();
void initializeWifi();
void initializeWebserver();
void initializeJalousieControll();


void addBootMessage();
void setupWifiConnection();
IPAddress createAccessPoint();
void createWebserverStructure();

void uploadPageHandler();
void jalousieHandler();
void simpleUploadPage();
void simpleIndexPage();
void sendHttpNotFound();

void handleFileUpload();
void handleFileDelete();
void handleFileList();
bool handleFileRead(String path);
void handleNotPreDefined();

String addLeadingSlash(String fileName);
String getContentType(String filename);
void simpleFileListPage(String path);

void jalousieDirection();
void disableJalousie(int pin);
void enableJalousie(int pin);

void firstJalousieStep();
void secondJalousieStep();
int getPinStateHelper(int pin);

void startJalousieAction();

void setDirection(String newDirection);
String getDirection();
void setTotalDuration(int duration);
int getTotalDuration();

unsigned long startMillis = 0;
unsigned long totalDuration = 28000;
bool active = false;
bool stopForced = false;
String direction = "down";

void setStartMillis() {
    startMillis = millis();
}

unsigned long getStartMillis() {
    return startMillis;
}

void setDirection(String newDirection) {
    direction = newDirection;
}

String getDirection() {
    return direction;
}

void setTotalDuration(int duration) {
    totalDuration = duration;
}

int getTotalDuration() {
    return totalDuration;
}

void setPinState(int pinId, int value) {
    pin[pinId] = value;
}

int getPinState(int pinId) {
    return pin[pinId];
}

void setActive(bool state) {
    active = state;
}

bool isActive() {
    return active;
}

void setStopForced(bool forced) {
    stopForced = forced;
}

bool isStopForced() {
    return stopForced;
}

void setup() {
  initializeSerial();
  initializeWifi();
  initializeWebserver();
  initializeJalousieControll();

  Serial.println();
  Serial.println("######################");
  Serial.println("# System initialization done #");
  Serial.println("######################");
}

void loop() {
    httpServer.handleClient();
    if(isActive()) {
        jalousieDirection();
    }
}


void initializeSerial(){
    Serial.begin(serialBoud);

    // we want to wait a bit for serial
    delay(10);
    Serial.println();

    // add our custom boot message :)
    addBootMessage();
}


void addBootMessage(){
    Serial.println("------------------------");
    Serial.println("Booting LightningOS 2017");
    Serial.println("------------------------");
    Serial.println();
}

void initializeWifi() {
    Serial.println("##############");
    Serial.println("# Initializing Wifi #");
    Serial.println("##############");

    Serial.printf("Connecting to %s", ssid);
    Serial.println();

    setupWifiConnection();

    Serial.println("Initializing Wifi: Successful");
    Serial.println("####################");
    Serial.println();
}

void setupWifiConnection() {
    WiFi.begin(ssid, pass);
    Serial.print("Connecting: ");

    // time in milliseconds
    int waitTimeout = 500;
    bool tryToConnect = true;
    int i = 0;

    // connect to given wifi
    while (WiFi.status() != WL_CONNECTED && tryToConnect) {
        if (wifiApFallback && i >= (wifiConnectionTimeout * 2)) {
            tryToConnect = false;
        }
        delay(waitTimeout);
        Serial.print(".");
        i++;
    }

    IPAddress clientIp = WiFi.localIP();

    // create access point if wanted
    if (WiFi.status() != WL_CONNECTED && wifiApFallback) {
        clientIp = createAccessPoint();
    }

    Serial.println("\n");
    Serial.printf("Device-IP: %s", clientIp.toString().c_str());
    Serial.println("\n");
}

IPAddress createAccessPoint() {
    Serial.println();
    Serial.printf("ERROR: Could not connect to %s!", ssid);
    Serial.println();
    Serial.println("--------------------------");
    Serial.println();
    Serial.println("Creating new access point");
    Serial.println();
    Serial.printf("AP-SSID: %s", apSsid);
    Serial.println();
    Serial.printf("AP-KEY: %s", apPass);
    Serial.println();

    // actually create the new AP
    WiFi.softAP(apSsid, apPass);

    Serial.println();
    Serial.println("New Access Point created!");

    return WiFi.softAPIP();
}

void initializeWebserver() {

    Serial.println("###################");
    Serial.println("# Initializing Webserver #");
    Serial.println("###################");

    // adds OTA functionality YAY
    httpUpdater.setup(&httpServer); // http://123.456.789.012/update

    SPIFFS.begin();

    createWebserverStructure();

    // start the actual web-server
    httpServer.begin();
}

void createWebserverStructure() {
    httpServer.on("/version", HTTP_GET, []() {
        httpServer.send(200, "text/plain", "0.0.2");
    });
    httpServer.on("/restart", HTTP_GET, []() {
        httpServer.send(200, "text/plain", "rebooting");
        ESP.restart();
    });
    httpServer.on("/upload", HTTP_GET, []() {uploadPageHandler();});
    httpServer.on("/upload", HTTP_POST, []() {
      httpServer.send(200);
    }, handleFileUpload);

    httpServer.on("/jalousie", HTTP_GET, []() {
      jalousieHandler();
      httpServer.sendHeader("Location", "/index.html",true); //Redirect to our html web page
      httpServer.send(302, "text/plain","");
    });

    httpServer.on("/stop", HTTP_GET, [](){
        setStopForced(true);
    });


    httpServer.on("/delete", HTTP_GET, handleFileDelete);
    httpServer.on("/list", HTTP_GET, handleFileList);

    httpServer.onNotFound(handleNotPreDefined);
}

void uploadPageHandler() {
    if (httpServer.hasArg("default") || !handleFileRead("/upload.html")) {
        simpleUploadPage();
    }
}

void handleNotPreDefined() {
    String uri = httpServer.uri();

    if (!handleFileRead(httpServer.uri())) {
        if (uri == "/" || uri == "/index.html") {
            simpleIndexPage();
        } else {
            sendHttpNotFound();
        }
    }
}

void sendHttpNotFound() {
    httpServer.send(404, "text/plain", "404: Not Found");
}

void handleFileUpload() {
    HTTPUpload& upload = httpServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String fileName = addLeadingSlash(upload.filename);
        fsUploadFile = SPIFFS.open(fileName, "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        String returnMessage = "error";
        if (fsUploadFile) {
            fsUploadFile.close();
            returnMessage = "success";
        }

        httpServer.sendHeader("Location", "/upload?message=" + returnMessage);
        httpServer.send(303);
    }
}

bool handleFileRead(String path) {
    if (path.endsWith("/")) {
        path += "index.html";
    };
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz)) {
            path += ".gz";
        }
        File file = SPIFFS.open(path, "r");
        httpServer.streamFile(file, getContentType(path));
        file.close();

        return true;
    }

    return false;
}

void handleFileList() {
    if (!httpServer.hasArg("dir")) {
        httpServer.send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = httpServer.arg("dir");
    simpleFileListPage(path);
}

void handleFileDelete() {
    if (httpServer.args() == 0) {
        return httpServer.send(500, "text/plain", "BAD ARGS");
    }

    String path = httpServer.arg(0);

    if (path == "/") {
        return httpServer.send(500, "text/plain", "BAD PATH");
    }

    if (!SPIFFS.exists(addLeadingSlash(path))) {
        return httpServer.send(404, "text/plain", "FileNotFound");
    }

    SPIFFS.remove(addLeadingSlash(path));
    httpServer.send(200, "text/plain", "File deleted");
}

String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes / 1024.0) + "KB";
    } else if (bytes < (1024 * 1024 * 1024)) {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    } else {
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
    }
}

String addLeadingSlash(String fileName) {
    if (!fileName.startsWith("/")) {
        fileName = "/" + fileName;
    }

    return fileName;
}

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".gz")) return "application/x-gzip";

    return "text/plain";
}

void simpleIndexPage() {
    String html =
            "<html><head><title>LightningOS | by Kai Grassnick</title></head>";
    html += "<body style='background-color:gray'>";
    html += "<a href='/upload'>Upload</a><br/>";
    html += "<a href='/update'>Update</a><br/>";
    html += "</body>";
    html += "</html>";

    httpServer.send(200, "text/html", html);
}

void simpleUploadPage() {
    String html = "<form method=\"post\" enctype=\"multipart/form-data\">";
    html += "<input type=\"file\" name=\"name\">";
    html += "<input class=\"button\" type=\"submit\" value=\"Upload\">";
    html += "</form>";

    httpServer.send(200, "text/html", html);
}


void simpleFileListPage(String path){

    String output = "[";

  Dir dir = SPIFFS.openDir(path);

    while (dir.next()) {
        File entry = dir.openFile("r");
        if (output != "[") output += ',';
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\",\"size\":\"";
        output += String(entry.size()).substring(1);
        output += "\"}";
        entry.close();
    }

    output += "]";
    httpServer.send(200, "text/json", output);
}

void initializeJalousieControll() {
    Serial.println("###################");
    Serial.println("# Initializing Controll pins #");
    Serial.println("###################");

    pinMode(D0, OUTPUT); // Main power J1
    pinMode(D1, OUTPUT); // Main power J2
    pinMode(D2, OUTPUT); // direction J1
    pinMode(D3, OUTPUT); // direction J2

    digitalWrite(D0, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
    digitalWrite(D3, HIGH);
}

void jalousieHandler(){
    setPinState(0, D2);
    setPinState(1, D3);
    
  if (httpServer.arg("target") == "door") {
        setPinState(1, -1);
  } else if (httpServer.arg("target") == "window") {
        setPinState(0, -1);
  }

  if (httpServer.hasArg("duration")) {
    int formDuration  = httpServer.arg("duration").toInt();
    if (formDuration == 33) {
      setTotalDuration(9333);
    } else if (formDuration == 50) {
      setTotalDuration(14000);
    }
  }


  if (httpServer.hasArg("direction")) {
        setDirection(httpServer.arg("direction"));
        startJalousieAction();
  }
}

void startJalousieAction() {
    setActive(true);

    setStartMillis();
    firstJalousieStep();
    jalousieDirection();
}

void jalousieDirection(){
    if(millis() - getStartMillis() >= getTotalDuration() || isStopForced()) {
        secondJalousieStep();
    }
}


int getPinStateHelper(int pin) {
  uint8_t pinStates[] = {HIGH, HIGH};
  if (getDirection() == "down") {
    pinStates[0] = LOW;
    pinStates[1] = HIGH;
  }

    return pinStates[pin];
}


void firstJalousieStep(){
  for (int i = 0; i < 2; i++) {
    if (getPinState(i) != -1) {
            digitalWrite(getPinState(i), getPinStateHelper(0));
    }
  }

    delay(250);

    for (int i = 0; i < 2; i++) {
        if (getPinState(i) != -1) {
            enableJalousie(getPinState(i));
        }
    }
}

void secondJalousieStep(){
    for (int i = 0; i < 2; i++) {
    if (getPinState(i) != -1) {
      disableJalousie(getPinState(i));
    }
  }

    delay(250);

    for (int i = 0; i < 2; i++) {
        if (getPinState(i) != -1) {
            digitalWrite(getPinState(i), getPinStateHelper(1));
        }
    }

    setActive(false);
    setStopForced(false);
}


void disableJalousie(int pin){
  int ioPin = D0;
  if (pin == D3) {
    ioPin = D1;
  }
    digitalWrite(ioPin, HIGH);
}

void enableJalousie(int pin){
  int ioPin = D0;
  if (pin == D3) {
    ioPin = D1;
  }
    digitalWrite(ioPin, LOW);
}
