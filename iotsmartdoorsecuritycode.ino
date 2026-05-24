 /* * Project: IoT Smart Door Security System
 * Developer: Aarti Pathare
 * Features: PIR Motion Detection, Blynk IoT Integration, Secret Key Override
 */

#define BLYNK_TEMPLATE_ID   "TMPL3PCWVRR-g"
#define BLYNK_TEMPLATE_NAME "Face Recognition Door"
#define BLYNK_AUTH_TOKEN    "cL2JrAvQ_x_mpYpgc0sk3MQ68igsXTab"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Pin Definitions
#define SDA_PIN 14
#define SCL_PIN 15
#define SERVO_PIN 13
#define SECRET_KEY 12
#define PIR_PIN 2
#define BUZZER_PIN 4  // Also controls onboard Flash LED
#define RED_LED 33    // Onboard Status LED

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

// Network Credentials
const char* ssid = "Vivo";
const char* password = "aaru1035";

// System Variables
int clickCount = 0;
unsigned long lastClickTime = 0;
unsigned long pirStartTime = 0;
bool personDetected = false;
bool sirenMuted = false;
const unsigned long suspiciousTime = 60000; // 1 Minute threshold

void startCameraServer();

// Display Default Standby Message
void displayStandby() {
  lcd.clear();
  lcd.print(" SYSTEM SECURED ");
  lcd.setCursor(0, 1);
  lcd.print("    READY...    ");
}

// Function to handle door unlocking mechanism
void unlockDoor(String method) {
  lcd.clear();
  lcd.print("ACCESS GRANTED");
  lcd.setCursor(0, 1);
  lcd.print(method);
  
  Blynk.virtualWrite(V5, 255); // Update Virtual LED on Blynk
  
  // Audio Feedback
  digitalWrite(BUZZER_PIN, HIGH); 
  delay(200); 
  digitalWrite(BUZZER_PIN, LOW);
  
  // Actuating Servo Motor
  myServo.write(90);
  delay(4000);
  myServo.write(0);
  
  Blynk.virtualWrite(V5, 0); 
  Blynk.virtualWrite(V0, 0); // Reset Blynk switch
  displayStandby();
}

// Blynk Cloud Control Handlers
BLYNK_WRITE(V0) { if (param.asInt()) unlockDoor("BY BLYNK APP"); }
BLYNK_WRITE(V2) { sirenMuted = param.asInt(); } // Toggle Siren Mute

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable Brownout Detector
  
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init(); 
  lcd.backlight();
  lcd.print("CONNECTING...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  lcd.clear();
  lcd.print("IP: "); 
  lcd.print(WiFi.localIP().toString());
  delay(2000);

  // Pin Configuration
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SECRET_KEY, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT); 
  digitalWrite(RED_LED, LOW);
  
  myServo.attach(SERVO_PIN); 
  myServo.write(0);

  // Camera Configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) == ESP_OK) { startCameraServer(); }
  
  Blynk.config(BLYNK_AUTH_TOKEN);
  displayStandby();
}

void loop() {
  Blynk.run();

  // PIR Motion and Alarm Logic
  if (digitalRead(PIR_PIN) == HIGH) {
    if (!personDetected) {
      pirStartTime = millis();
      personDetected = true;
      lcd.setCursor(0, 1); 
      lcd.print("MOTION DETECTED ");
      Blynk.logEvent("suspicious_alert", "Someone at the door!"); 
    }
    
    // Trigger Alarm if person stays for more than suspiciousTime
    if (millis() - pirStartTime >= suspiciousTime) {
      lcd.clear(); 
      lcd.print("!!! WARNING !!!");
      lcd.setCursor(0, 1); 
      lcd.print("INTRUDER ALERT!");
      
      if (!sirenMuted) {
        digitalWrite(BUZZER_PIN, HIGH); 
        delay(100);
        digitalWrite(BUZZER_PIN, LOW); 
        delay(100);
      }
    }
  } else {
    if (personDetected) {
      personDetected = false;
      displayStandby();
    }
  }

  // Secret Key Override Logic (3 Clicks to Open)
  if (digitalRead(SECRET_KEY) == LOW) {
    unsigned long now = millis();
    if (now - lastClickTime > 400) {
      clickCount++;
      lastClickTime = now;
      lcd.setCursor(0, 1); 
      lcd.print("KEY CLICKED: "); 
      lcd.print(clickCount);
      digitalWrite(BUZZER_PIN, HIGH); 
      delay(100); 
      digitalWrite(BUZZER_PIN, LOW);
    }
    if (clickCount == 3) { 
      unlockDoor("BY SECRET KEY"); 
      clickCount = 0; 
    }
  }
  
  // Reset click count after 2 seconds of inactivity
  if (clickCount > 0 && (millis() - lastClickTime > 2000)) {
    clickCount = 0;
  }
}
