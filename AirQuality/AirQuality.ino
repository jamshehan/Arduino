#include <ThingSpeak.h>
#include "secrets.h"

#include <WiFi.h>
#include <Wire.h>
#include <SFE_MicroOLED.h>
#include <SparkFunBME280.h>
#include <SparkFunCCS811.h>

#define REFRESH_RATE 5000

#define OLED_RESET_PIN 9
#define OLED_JUMPER_MODE 1

#define BME280_ADDRESS 0x77

#define CCS811_ADDRESS 0x5B

#define RFID_ADDRESS 0x7D
#define RFID_TAG_BYTES 6
#define RFID_TIME_BYTES 4
#define RFID_TOTAL_BYTES 10

WiFiServer server(80);
WiFiClient wifiClient;

unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;

MicroOLED oled(OLED_RESET_PIN, OLED_JUMPER_MODE);

BME280 bme280;

CCS811 ccs881(CCS811_ADDRESS);

void scanI2C() {
  byte error, address;
  int nDevices;

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");

      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(9600);

  Serial.println("");
  
  char wifiName[] = SECRET_SSID;
  char wifiPassword[] = SECRET_PASS;

  Serial.print("WiFi ssid: ");
  Serial.println(wifiName);

  Serial.print("ThingSpeak Channel: ");
  Serial.println(myChannelNumber);
    
  WiFi.begin(wifiName, wifiPassword);

  server.begin();

  Wire.begin();

  scanI2C();

  oled.begin();

  bme280.settings.commInterface = I2C_MODE;
  bme280.settings.I2CAddress = BME280_ADDRESS;
  bme280.settings.runMode = 3; //Normal mode
  bme280.settings.tStandby = 0;
  bme280.settings.filter = 4;
  bme280.settings.tempOverSample = 5;
  bme280.settings.pressOverSample = 5;
  bme280.settings.humidOverSample = 5;

  delay(10);

  bme280.begin();

  ccs881.begin();

  ThingSpeak.begin(wifiClient);  // Initialize ThingSpeak

  oled.clear(ALL);
}

String readRFID() {
  byte tempTag = 0; 
  long tempTime = 0; 
  int shift = 24; 
  long time = 0; 
  String tagID; 
  
  Wire.requestFrom((uint8_t)RFID_ADDRESS, (uint8_t)RFID_TOTAL_BYTES); 
  for(int x = 0; x < RFID_TAG_BYTES; x++) {
    tempTag = Wire.read();
    tagID += (String)tempTag; 
  }
  
  for (int x = 0; x < RFID_TIME_BYTES; x++){
    // Time is fed a byte at a time onto the buffer so we need to put it back
    // in order when we read it. 
    tempTime = Wire.read();
    time |= (tempTime << shift); 
    shift = shift - 8; 
  }

  return tagID;
}

long lastUpdate = millis();

long scrollingToggle = 0;

float lastTemperature = 0;
float lastHumidity = 0;
float lastPressure = 0;
float lastCO2 = 0;
float lastTVOC = 0;

void loop() {
  WiFiClient client = server.available();

  if (client) {
    String currentLine = "";  
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<script type='text/javascript'>setInterval(function() { if (document.getElementById('auto-update').checked) { location.reload(); } }, 4000);</script>");
            client.println("<h1>Air Quality Monitor</h1>");
            client.print("<p>Temperature: ");
            client.print(lastTemperature);
            client.print(" F</p>");
            client.print("<p>Humidity: ");
            client.print(lastHumidity);
            client.print(" %</p>");
            client.print("<p>Pressure: ");
            client.print(lastPressure);
            client.print(" kPa</p>");
            client.print("<p>CO2: ");
            client.print(lastCO2);
            client.print(" ppm</p>");
            client.print("<p>TVOC: ");
            client.print(lastTVOC);
            client.print(" ppb</p>");
            client.print("<p>Last update: ");
            client.print((millis() - lastUpdate) * 0.001);
            client.print(" s</p>");
            client.print("<input id='auto-update' type='checkbox' checked>");
            client.println();
            
            break;
          } else {
            currentLine = "";
          }
        }
      }
    }

    client.stop();
  }
  
  if (millis() > lastUpdate + REFRESH_RATE) {
    oled.clear(PAGE);
    oled.setFontType(0);
    oled.setCursor(0, 0);
    oled.print(F("AirQuality"));
    
    oled.setCursor(0, 8);
    switch (WiFi.status()) {
      case WL_CONNECTED: {
          Serial.print("Connected to WiFi at: ");
          Serial.println(WiFi.localIP());
          String ip = WiFi.localIP().toString();
          
          if (ip.length() > 10) {
            int start = scrollingToggle % (ip.length() - 10 + 1);
            ip = ip.substring(start, start + 10);
          }
      
          oled.print(ip);
          
          break;
        }
      case WL_NO_SHIELD:
        oled.print("WiFi error");
        break;
      case WL_IDLE_STATUS:
        oled.print("Connecting");
        break;
      case WL_NO_SSID_AVAIL:
        oled.print("No SSID");
        break;
      case WL_CONNECTION_LOST:
        oled.print("Lost WiFi");
        break;
      case WL_DISCONNECTED:
        oled.print("No WiFi");
        break;
    }
    
    //oled.setCursor(0, 16);
    //oled.print(readRFID().substring(0, 10));

    lastTemperature = bme280.readTempF();
    lastHumidity = bme280.readFloatHumidity();
    lastPressure = bme280.readFloatPressure() * 0.001;
  
    oled.setCursor(0, 24);
    switch (scrollingToggle % 3) {
      case 0:
        oled.print(lastTemperature);
        oled.print(" F");
        break;
      case 1:
        oled.print(lastHumidity);
        oled.print(" %");
        break;
      case 2:
        oled.print(lastPressure);
        oled.print(" kPa");
        break;
    }
  
    ccs881.readAlgorithmResults();

    lastCO2 = ccs881.getCO2();
    lastTVOC = ccs881.getTVOC();
  
    oled.setCursor(0, 32);
    switch (scrollingToggle % 2) {
      case 0:
        oled.print(lastCO2, 0);
        oled.print(" ppm");
        break;
      case 1:
        oled.print(lastTVOC, 0);
        oled.print(" ppb");
        break;
    }
    
    oled.display();
  
    scrollingToggle++;

    lastUpdate = millis();

    // send the values to ThingSpeak
    ThingSpeak.setField(1, lastTemperature);
    ThingSpeak.setField(2, lastHumidity);
    ThingSpeak.setField(3, lastPressure);
    ThingSpeak.setField(4, lastCO2);
    ThingSpeak.setField(5, lastTVOC);
    int responseCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if(responseCode == 200){
      Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(responseCode));
    }
  
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }

  yield();
}
