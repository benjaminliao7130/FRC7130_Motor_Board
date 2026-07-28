#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h> 
#include <Servo.h>
#include <FastLED.h> 

// === OLED Configuration ===
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
#define i2c_Address 0x3C 
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === FastLED Independent Dual Pin Configuration ===
#define LED1_PIN        11     // Dedicated LED for M1
#define LED2_PIN        12     // Dedicated LED for M2
#define NUM_LEDS        1      
#define BRIGHTNESS      100    

CRGB led1[NUM_LEDS]; 
CRGB led2[NUM_LEDS]; 

// Hardware Initialization
Servo motor1; 
Servo motor2; 

const int potPin1 = A0; 
const int potPin2 = A1; 
const int swFullMode = 3;  // Switch Up: Full speed mode (100%)
const int swFineMode = 2;  // Switch Down: Fine tuning mode (50% limit)
const int buzzerPin = 7; 
const int motor1Pin = 9;
const int motor2Pin = 10;

// Protection mechanisms and state variables
float currentPulseWidth1 = 1500.0; 
float currentPulseWidth2 = 1500.0; 
float rampStep = 8.0; 
int lastMode = -1; 

// Independent LED timing variables
unsigned long lastBlinkTime1 = 0; 
unsigned long lastBlinkTime2 = 0; 
bool led1IsOn = false; 
bool led2IsOn = false; 

// Buzzer timing
unsigned long lastBuzzerTime = 0;
bool buzzerState = false;

// === Potentiometer Value Reading (with mode check) ===
int getSpeedPercent(int pin, int mode) {
  int potValue = analogRead(pin); 
  int mapped = map(potValue, 0, 1023, -100, 100); 
  mapped = constrain(mapped, -100, 100); 
  
  // Center ±5% deadzone for safety. Do not remove!
  if (abs(mapped) <= 5) return 0;
  
  // Output 1% resolution for fine-tuning mode (Mode 2)
  if (mode == 2) {
    return mapped;
  } 
  // IDLE or FULL mode: round to nearest 5% to filter noise
  else {
    if (mapped >= 0) return (mapped + 2) / 5 * 5; 
    else return (mapped - 2) / 5 * 5; 
  }
}

// === Independent LED State Update Function ===
void updateMotorLED(int speedPercent, CRGB* led, unsigned long& lastTime, bool& isOn, int mode) {
  if (mode == 0) { // IDLE mode (including alarm state)
    if (abs(speedPercent) > 0) {
      if (millis() - lastTime >= 150) { // Not centered: rapid red flashing
        lastTime = millis();
        isOn = !isOn;
        led[0] = isOn ? CRGB::Red : CRGB::Black;
      }
    } else {
      led[0] = CRGB::Green; // Centered (safe): solid green
    }
  } else { // ACTIVE mode (running)
    int interval = 400; // Breathing rate when stationary
    if (abs(speedPercent) > 0) {
      interval = map(abs(speedPercent), 0, 100, 400, 40); // Faster speed = faster flashing
    }
    if (millis() - lastTime >= (unsigned long)interval) {
      lastTime = millis();
      isOn = !isOn;
      if (isOn) {
        if (speedPercent == 0) led[0] = CRGB::Orange; // Orange at zero point
        else if (speedPercent > 0) led[0] = CRGB::Green; // Green for forward
        else led[0] = CRGB::Red; // Red for reverse
      } else {
        led[0] = CRGB::Black;
      }
    }
  }
}

