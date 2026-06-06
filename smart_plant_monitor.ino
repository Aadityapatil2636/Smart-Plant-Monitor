
#define BLYNK_TEMPLATE_ID   "TMPL3cZbpangp"
#define BLYNK_TEMPLATE_NAME "Smart Plant monitor"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

#define SOIL_PIN       34
#define DHT_PIN         4
#define TRIG_PIN        5
#define ECHO_PIN       18
#define RELAY_PIN      26
#define BUZZER_PIN     27
#define RGB_R          13
#define RGB_G          12
#define RGB_B          14

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define SOIL_DRY_THRESHOLD   2800
#define SOIL_WET_THRESHOLD   2000
#define TANK_LOW_CM          19.5
#define TANK_MAX_CM          10.0

BlynkTimer timer;
static bool soilAlertSent  = false;
static bool waterAlertSent = false;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(RGB_R,      OUTPUT);
  pinMode(RGB_G,      OUTPUT);
  pinMode(RGB_B,      OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  noTone(BUZZER_PIN);
  setRGB(0, 0, 0);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 15);
  display.println("Smart Plant System");
  display.setCursor(35, 35);
  display.println("Starting...");
  display.display();
  delay(2000);
  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(2000L, readSensors);
}

void loop() {
  Blynk.run();
  timer.run();
}

void readSensors() {
  int   soilRaw  = analogRead(SOIL_PIN);
  float temp     = dht.readTemperature();
  float humidity = dht.readHumidity();
  float waterCm  = getWaterLevel();

  if (isnan(temp) || isnan(humidity)) {
    temp = 0.0; humidity = 0.0;
  }

  int soilPercent = map(soilRaw, 4095, 0, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  int waterPercent = map((int)waterCm, (int)TANK_LOW_CM,
                         (int)TANK_MAX_CM, 0, 100);
  waterPercent = constrain(waterPercent, 0, 100);

  bool waterIsLow = (waterCm >= TANK_LOW_CM);

  if (waterIsLow) {
    digitalWrite(RELAY_PIN, LOW);
  } else if (soilPercent < 30) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }

  if (waterIsLow) {
    tone(BUZZER_PIN, 1000, 500);
  } else {
    noTone(BUZZER_PIN);
  }

  if (waterIsLow) {
    setRGB(0, 0, 255);
  } else if (soilPercent < 30) {
    setRGB(255, 0, 0);
  } else {
    setRGB(0, 255, 0);
  }

  String tip = getPlantTip(soilPercent, temp, humidity, waterIsLow);
  updateOLED(soilPercent, waterPercent, temp, humidity, tip);

  Blynk.virtualWrite(V0, soilPercent);
  Blynk.virtualWrite(V1, waterPercent);
  Blynk.virtualWrite(V2, temp);
  Blynk.virtualWrite(V3, humidity);
  Blynk.virtualWrite(V4, tip);

  if (soilPercent < 30 && !soilAlertSent) {
    Blynk.logEvent("soil_dry", "Soil is dry! Auto watering started.");
    soilAlertSent = true;
  }
  if (soilPercent >= 30) soilAlertSent = false;

  if (waterIsLow && !waterAlertSent) {
    Blynk.logEvent("water_low", "Water tank is low! Please refill.");
    waterAlertSent = true;
  }
  if (!waterIsLow) waterAlertSent = false;
}

float getWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;
  return (duration * 0.034) / 2.0;
}

void updateOLED(int soil, int water, float temp,
                float hum, String tip) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("* Plant Monitor *");
  display.drawLine(0, 9, 127, 9, WHITE);
  display.setCursor(0, 13);
  display.printf("Soil  : %d%%\n",   soil);
  display.printf("Water : %d%%\n",   water);
  display.printf("Temp  : %.1fC\n",  temp);
  display.printf("Humid : %.1f%%\n", hum);
  display.drawLine(0, 52, 127, 52, WHITE);
  display.setCursor(0, 55);
  display.println(tip);
  display.display();
}

String getPlantTip(int soil, float temp,
                   float hum, bool waterLow) {
  if (waterLow)   return "Refill tank NOW!";
  if (soil < 30)  return "Watering now!";
  if (temp > 35)  return "Too hot! Move shade";
  if (hum > 80)   return "High humidity!";
  if (hum < 30)   return "Too dry - mist it";
  if (temp < 15)  return "Too cold for plant!";
  return "Plant is happy :)";
}

void setRGB(int r, int g, int b) {
  analogWrite(RGB_R, 255 - r);
  analogWrite(RGB_G, 255 - g);
  analogWrite(RGB_B, 255 - b);
}
