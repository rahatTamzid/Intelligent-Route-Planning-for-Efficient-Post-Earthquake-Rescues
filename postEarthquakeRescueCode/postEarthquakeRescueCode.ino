
#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#elif __has_include(<WiFiNINA.h>)
#include <WiFiNINA.h>
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#elif __has_include(<WiFiS3.h>)
#include <WiFiS3.h>
#endif
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// For Wifi and Firebase Auth
#define WIFI_SSID "Cockpit"
#define WIFI_PASSWORD "print2020"
#define API_KEY "zzzzzzzzzzzz"
#define DATABASE_URL "zzzzzzzzzzzfirebaseio.com"

// Firebase Auth

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <MS5611.h>

// For Real Time
#include <NTPClient.h>
#include <WiFiUdp.h>

//Setting Time zone
// Define NTP parameters
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 6 * 3600);  // GMT+6 offset


int buzzer = D0;
int cancel_pin = D6;  // Initializing Arduino Pin

//TFT LCD PIN
int Reading;

#define TFT_CS 15
#define TFT_RST 0
#define TFT_DC 2
//

//For sensors and LCD
MS5611 ms5611;
double referencePressure;
int people_count;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MPU6050 mpu(Wire);
const int bufferSize = 50;
int xBuffer[bufferSize];
int yBuffer[bufferSize];
int zBuffer[bufferSize];
int altitudeBuffer[bufferSize];
int bufferIndex = 0;
const int threshold1 = 100;  // Adjust this threshold based on your requirements
const int threshold2 = 150;


//


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long dataMillis = 0;
int count = 0;
bool signupOK = false;
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif





// Setup Function

void setup() {
  Serial.begin(115200);
  //For Sensor and display
  Wire.begin();
  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  while (status != 0) {}  // stop everything if could not connect to MPU6050
  Serial.println(F("Calculating offsets, do not move MPU6050"));
  delay(1000);
  mpu.calcOffsets();  // gyro and accelero
  Serial.println("Done!\n");
  while (!ms5611.begin()) {
    Serial.println("Could not find a valid MS5611 sensor, check wiring!");
    delay(500);
  }
  referencePressure = ms5611.readPressure();
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(3);
  tft.setTextWrap(false);
  tft.setCursor(20, 0);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2);
  tft.println("SEISMO-NAV");

  // End setup for sensor and display

  pinMode(cancel_pin, INPUT);  // Declaring Arduino Pin as an Input

  //Setup For Time
  timeClient.begin();
  //Setup For firebase
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  multi.addAP(WIFI_SSID, WIFI_PASSWORD);
  multi.run();
#else
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif
  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (millis() - ms > 10000)
      break;
#endif
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  config.wifi.clearAP();
  config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  Serial.print("Sign up new user... ");
  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  String mac = WiFi.macAddress();
  String path = "/nodedata/" + mac;
  Serial.printf("Set Node Mac... %s\n", Firebase.RTDB.setString(&fbdo, path, mac) ? "ok" : fbdo.errorReason().c_str());





  // Firebase Setup Ends

  //Buzzer setup
  pinMode(buzzer, OUTPUT);
}







// Draw Graph Function

void updateGraph(int newX, int newY, int newZ) {
  xBuffer[bufferIndex] = newX;
  yBuffer[bufferIndex] = newY;
  zBuffer[bufferIndex] = newZ;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  int xScale = tft.width() / bufferSize;
  int xOffset = 0;

  // Draw X-axis
  for (int i = 0; i < bufferSize - 1; ++i) {
    int x0 = xOffset + i * xScale;
    int y0 = map(xBuffer[i], -90, 90, tft.height(), 0) + 20;
    int x1 = xOffset + (i + 1) * xScale;
    int y1 = map(xBuffer[i + 1], -90, 90, tft.height(), 0) + 20;
    tft.drawLine(x0, y0, x1, y1, ST77XX_RED);
  }

  // Draw Y-axis
  for (int i = 0; i < bufferSize - 1; ++i) {
    int x0 = xOffset + i * xScale;
    int y0 = map(yBuffer[i], -90, 90, tft.height(), 0);
    int x1 = xOffset + (i + 1) * xScale;
    int y1 = map(yBuffer[i + 1], -90, 90, tft.height(), 0);
    tft.drawLine(x0, y0, x1, y1, ST77XX_GREEN);
  }

  // Draw Z-axis
  for (int i = 0; i < bufferSize - 1; ++i) {
    int x0 = xOffset + i * xScale;
    int y0 = map(zBuffer[i], -90, 90, tft.height(), 0) - 20;
    int x1 = xOffset + (i + 1) * xScale;
    int y1 = map(zBuffer[i + 1], -90, 90, tft.height(), 0) - 20;
    tft.drawLine(x0, y0, x1, y1, ST77XX_BLUE);
  }
  // Draw preceding black 'boxes' to erase old plot lines
  int eraseX0 = xOffset + ((bufferIndex - 1 + bufferSize) % bufferSize) * xScale;
  int eraseX1 = eraseX0 + 3*xScale;
  int eraseY0 = 0;
  int eraseY1 = tft.height();

  tft.fillRect(eraseX0, eraseY0, eraseX1 - eraseX0, eraseY1 - eraseY0, ST7735_BLACK);
}




