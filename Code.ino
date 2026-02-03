#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ctype.h>
#include <string.h>

// =============================
// PINS (YOUR SAME PINS)
// =============================
const int FLAME_SENSOR_PIN = 9;
const int MQ2_DIGITAL_PIN  = 10;
const int MQ2_ANALOG_PIN   = A2;   // not used (kept)
const int BUZZER_PIN       = 3;
const int SWEEP_SERVO_PIN  = 6;
const int DOOR_SERVO_PIN   = 5;
const int LED_PIN          = A3;
const int FAN_RELAY_PIN    = 11;

const int SWEEP_MIN_POS = 40;
const int SWEEP_MAX_POS = 120;

// Relay logic (active LOW relay)
const int FAN_ON  = LOW;
const int FAN_OFF = HIGH;

// =============================
// SENSOR LOGIC SETTINGS ✅
// =============================
// ✅ YOUR FLAME SENSOR OUTPUT IS 0 NORMALLY, SO IT IS ACTIVE HIGH
// normal = 0, flame = 1
const bool FLAME_ACTIVE_LOW = false;

// MQ2 modules are usually ACTIVE LOW (LOW = smoke detected)
// if your MQ2 reads 1 normally, keep this true. If it reads 0 normally, set false.
const bool MQ2_ACTIVE_LOW   = true;

// Startup ignore time (prevents instant alarm after upload)
const unsigned long STARTUP_IGNORE_MS = 2000;

// =============================
// OBJECTS
// =============================
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A1, A0, 2, 4};
byte colPins[COLS] = {7, 8, 12, 13};
Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo sweepServo;
Servo doorServo;

// Door angles
const int DOOR_OPEN_POS   = 180;
const int DOOR_CLOSED_POS = 0;

// =============================
// SERVO SWEEP CONTROL
// =============================
unsigned long previousServoTime = 0;
const unsigned long servoInterval = 15;
int servoPos = SWEEP_MIN_POS;
int servoDirection = 1;

// =============================
// PASSWORD SYSTEM
// =============================
#define PASSWORD_LENGTH 5
char masterPassword[PASSWORD_LENGTH] = "1234";
char enteredPassword[PASSWORD_LENGTH];
byte passwordIndex = 0;

bool unlocked = false;
unsigned long unlockTime = 0;

const int MAX_FAILED_ATTEMPTS = 3;
int failedAttempts = 0;

// =============================
// INTRUDER (WRONG PIN) ALARM
// =============================
bool intruderAlarmActive = false;
bool lockedOut = false;
unsigned long previousIntruderBlink = 0;
const unsigned long intruderBlinkInterval = 200;

// =============================
// FIRE MODE (STABLE + NON BLOCKING)
// =============================
bool fireMode = false;
bool fireScreenShown = false;

unsigned long hazardLowStart  = 0;
unsigned long hazardHighStart = 0;

const unsigned long FIRE_START_CONFIRM_MS = 300;
const unsigned long FIRE_CLEAR_CONFIRM_MS = 3000;

unsigned long fireBlinkMillis = 0;
const unsigned long fireBlinkInterval = 200;
bool fireBlinkState = false;

// MQ2 warmup
const unsigned long MQ2_WARMUP_MS = 30000;

// =============================
// FAN/MOTOR OVERRIDE
// =============================
bool manualFanOverride = false;

// =============================
// FUNCTION PROTOTYPES
// =============================
void displayInitialScreen();
void handleServoSweep();
void handleBluetooth();
void checkPassword();

void activateIntruderAlarm();
void resetIntruderAlarmOnly();
void fullSystemReset();

bool readRawHazard();


// =============================
// SETUP
// =============================
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);

  // ✅ Stable reads
  pinMode(FLAME_SENSOR_PIN, INPUT_PULLUP);
  pinMode(MQ2_DIGITAL_PIN, INPUT_PULLUP);

  digitalWrite(FAN_RELAY_PIN, FAN_OFF);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  sweepServo.attach(SWEEP_SERVO_PIN);
  sweepServo.write(SWEEP_MIN_POS);

  doorServo.attach(DOOR_SERVO_PIN);
  doorServo.write(DOOR_CLOSED_POS);

  lcd.init();
  lcd.backlight();

  Serial.begin(9600);
  Serial.println("System Ready. Connect App.");

  displayInitialScreen();
}


