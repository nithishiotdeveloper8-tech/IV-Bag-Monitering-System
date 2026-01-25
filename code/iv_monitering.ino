/************ BLYNK ************/
#define BLYNK_TEMPLATE_ID "*********"
#define BLYNK_TEMPLATE_NAME "IV BAG MONITOR"
#define BLYNK_AUTH_TOKEN "**********"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

/************ OLED ************/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/************ HX711 ************/
#include "HX711.h"

/************ SERVO ************/
#include <ESP32Servo.h>

/************ WIFI ************/
char ssid[] = "********";
char pass[] = "********";

/************ PINS ************/
#define HX711_DOUT 23
#define HX711_CLK  19

#define LED_GREEN  26
#define LED_YELLOW 27
#define LED_RED    14

#define BUZZER_PIN 25
#define SERVO_PIN  18   // ✅ CONFIRMED WORKING

/************ OLED ************/
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/************ BUZZER PWM ************/
#define BUZZ_CH   0
#define BUZZ_FREQ 2000
#define BUZZ_RES  8

/************ SERVO ************/
Servo ivServo;
#define SERVO_OPEN  10     // IV OPEN position
#define SERVO_CLOSE 90     // IV CLAMP position

/************ HX711 ************/
HX711 scale;
float calibration_factor = 210474.0;

/************ IV SETTINGS ************/
#define MAX_ML 500
#define LOW_IV 30

int measuredMl = 0;
int percentIV = 0;

/************ TIMING ************/
unsigned long lastUpdate = 0;

/************ SETUP ************/
void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Buzzer PWM
  ledcSetup(BUZZ_CH, BUZZ_FREQ, BUZZ_RES);
  ledcAttachPin(BUZZER_PIN, BUZZ_CH);
  ledcWrite(BUZZ_CH, 0);

  // Servo
  ivServo.setPeriodHertz(50);
  ivServo.attach(SERVO_PIN, 500, 2400);
  ivServo.write(SERVO_OPEN);

  // HX711
  scale.begin(HX711_DOUT, HX711_CLK);
  scale.set_scale(calibration_factor);
  scale.tare();

  // OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED NOT FOUND");
    while (1);
  }
  display.clearDisplay();
  display.display();

  // Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

/************ MEASURE IV ************/
void measureIV() {
  float kg = scale.get_units(5);
  if (kg < 0) kg = 0;

  measuredMl = kg * 1000;
  measuredMl = constrain(measuredMl, 0, MAX_ML);

  percentIV = map(measuredMl, 0, MAX_ML, 0, 100);
  percentIV = constrain(percentIV, 0, 100);
}

/************ ALERTS + SERVO ************/
void updateAlerts() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  ledcWrite(BUZZ_CH, 0);

  if (percentIV >= 70) {
    digitalWrite(LED_GREEN, HIGH);
    ivServo.write(SERVO_OPEN);
  }
  else if (percentIV >= LOW_IV) {
    digitalWrite(LED_YELLOW, HIGH);
    ivServo.write(SERVO_OPEN);
  }
  else {
    digitalWrite(LED_RED, HIGH);
    ledcWrite(BUZZ_CH, 130);     // 🔊 BUZZER ON
    ivServo.write(SERVO_CLOSE); // 🔒 CLAMP IV
  }
}

/************ OLED UI ************/
void updateOLED() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(30, 0);
  display.println("IV MONITOR");

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print("Volume : ");
  display.print(measuredMl);
  display.println(" mL");

  display.setCursor(0, 30);
  display.print("Level  : ");
  display.print(percentIV);
  display.println(" %");

  // Progress bar
  int barWidth = map(percentIV, 0, 100, 0, 120);
  display.drawRect(4, 44, 120, 10, SSD1306_WHITE);
  display.fillRect(4, 44, barWidth, 10, SSD1306_WHITE);

  display.setCursor(0, 56);
  display.print("Status : ");
  if (percentIV < LOW_IV) display.print("LOW !");
  else display.print("NORMAL");

  display.display();
}

/************ LOOP ************/
void loop() {
  Blynk.run();

  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();

    measureIV();
    updateOLED();
    updateAlerts();

    Blynk.virtualWrite(V0, measuredMl);
    Blynk.virtualWrite(V1, percentIV);
  }
}
