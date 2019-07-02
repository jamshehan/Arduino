#include <ThingSpeak.h>
#include "secrets.h"

#include <WiFi.h>
#include <SFE_MicroOLED.h>
#include <SparkFunBME280.h>
#include <SparkFunCCS811.h>

#define REFRESH_RATE 15000

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

bool connecting = false;

void WIFI_Connect()
{
  WiFi.disconnect();
  connecting = true;
  
  char wifiName[] = SECRET_SSID;
  char wifiPassword[] = SECRET_PASS;
  
  Serial.print("Connecting to WiFi ");
  Serial.print(wifiName);
  Serial.println("...");
    
  WiFi.begin(wifiName, wifiPassword);
  
  for (int i = 0; i < 60; i++)
  {
    if ( WiFi.status() != WL_CONNECTED )
    {
      delay ( 250 );
      digitalWrite(LED_BUILTIN, LOW);
      Serial.print ( "." );
      delay ( 250 );
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
  if ( WiFi.status() == WL_CONNECTED )
  {
    Serial.println("");
    Serial.println("WiFi Connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  connecting = false;
  digitalWrite(LED_BUILTIN, 0);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(9600);

  Serial.println("");

  Serial.print("ThingSpeak Channel: ");
  Serial.println(myChannelNumber);
    
  WIFI_Connect();

  server.begin();

  oled.begin();

  bme280.settings.commInterface = I2C_MODE;
  bme280.settings.I2CAddress = BME280_ADDRESS;
  bme280.settings.runMode = MODE_FORCED;
  bme280.settings.tStandby = 0;
  bme280.settings.filter = 4;
  bme280.settings.tempOverSample = 1;
  bme280.settings.pressOverSample = 1;
  bme280.settings.humidOverSample = 1;
  bme280.settings.tempCorrection = -5; // -5C correction for sensor calibration

  delay(10);

  bme280.begin();

  ccs881.begin();

  ThingSpeak.begin(wifiClient);  // Initialize ThingSpeak

  oled.clear(ALL);
  
  delay(2000);
}

long lastUpdate = millis();

long scrollingToggle = 0;

float lastTemperature = 0;
float lastHumidity = 0;
float lastPressure = 0;
float lastCO2 = 0;
float lastTVOC = 0;
int lastHall = 0;

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
            client.print("<p>Hall Value: ");
            client.print(lastHall);
            client.print("</p>");
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

    lastHall = hallRead();
    wl_status_t wifiStatus = WiFi.status();

    switch (wifiStatus) {
      case WL_CONNECTED: {
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
        Serial.println("wifi disconnected ");
        break;
      case WL_DISCONNECTED:
        oled.print("No WiFi");
        Serial.println("wifi disconnected ");
        break;
    }

    if(wifiStatus != WL_CONNECTED && !connecting) {
      WIFI_Connect();
    }
       
    lastTemperature = bme280.readTempF();
    lastHumidity = bme280.readFloatHumidity();
    lastPressure = bme280.readFloatPressure() * 0.001;

    oled.setCursor(0, 8);
    oled.print(lastTemperature);
    oled.print(" F");

    oled.setCursor(0, 16);
    oled.print(lastHumidity);
    oled.print(" %");

    oled.setCursor(0, 24);
    oled.print(lastPressure);
    oled.print(" kPa");
  
    ccs881.readAlgorithmResults();

    lastCO2 = ccs881.getCO2();
    lastTVOC = ccs881.getTVOC();
  
    oled.setCursor(0, 32);
    oled.print(lastCO2, 0);
    oled.print(" ppm");

    oled.setCursor(0, 40);
    oled.print(lastTVOC, 0);
    oled.print(" ppb");
    
    oled.display();
  
    scrollingToggle++;

    lastUpdate = millis();

    // send the values to ThingSpeak
    ThingSpeak.setField(1, lastTemperature);
    ThingSpeak.setField(2, lastHumidity);
    ThingSpeak.setField(3, lastPressure);
    ThingSpeak.setField(4, lastCO2);
    ThingSpeak.setField(5, lastTVOC);
    ThingSpeak.setField(6, lastHall);
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