void WarningMessage() {


  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(30, 5);
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.println("WARNING !");
  tft.setCursor(20, tft.height() - 80);
  tft.setTextColor(ST77XX_RED, ST7735_BLACK);
  tft.setTextSize(1);
  tft.print("Collapse Detected!");
  tft.setCursor(20, tft.height() - 60);
  tft.print("Wating For Rescue");
  tft.setCursor(20, tft.height() - 50);
  tft.print("Press cancel Within ");
  tft.setTextColor(ST77XX_WHITE, ST7735_BLACK);
  tft.setTextSize(2);

int count = 0;

  for (int r = 120; r > 0; r--) {
    Reading = digitalRead(cancel_pin);
    if (Reading == HIGH) {
      Serial.println("Request Cancelled ");
    tft.setCursor(20, tft.height() - 20);
      tft.setTextColor(ST7735_YELLOW);
      tft.setTextSize(1);
      tft.println("Cancelled !");
      tft.setCursor(20, tft.height() - 10);
      tft.setTextColor(ST77XX_WHITE, ST7735_BLACK);
      tft.setTextSize(1);
      tft.print("Continueing Reading");
      String mac = WiFi.macAddress();
      String Path = "/nodedata/" + mac;
      deleteNodeData(Path);
      delay(5000);
        r = 1;
    } else {
      tft.invertDisplay(true);
      tft.setCursor(40, tft.height() - 40);
      char timeCount[10];
      dtostrf(r, 3, 0, timeCount);
      tft.println(timeCount);
      tft.setCursor(90, tft.height() - 40);
      tft.print("Sec");
      delay(500);
      tft.invertDisplay(false);
      delay(500);
       count ++;
    }

 

  }
 
   
 while(count >119){
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(30, 5);
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.println("WARNING !");
  tft.setCursor(20, tft.height() - 80);
  tft.setTextColor(ST77XX_RED, ST7735_BLACK);
  tft.setTextSize(1);
  tft.print("Collapse Detected!");
  tft.setCursor(20, tft.height() - 60);
  tft.print("Wating For Rescue");
 }




    // Display countdown
  }




  //     // Display countdown
  //     tft.invertDisplay(true);
  //     tft.setCursor(40, tft.height() - 40);
  //     char timeCount[10];
  //     dtostrf(r, 3, 0, timeCount);
  //     tft.println(timeCount);
  //     tft.setCursor(90, tft.height() - 40);
  //     tft.print("Sec");
  //     // Buzzer sound
  //     for (int i = 200; i < 1200; i++) {
  //       delay(5);
  //       if (i == 700) {
  //         tft.invertDisplay(false);
  //       }
  //       tone(buzzer, i);
  //     }
  //     tft.invertDisplay(true);
  //     for (int i = 1200; i > 200; i--) {
  //       delay(5);
  //       if (i == 700) {
  //         tft.invertDisplay(false);
  //       }
  //       tone(buzzer, i);
  //     }
  //     tft.invertDisplay(true);

  //     // Reset buzzer
  //     noTone(buzzer);
  //   }
  // }




  void deleteNodeData(const String& nodePath) {
    FirebaseData fbdo;

    if (Firebase.RTDB.deleteNode(&fbdo, nodePath)) {
      Serial.println("Node data deleted successfully");
    } else {
      Serial.println("Error deleting node data");
      Serial.println(fbdo.errorReason());
    }
  }

  // Main Loop

  void loop() {
    //Determine People Count
    timeClient.update();
    // For Sensors And Display
    tft.setCursor(20, 0);            // Set position (x,y)
    tft.setTextColor(ST7735_WHITE);  // Set color of text. First is the color of text and after is color of background
    tft.setTextSize(2);              // Set text size. Goes from 0 (the smallest) to 20 (very big)
    tft.println("SEISMO-NAV");       // Print a text or value
    mpu.update();
    int xValue = mpu.getAngleX();
    int yValue = mpu.getAngleY();
    int zValue = mpu.getAngleZ();
    int16_t vectorSum = sqrt(xValue * xValue + yValue * yValue + zValue * zValue);

    // Altitude Code:
    uint32_t rawTemp = ms5611.readRawTemperature();
    uint32_t rawPressure = ms5611.readRawPressure();
    // Read true temperature & Pressure
    double realTemperature = ms5611.readTemperature();
    long realPressure = ms5611.readPressure();
    // Calculate altitude
    float absoluteAltitude = ms5611.getAltitude(realPressure);
    float relativeAltitude = abs(ms5611.getAltitude(realPressure, referencePressure));
    Reading = digitalRead(cancel_pin);
    updateGraph(xValue, yValue, zValue);
    char stringX[10];
    char stringY[10];
    char stringZ[10];
    char Vect[10];
    char altitude[10];
    dtostrf(xValue, 3, 0, stringX);
    dtostrf(yValue, 3, 0, stringY);
    dtostrf(zValue, 3, 0, stringZ);
    dtostrf(vectorSum, 3, 0, Vect);
    dtostrf(relativeAltitude, 3, 2, altitude);
    tft.setCursor(0, tft.height() - 10);
    tft.setTextColor(ST77XX_RED, ST7735_BLACK);
    tft.setTextSize(1);
    tft.print("X");
    tft.print(stringX);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    tft.print("Y");
    tft.setTextSize(1);
    tft.print(stringY);
    tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
    tft.print("Z");
    tft.setTextSize(1);
    tft.print(stringZ);
    tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);
    tft.print("V:");
    tft.setTextSize(1);
    tft.print(Vect);
    tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    tft.print("A");
    tft.setTextSize(1);
    tft.print(altitude);
    // End Sensors and Display
    // Condition For Alarm



    Reading = digitalRead(cancel_pin);

    if (Reading == HIGH) {
      Serial.println("HIGH");
    } else {
      Serial.println("LOW");
    }

    // Check Damage


    if (relativeAltitude > 3) {
      Serial.println("Crash Detected!");
      String mac = WiFi.macAddress();
      String Path = "/nodedata/" + mac;
      String UserPath = Path + "/node_id";
      String LatitudePath = Path + "/latitude";
      String LongitudePath = Path + "/longitude";
      String PeoplePath = Path + "/people_count";
      double latitude = 23.8116641;
      double longitude = 90.3570204;
      String cat1 = "A";
      String cat2 = "B";
      String catPath = Path + "/Damage_Category";
      // Get current hour
      int currentHour = timeClient.getHours();
      // Day and night conditions
      if (currentHour >= 6 && currentHour < 18) {  // Daytime (assumed 6 AM to 6 PM)
        people_count = 800;
      } else {  // Nighttime
        people_count = 500;
      }
      Serial.printf("Set latitude... %s\n", Firebase.RTDB.setDouble(&fbdo, LatitudePath, latitude) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set longitude... %s\n", Firebase.RTDB.setDouble(&fbdo, LongitudePath, longitude) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set People Count... %s\n", Firebase.RTDB.setInt(&fbdo, PeoplePath, people_count) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set Damage Category... %s\n", Firebase.RTDB.setString(&fbdo, catPath, cat2) ? "ok" : fbdo.errorReason().c_str());
      WarningMessage();
    } else if (((vectorSum >= threshold1) && (vectorSum <= threshold2)) && (relativeAltitude > 1 && relativeAltitude < 2)) {
      Serial.println("Crash Detected!");
      String mac = WiFi.macAddress();
      String Path = "/nodedata/" + mac;
      String UserPath = Path + "/node_id";
      String LatitudePath = Path + "/latitude";
      String LongitudePath = Path + "/longitude";
      String PeoplePath = Path + "/people_count";
      double latitude = 23.8116641;
      double longitude = 90.3570204;
      String cat1 = "A";
      String cat2 = "B";
      String catPath = Path + "/Damage_Category";
      // Get current hour
      int currentHour = timeClient.getHours();
      // Day and night time people count conditions
      if (currentHour >= 6 && currentHour < 18) {  // Daytime (assumed 6 AM to 6 PM)
        people_count = 800;
      } else {  // Nighttime
        people_count = 500;
      }
      Serial.printf("Set latitude... %s\n", Firebase.RTDB.setDouble(&fbdo, LatitudePath, latitude) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set longitude... %s\n", Firebase.RTDB.setDouble(&fbdo, LongitudePath, longitude) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set People Count... %s\n", Firebase.RTDB.setInt(&fbdo, PeoplePath, people_count) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Set Damage Category... %s\n", Firebase.RTDB.setString(&fbdo, catPath, cat1) ? "ok" : fbdo.errorReason().c_str());
      WarningMessage();
    }
  }