void setup() {
  FastLED.addLeds<WS2812B, LED1_PIN, GRB>(led1, NUM_LEDS); 
  FastLED.addLeds<WS2812B, LED2_PIN, GRB>(led2, NUM_LEDS); 
  FastLED.setBrightness(BRIGHTNESS); 
  FastLED.clear(); 
  FastLED.show(); 

  Wire.begin();
  Wire.setWireTimeout(25000, true); 
  if(!display.begin(i2c_Address, true)) {
    while(true) delay(100); // Infinite loop on screen error
  }
  Wire.setClock(400000); 
  
  pinMode(swFullMode, INPUT_PULLUP); 
  pinMode(swFineMode, INPUT_PULLUP); 
  pinMode(buzzerPin, OUTPUT); 
  digitalWrite(buzzerPin, LOW); 
  
  motor1.attach(motor1Pin, 1000, 2000); 
  motor2.attach(motor2Pin, 1000, 2000); 
  motor1.writeMicroseconds(1500); 
  motor2.writeMicroseconds(1500); 

  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(SH110X_WHITE); 
  display.setCursor(0, 0); 
  display.print(F("FRC 7130 DUAL CTRL"));
  display.setCursor(0, 16); 
  display.print(F("Initializing...")); 
  display.display();
  delay(1500); 

  // === Boot Safety Check ===
  // Read values in Mode 0 at boot
  int checkValue1 = getSpeedPercent(potPin1, 0); 
  int checkValue2 = getSpeedPercent(potPin2, 0); 
  while (abs(checkValue1) > 0 || abs(checkValue2) > 0) { 
    display.clearDisplay();
    display.setCursor(0, 16); 
    display.print(F("! SAFETY WARNING !")); 
    display.setCursor(0, 32); 
    display.print(F("Set POTS to CENTER")); 
    display.display();
    
    updateMotorLED(checkValue1, led1, lastBlinkTime1, led1IsOn, 0);
    updateMotorLED(checkValue2, led2, lastBlinkTime2, led2IsOn, 0);
    FastLED.show(); 
    
    if (millis() - lastBuzzerTime > 150) {
      lastBuzzerTime = millis();
      buzzerState = !buzzerState;
      digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
    }
    
    checkValue1 = getSpeedPercent(potPin1, 0); 
    checkValue2 = getSpeedPercent(potPin2, 0); 
  }
  
  digitalWrite(buzzerPin, LOW);
  display.clearDisplay();
  display.setCursor(0, 16); 
  display.print(F("FRC 7130: ARMED")); 
  display.display();
  delay(1000); 
}