// =============================
// LOOP
// =============================
void loop() {
  unsigned long now = millis();

  // Always listen Bluetooth first
  handleBluetooth();

  // ✅ Ignore sensors just after uploading
  if (now < STARTUP_IGNORE_MS) {
    handleServoSweep();
    return;
  }

  // ----------- FIRE DETECTION -----------
  bool rawHazard = readRawHazard();

  // ENTER FIRE MODE
  if (!fireMode) {
    if (rawHazard) {
      if (hazardLowStart == 0) hazardLowStart = now;

      if (now - hazardLowStart >= FIRE_START_CONFIRM_MS) {
        fireMode = true;
        fireScreenShown = false;
        hazardHighStart = 0;

        // cancel override
        manualFanOverride = false;

        Serial.println("ALARM: Confirmed Fire/Smoke!");
      }
    } else {
      hazardLowStart = 0;
    }
  }

  // ----------- FIRE MODE RUNNING -----------
  if (fireMode) {
    // FORCE motor ON
    digitalWrite(FAN_RELAY_PIN, FAN_ON);

    // STOP sweep servo during fire ✅

    if (!fireScreenShown) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! FIRE ALERT !!!");
      lcd.setCursor(0, 1);
      lcd.print("Motor ON + Alarm");
      fireScreenShown = true;
    }

    // Blink buzzer + LED
    if (now - fireBlinkMillis >= fireBlinkInterval) {
      fireBlinkMillis = now;
      fireBlinkState = !fireBlinkState;
      digitalWrite(BUZZER_PIN, fireBlinkState);
      digitalWrite(LED_PIN, fireBlinkState);
    }

    // Keep Bluetooth active even during fire
    handleBluetooth();

    // EXIT FIRE MODE
    if (!rawHazard) {
      if (hazardHighStart == 0) hazardHighStart = now;

      if (now - hazardHighStart >= FIRE_CLEAR_CONFIRM_MS) {
        fireMode = false;
        hazardLowStart = 0;
        hazardHighStart = 0;

        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);

        digitalWrite(FAN_RELAY_PIN, FAN_OFF);
        manualFanOverride = false;

        displayInitialScreen();
        Serial.println("Fire Cleared -> Normal Mode");
      }
    } else {
      hazardHighStart = 0;
    }

    return;
  }

  // ----------- NORMAL MODE -----------
  if (!manualFanOverride) {
    digitalWrite(FAN_RELAY_PIN, FAN_OFF);
  }

  // Intruder alarm blinking
  if (intruderAlarmActive) {
    if (now - previousIntruderBlink >= intruderBlinkInterval) {
      previousIntruderBlink = now;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }

  // Auto-lock door after 60 seconds
  if (unlocked && (now - unlockTime >= 60000)) {
    unlocked = false;
    doorServo.write(DOOR_CLOSED_POS);
    displayInitialScreen();
    Serial.println("Door Auto-Locked");
  }

  // Keypad input
  if (!unlocked && !lockedOut) {
    char key = customKeypad.getKey();
    if (key) {
      if (isdigit(key) && passwordIndex < 4) {
        enteredPassword[passwordIndex] = key;
        lcd.setCursor(passwordIndex, 1);
        lcd.print('*');
        passwordIndex++;

        if (passwordIndex == 4) {
          checkPassword();
        }
      }
      else if (key == '*' || key == '#') {
        passwordIndex = 0;
        memset(enteredPassword, 0, PASSWORD_LENGTH);
        displayInitialScreen();
      }
    }
  }

  // Sweep servo runs only in normal mode ✅
  handleServoSweep();
}


// =============================
// READ RAW HAZARD
// =============================
bool readRawHazard() {
  int flameState = digitalRead(FLAME_SENSOR_PIN);

  // ✅ active HIGH flame sensor (normal=0, flame=1)
  bool flameDetected = FLAME_ACTIVE_LOW ? (flameState == LOW) : (flameState == HIGH);

  // MQ2 digital warmup + logic
  bool smokeDetected = false;
  if (millis() > MQ2_WARMUP_MS) {
    int smokeState = digitalRead(MQ2_DIGITAL_PIN);
    smokeDetected = MQ2_ACTIVE_LOW ? (smokeState == LOW) : (smokeState == HIGH);
  }

  return (flameDetected || smokeDetected);
}


