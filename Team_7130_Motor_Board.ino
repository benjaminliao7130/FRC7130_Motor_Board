#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h> 
#include <FastLED.h> 
// NOTE: Completely replaced Servo.h with low-level hardware timer

// === OLED Configuration ===
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
#define i2c_Address 0x3C 
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === FastLED Configuration ===
#define LED1_PIN        11 
#define LED2_PIN        12 
#define NUM_LEDS        1      
#define BRIGHTNESS      100    
CRGB led1[NUM_LEDS]; 
CRGB led2[NUM_LEDS]; 

// Hardware Pins
const int potPin1 = A0; 
const int potPin2 = A1; 
const int swFullMode = 3;  
const int swFineMode = 2;  
const int buzzerPin = 7; 
const int motor1Pin = 9;  // Must be connected to Pin 9 (Timer1 OC1A)
const int motor2Pin = 10; // Must be connected to Pin 10 (Timer1 OC1B)

// Protection mechanisms and state variables
float currentPulseWidth1 = 1500.0; 
float currentPulseWidth2 = 1500.0; 
float rampStep = 8.0; 
int lastMode = -1; 

// LED & Buzzer timing
unsigned long lastBlinkTime1 = 0; 
unsigned long lastBlinkTime2 = 0; 
bool led1IsOn = false; 
bool led2IsOn = false; 
unsigned long lastBuzzerTime = 0;
bool buzzerState = false;

// === Timer1 Hardware PWM Initialization (50Hz, 1000~2000us) ===
void setupHardwarePWM() {
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  
  TCCR1A = 0; TCCR1B = 0;
  
  // Fast PWM (Mode 14), TOP = ICR1, Non-inverting output
  TCCR1A |= (1 << WGM11) | (1 << COM1A1) | (1 << COM1B1);
  TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11);
  
  ICR1 = 39999; // 50Hz (20ms period)
  OCR1A = 3000; // Default to 1500us
  OCR1B = 3000; 
}

// === Set Motor PWM ===
void setMotorPWM(int motor, int microseconds) {
  if (motor == 1) OCR1A = microseconds * 2;
  if (motor == 2) OCR1B = microseconds * 2;
}

// === Potentiometer Value Reading ===
int getSpeedPercent(int pin, int mode) {
  int potValue = analogRead(pin); 
  int mapped = map(potValue, 0, 1023, -100, 100); 
  mapped = constrain(mapped, -100, 100); 
  
  if (abs(mapped) <= 5) return 0;
  if (mode == 2) return mapped;
  
  return (mapped >= 0) ? (mapped + 2) / 5 * 5 : (mapped - 2) / 5 * 5; 
}

