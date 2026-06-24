#include <WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ==========================================
//           USER CONFIGURATION
// ==========================================
const char* ssid = "vivo Y400 Pro 5G";            // Replace with your Wi-Fi name
const char* password = "Dev@2471";    // Replace with your Wi-Fi password

unsigned long myChannelNumber = 3415227;          // Replace with your ThingSpeak Channel ID
const char * myWriteAPIKey = "R60U3S4HBLNTPAMV"; // Replace with your Write API Key

// --- pH CALIBRATION ---
float phCalibrationOffset = 0.00; 

// ==========================================
//             PIN DEFINITIONS
// ==========================================
#define DHTPIN 26
#define DHTTYPE DHT11       // Using the blue DHT11!
#define SOIL_PWR_PIN 25
#define SOIL_DATA_PIN 36    // VP pin on ESP32
#define PH_ANALOG_PIN 32    // Direct analog connection to sensor D5
#define RELAY_PIN 27

// LCD Pins
#define TFT_CS   5
#define TFT_DC   22
#define TFT_RST  4

// ==========================================
//        COMPONENT INITIALIZATION
// ==========================================
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
WiFiClient client;          

// --- Global Variables ---
float temperature = 0.0;
float humidity = 0.0;
int soilMoistureRaw = 0;
int soilMoisturePercent = 0;
float phValue = 7.0; 
int pumpState = 0; // 0 = OFF, 1 = ON

// --- Timing Variables ---
unsigned long previousMillisLocal = 0;
const long localInterval = 5000;  // 5 seconds for Serial, LCD & Pump logic

unsigned long previousMillisCloud = 0;
const long cloudInterval = 20000; // 20 seconds for ThingSpeak upload limit

// ==========================================
//                  SETUP
// ==========================================
void setup() {
  Serial.begin(115200); 
  
  pinMode(SOIL_PWR_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PH_ANALOG_PIN, INPUT); 

  // Start with sensors and pump completely OFF (Active-High Relay)
  digitalWrite(SOIL_PWR_PIN, LOW); 
  digitalWrite(RELAY_PIN, LOW);    

  dht.begin();
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  
  tft.setCursor(10, 10);
  tft.println("Starting System...");

  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.println("WiFi Connected!");
  Serial.println("\nWiFi Connected!");
  delay(1500);

  ThingSpeak.begin(client);

  Serial.println("Taking initial readings...");
  readSensors();     
  controlPump();     
  printToSerial();   
  updateLCD();       
  sendToThingSpeak(); 
}

// ==========================================
//                MAIN LOOP
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  // --- LOCAL TASK: Every 5 Seconds ---
  if (currentMillis - previousMillisLocal >= localInterval) {
    previousMillisLocal = currentMillis;
    readSensors();
    controlPump();
    printToSerial(); 
    updateLCD();     
  }

  // --- CLOUD TASK: Every 20 Seconds ---
  if (currentMillis - previousMillisCloud >= cloudInterval) {
    previousMillisCloud = currentMillis;
    sendToThingSpeak(); 
  }
}

// ==========================================
//               FUNCTIONS
// ==========================================

void readSensors() {
  // 1. Read Temp & Humidity 
  humidity = dht.readHumidity();
  delay(50);
  temperature = dht.readTemperature();

  // 2. Read Soil Moisture
  digitalWrite(SOIL_PWR_PIN, HIGH);
  delay(10); 
  soilMoistureRaw = analogRead(SOIL_DATA_PIN);
  digitalWrite(SOIL_PWR_PIN, LOW);
  
  // Map ADC to Percentage
  soilMoisturePercent = map(soilMoistureRaw, 3000, 1000, 0, 100); 
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

  // 3. Read Analog pH
  int phRaw = analogRead(PH_ANALOG_PIN);
  float voltage = phRaw * (3.3 / 4095.0);
  phValue = (3.5 * voltage) + phCalibrationOffset;
}

void controlPump() {
  // 1. Turn ON if very dry
  if (soilMoisturePercent < 30) {
    digitalWrite(RELAY_PIN, HIGH); // HIGH = ON for Active-High Relay
    pumpState = 1; 
  } 
  // 2. Turn OFF as soon as it hits 35%
  else if (soilMoisturePercent > 35) {
    digitalWrite(RELAY_PIN, LOW); // LOW = OFF for Active-High Relay
    pumpState = 0; 
  }
}

void printToSerial() {
  Serial.println("\n--- Current Plant Status ---");
  Serial.print("Temperature:   "); Serial.print(temperature); Serial.println(" C");
  Serial.print("Humidity:      "); Serial.print(humidity); Serial.println(" %");
  Serial.print("Soil Moisture: "); Serial.print(soilMoisturePercent); Serial.println(" %");
  Serial.print("pH Level:      "); Serial.println(phValue);
  Serial.print("Pump Status:   "); 
  if(pumpState == 1) Serial.println("ON (WATERING)");
  else Serial.println("OFF (STANDBY)");
  Serial.println("----------------------------");
}

void updateLCD() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  
  tft.setTextSize(3);
  tft.println(" Plant Monitor");
  tft.println("---------------");
  
  tft.setTextSize(2);
  tft.print("Temp: "); tft.print(temperature); tft.println(" C");
  tft.print("Hum:  "); tft.print(humidity); tft.println(" %");
  tft.print("Soil: "); tft.print(soilMoisturePercent); tft.println(" %");
  tft.print("pH:   "); tft.println(phValue);
  
  tft.println();
  tft.print("Pump: ");
  if(pumpState == 1) {
     tft.setTextColor(ILI9341_GREEN);
     tft.println("ON (WATERING)");
  } else {
     tft.setTextColor(ILI9341_RED);
     tft.println("OFF (STANDBY)");
  }
  tft.setTextColor(ILI9341_WHITE); 
}

void sendToThingSpeak() {
  if(WiFi.status() == WL_CONNECTED){
    ThingSpeak.setField(1, temperature);
    ThingSpeak.setField(2, humidity);
    ThingSpeak.setField(3, soilMoisturePercent);
    ThingSpeak.setField(4, phValue);
    ThingSpeak.setField(5, pumpState);
    
    int httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    
    if(httpCode == 200){
      Serial.println("=> ThingSpeak Channel update successful!");
    } else {
      Serial.print("=> Problem updating ThingSpeak. HTTP error: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.println("=> WiFi disconnected.");
  }
}