void loop() {
  // Determine physical switch state first
  bool modeFull = (digitalRead(swFullMode) == LOW); 
  bool modeFine = (digitalRead(swFineMode) == LOW); 
  int currentMode = 0; 
  if (modeFull) currentMode = 1; 
  else if (modeFine) currentMode = 2; 

  // Determine reading resolution (5% or 1%) based on current mode
  int speedPercent1 = getSpeedPercent(potPin1, currentMode); 
  int speedPercent2 = getSpeedPercent(potPin2, currentMode); 
  
  float targetPulseWidth1 = 1500.0; 
  float targetPulseWidth2 = 1500.0; 

  if (currentMode == 0) { 
    targetPulseWidth1 = 1500.0; 
    targetPulseWidth2 = 1500.0; 
    
    if (abs(speedPercent1) > 0 || abs(speedPercent2) > 0) { 
      if (millis() - lastBuzzerTime > 150) {
        lastBuzzerTime = millis();
        buzzerState = !buzzerState;
        digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
      }
    } else {
      digitalWrite(buzzerPin, LOW); 
    }
  } else {
    digitalWrite(buzzerPin, LOW); 
    
    // Set fine-tuning speed limit to 50%
    float speedMultiplier = (currentMode == 1) ? 1.0 : 0.5; 
    targetPulseWidth1 = 1500.0 + (speedPercent1 * 5.0 * speedMultiplier);
    targetPulseWidth2 = 1500.0 + (speedPercent2 * 5.0 * speedMultiplier);
  }

  // Ramping protection (Slope calculation)
  if (currentPulseWidth1 < targetPulseWidth1) { 
    currentPulseWidth1 += rampStep; 
    if (currentPulseWidth1 > targetPulseWidth1) currentPulseWidth1 = targetPulseWidth1; 
  } else if (currentPulseWidth1 > targetPulseWidth1) { 
    currentPulseWidth1 -= rampStep; 
    if (currentPulseWidth1 < targetPulseWidth1) currentPulseWidth1 = targetPulseWidth1; 
  }

  if (currentPulseWidth2 < targetPulseWidth2) { 
    currentPulseWidth2 += rampStep; 
    if (currentPulseWidth2 > targetPulseWidth2) currentPulseWidth2 = targetPulseWidth2; 
  } else if (currentPulseWidth2 > targetPulseWidth2) { 
    currentPulseWidth2 -= rampStep; 
    if (currentPulseWidth2 < targetPulseWidth2) currentPulseWidth2 = targetPulseWidth2; 
  }

  motor1.writeMicroseconds((int)currentPulseWidth1); 
  motor2.writeMicroseconds((int)currentPulseWidth2); 

  // Map PWM signal back to +/- 100% for OLED display
  int displaySpd1 = map((int)currentPulseWidth1, 1000, 2000, -100, 100);
  int displaySpd2 = map((int)currentPulseWidth2, 1000, 2000, -100, 100);
  
  // Display step logic follows the mode
  int qSpd1, qSpd2;
  if (currentMode == 2) {
    qSpd1 = displaySpd1; // Fine mode uses true 1% resolution
    qSpd2 = displaySpd2; 
  } else {
    // Full/Idle mode locked to 5% steps for a stable display
    qSpd1 = (displaySpd1 >= 0) ? (displaySpd1 + 2) / 5 * 5 : (displaySpd1 - 2) / 5 * 5;
    qSpd2 = (displaySpd2 >= 0) ? (displaySpd2 + 2) / 5 * 5 : (displaySpd2 - 2) / 5 * 5;
  }

  // === Update Two Independent LED States ===
  updateMotorLED(speedPercent1, led1, lastBlinkTime1, led1IsOn, currentMode);
  updateMotorLED(speedPercent2, led2, lastBlinkTime2, led2IsOn, currentMode);
  FastLED.show(); 

  // === OLED Display Update ===
  static int lastQSpd1 = -999;
  static int lastQSpd2 = -999;
  static int lastSpeed1 = -999; 
  static int lastSpeed2 = -999; 
  static unsigned long lastLcdTime = 0; 
  
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag(); 
    display.begin(i2c_Address, true); 
    Wire.setClock(400000);            
    lastQSpd1 = -999; lastSpeed1 = -999; lastSpeed2 = -999; lastMode = -1;                
  }

  bool modeChanged = (currentMode != lastMode);
  bool displayChanged = (qSpd1 != lastQSpd1) || (qSpd2 != lastQSpd2); 
  bool warningChanged = (currentMode == 0) && ((speedPercent1 != lastSpeed1) || (speedPercent2 != lastSpeed2));

  if (modeChanged || ((displayChanged || warningChanged) && (millis() - lastLcdTime > 80))) { 
    display.clearDisplay(); 
    
    display.setCursor(0, 0); 
    display.print(F("FRC 7130 DUAL CTRL"));
    
    display.setCursor(0, 16); 
    if (currentMode == 0) {
      if (abs(speedPercent1) > 0 || abs(speedPercent2) > 0) display.print(F("WARN: POTS NOT ZERO!"));
      else display.print(F("Mode: IDLE (SAFE)")); 
    } else if (currentMode == 1) {
      display.print(F("Mode: FULL (100%)")); 
    } else if (currentMode == 2) {
      display.print(F("Mode: FINE ( 50%)")); 
    }

    display.setCursor(0, 32);
    display.print(F("M1:"));
    if (qSpd1 == 0) display.print(F("STOP  0%"));
    else {
      display.print(qSpd1 > 0 ? F("FWD ") : F("REV "));
      if (abs(qSpd1) < 10) display.print(F("  "));
      else if (abs(qSpd1) < 100) display.print(F(" "));
      display.print(abs(qSpd1)); display.print(F("%"));
    }

    display.setCursor(0, 48);
    display.print(F("M2:"));
    if (qSpd2 == 0) display.print(F("STOP  0%"));
    else {
      display.print(qSpd2 > 0 ? F("FWD ") : F("REV "));
      if (abs(qSpd2) < 10) display.print(F("  "));
      else if (abs(qSpd2) < 100) display.print(F(" "));
      display.print(abs(qSpd2)); display.print(F("%"));
    }
    
    display.display(); 
    Wire.clearWireTimeoutFlag(); 
    lastQSpd1 = qSpd1; lastQSpd2 = qSpd2; 
    lastSpeed1 = speedPercent1; lastSpeed2 = speedPercent2; 
    lastMode = currentMode; lastLcdTime = millis(); 
  }
}