// === LED State Update ===
void updateMotorLED(int speedPercent, CRGB* led, unsigned long& lastTime, bool& isOn, int mode) {
  if (mode == 0) { 
    if (abs(speedPercent) > 0) {
      if (millis() - lastTime >= 150) { 
        lastTime = millis(); isOn = !isOn;
        led[0] = isOn ? CRGB::Red : CRGB::Black;
      }
    } else {
      led[0] = CRGB::Green; 
    }
  } else { 
    int interval = (abs(speedPercent) > 0) ? map(abs(speedPercent), 0, 100, 400, 40) : 400;
    if (millis() - lastTime >= (unsigned long)interval) {
      lastTime = millis(); isOn = !isOn;
      if (isOn) {
        if (speedPercent == 0) led[0] = CRGB::Orange; 
        else if (speedPercent > 0) led[0] = CRGB::Green; 
        else led[0] = CRGB::Red; 
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
    while(true) delay(100); 
  }
  Wire.setClock(400000); 
  
  pinMode(swFullMode, INPUT_PULLUP); 
  pinMode(swFineMode, INPUT_PULLUP); 
  pinMode(buzzerPin, OUTPUT); 
  digitalWrite(buzzerPin, LOW); 
  
  setupHardwarePWM(); 

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
  int checkValue1 = getSpeedPercent(potPin1, 0); 
  int checkValue2 = getSpeedPercent(potPin2, 0); 
  unsigned long lastSafetyLedUpdate = 0;
  
  while (abs(checkValue1) > 0 || abs(checkValue2) > 0) { 
    display.clearDisplay();
    display.setCursor(0, 16); 
    display.print(F("! SAFETY WARNING !")); 
    display.setCursor(0, 32); 
    display.print(F("Set POTS to CENTER")); 
    display.display();
    
    updateMotorLED(checkValue1, led1, lastBlinkTime1, led1IsOn, 0);
    updateMotorLED(checkValue2, led2, lastBlinkTime2, led2IsOn, 0);
    
    if (millis() - lastSafetyLedUpdate > 50) {
        lastSafetyLedUpdate = millis();
        FastLED.show(); 
    }
    
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
  bool modeFull = (digitalRead(swFullMode) == LOW); 
  bool modeFine = (digitalRead(swFineMode) == LOW); 
  int currentMode = (modeFull) ? 1 : ((modeFine) ? 2 : 0); 

  int speedPercent1 = getSpeedPercent(potPin1, currentMode); 
  int speedPercent2 = getSpeedPercent(potPin2, currentMode); 
  float targetPulseWidth1 = 1500.0, targetPulseWidth2 = 1500.0; 

  if (currentMode == 0) { 
    if (abs(speedPercent1) > 0 || abs(speedPercent2) > 0) { 
      if (millis() - lastBuzzerTime > 150) {
        lastBuzzerTime = millis(); buzzerState = !buzzerState;
        digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
      }
    } else {
      digitalWrite(buzzerPin, LOW); 
    }
  } else {
    digitalWrite(buzzerPin, LOW); 
    float speedMultiplier = (currentMode == 1) ? 1.0 : 0.5; 
    targetPulseWidth1 = 1500.0 + (speedPercent1 * 5.0 * speedMultiplier);
    targetPulseWidth2 = 1500.0 + (speedPercent2 * 5.0 * speedMultiplier);
  }

  // === Ramping protection 緩啟動時間控制 ===
  static unsigned long lastRampTime = 0;
  const unsigned int RAMP_INTERVAL = 15; // 每 15 毫秒運算一次步進

  if (millis() - lastRampTime >= RAMP_INTERVAL) {
    lastRampTime = millis();

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

    // Write to hardware registers to generate PWM (只在數值更新時寫入)
    setMotorPWM(1, (int)currentPulseWidth1); 
    setMotorPWM(2, (int)currentPulseWidth2); 
  }

  int displaySpd1 = map((int)currentPulseWidth1, 1000, 2000, -100, 100);
  int displaySpd2 = map((int)currentPulseWidth2, 1000, 2000, -100, 100);
  int qSpd1 = (currentMode == 2) ? displaySpd1 : ((displaySpd1 >= 0) ? (displaySpd1 + 2) / 5 * 5 : (displaySpd1 - 2) / 5 * 5);
  int qSpd2 = (currentMode == 2) ? displaySpd2 : ((displaySpd2 >= 0) ? (displaySpd2 + 2) / 5 * 5 : (displaySpd2 - 2) / 5 * 5);

  updateMotorLED(speedPercent1, led1, lastBlinkTime1, led1IsOn, currentMode);
  updateMotorLED(speedPercent2, led2, lastBlinkTime2, led2IsOn, currentMode);
  
  static unsigned long lastLedUpdate = 0;
  if (millis() - lastLedUpdate > 50) { 
    lastLedUpdate = millis();
    FastLED.show(); 
  }

  static int lastQSpd1 = -999, lastQSpd2 = -999;
  static int lastSpeed1 = -999, lastSpeed2 = -999; 
  static unsigned long lastLcdTime = 0; 
  
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag(); display.begin(i2c_Address, true); 
    Wire.setClock(400000); lastQSpd1 = -999; lastSpeed1 = -999; lastSpeed2 = -999; lastMode = -1;                
  }

  if ((currentMode != lastMode) || (((qSpd1 != lastQSpd1) || (qSpd2 != lastQSpd2) || (currentMode == 0 && (speedPercent1 != lastSpeed1 || speedPercent2 != lastSpeed2))) && (millis() - lastLcdTime > 80))) { 
    display.clearDisplay(); 
    display.setCursor(0, 0); display.print(F("FRC 7130 DUAL CTRL"));
    
    display.setCursor(0, 16); 
    if (currentMode == 0) {
      if (abs(speedPercent1) > 0 || abs(speedPercent2) > 0) display.print(F("WARN: POTS NOT ZERO!"));
      else display.print(F("Mode: IDLE (SAFE)")); 
    } else if (currentMode == 1) { display.print(F("Mode: FULL (100%)")); }
    else if (currentMode == 2) { display.print(F("Mode: FINE ( 50%)")); }

    display.setCursor(0, 32); display.print(F("M1:"));
    if (qSpd1 == 0) display.print(F("STOP  0%"));
    else {
      display.print(qSpd1 > 0 ? F("FWD ") : F("REV "));
      if (abs(qSpd1) < 10) display.print(F("  "));
      else if (abs(qSpd1) < 100) display.print(F(" "));
      display.print(abs(qSpd1)); display.print(F("%"));
    }

    display.setCursor(0, 48); display.print(F("M2:"));
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