// =============================
// BLUETOOTH COMMANDS
// o=open, c=close, f=motor on, s=motor off, b=stop wrong-pin buzzer, r=reset
// =============================
void handleBluetooth() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r' || command == ' ') continue;
    command = tolower(command);

    if (command == 'o') {
      unlocked = true;
      lockedOut = false;
      failedAttempts = 0;

      intruderAlarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);

      unlockTime = millis();
      doorServo.write(DOOR_OPEN_POS);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("BT: Door OPEN");

      Serial.println("CMD OK: Door OPEN");
    }
    else if (command == 'c') {
      unlocked = false;
      doorServo.write(DOOR_CLOSED_POS);
      displayInitialScreen();
      Serial.println("CMD OK: Door CLOSE");
    }
    else if (command == 'f') {
      digitalWrite(FAN_RELAY_PIN, FAN_ON);
      manualFanOverride = true;
      Serial.println("CMD OK: Motor ON");
    }
    else if (command == 's') {
      digitalWrite(FAN_RELAY_PIN, FAN_OFF);
      manualFanOverride = false;
      Serial.println("CMD OK: Motor OFF");
    }
    else if (command == 'b') {
      intruderAlarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Alarm Silenced");
      lcd.setCursor(0, 1);
      lcd.print("Still Locked");

      Serial.println("CMD OK: Alarm Silenced");
    }
    else if (command == 'r') {
      fullSystemReset();
      Serial.println("CMD OK: Full Reset");
    }
    else {
      Serial.print("UNKNOWN CMD: ");
      Serial.println(command);
    }
  }
}


// =============================
// LCD SCREEN
// =============================
void displayInitialScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Pass:");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}


// =============================
// INTRUDER ALARM
// =============================
void activateIntruderAlarm() {
  intruderAlarmActive = true;
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("ALARM: Wrong PIN Locked!");
}

void resetIntruderAlarmOnly() {
  intruderAlarmActive = false;
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}


// =============================
// FULL RESET
// =============================
void fullSystemReset() {
  failedAttempts = 0;
  lockedOut = false;
  unlocked = false;
  passwordIndex = 0;
  memset(enteredPassword, 0, PASSWORD_LENGTH);

  resetIntruderAlarmOnly();

  fireMode = false;
  fireScreenShown = false;
  hazardLowStart = 0;
  hazardHighStart = 0;

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  manualFanOverride = false;
  digitalWrite(FAN_RELAY_PIN, FAN_OFF);

  doorServo.write(DOOR_CLOSED_POS);

  displayInitialScreen();
}


// =============================
// PASSWORD CHECK
// =============================
void checkPassword() {
  enteredPassword[passwordIndex] = '\0';

  if (strcmp(enteredPassword, masterPassword) == 0) {
    unlocked = true;
    unlockTime = millis();

    failedAttempts = 0;
    lockedOut = false;

    resetIntruderAlarmOnly();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Welcome Home");
    doorServo.write(DOOR_OPEN_POS);

    Serial.println("Pass Correct: Door OPEN");
  }
  else {
    failedAttempts++;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong Password!");

    if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
      lockedOut = true;
      lcd.setCursor(0, 1);
      lcd.print("LOCKED! BT=buzz");
      activateIntruderAlarm();
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Try again...");
    }
  }

  passwordIndex = 0;
  memset(enteredPassword, 0, PASSWORD_LENGTH);

  delay(700);
  if (!lockedOut && !unlocked) displayInitialScreen();
}


// =============================
// SWEEP SERVO CONTROL
// =============================
void handleServoSweep() {
  unsigned long now = millis();
  if (now - previousServoTime >= servoInterval) {
    previousServoTime = now;

    servoPos += servoDirection;

    if (servoPos >= SWEEP_MAX_POS) {
      servoPos = SWEEP_MAX_POS;
      servoDirection = -1;
    } else if (servoPos <= SWEEP_MIN_POS) {
      servoPos = SWEEP_MIN_POS;
      servoDirection = 1;
    }

    sweepServo.write(servoPos);
  }
}

