#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>
#include <Preferences.h>
#include "SlotMachineGame.h"
#include "BehaviorEngine.h"

#define SDA_PIN 21
#define SCL_PIN 22

#define BUTTON_PIN_1 19  // Bianco
#define BUTTON_PIN_2 23  // Rosso
#define BUTTON_PIN_3 18  // Blu
#define BUTTON_PIN_4 5   // Nero

// ==================================================
// OLED
// ==================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);
Preferences prefs;
SlotMachineGame slotGame;
BehaviorEngine behaviorEngine;
int activeEyeExpressionIndex = 0;
unsigned long lastSlotRender = 0;

enum Screen {
  ROBOT, MENU, GAMES, GAMES_LIST, GAMES_MODE, GAMES_RECORDS, SETTINGS,
  SYSTEM_MENU, SYSTEM_INFO, SYSTEM_RESOURCES, SYSTEM_HARDWARE, SYSTEM_DIAGNOSTICS,
  SYSTEM_REBOOT, EYE_MENU, EYE_MODEL_CAROUSEL, EYE_EXPRESSION,
  SETTINGS_BEHAVIOR, SETTINGS_DISPLAY, SETTINGS_CONTROLS, SETTINGS_SOUND, SETTINGS_RESET,
  SLOT_GAME
};
Screen currentScreen = ROBOT;

const char* menuLabels[] = {"GIOCHI", "ROBOT", "IMPOSTAZIONI", "SISTEMA"};
const int menuItems = 4;
int menuIndex = 0;
const char* gameLabels[] = {"GIOCA", "MODALITA", "RECORD"};
const int gameItems = 3;
int gameIndex = 0;
const char* gameListLabels[] = {"SNAKE", "DINO", "PONG", "SLOT"};
const int gameListItems = 4;
int gameListIndex = 0;
const char* gameModeLabels[] = {"UTENTE", "AUTONOMO"};
const int gameModeItems = 2;
int gameModeIndex = 0;
const char* settingsLabels[] = {"OCCHI", "COMPORT.", "DISPLAY", "CONTROLLI", "SUONI", "RESET"};
const int settingsItems = 6;
int settingsIndex = 0;
const char* systemLabels[] = {"INFO ROBOT", "RISORSE", "HARDWARE", "DIAGN.", "RIAVVIA"};
const int systemItems = 5;
int systemIndex = 0;
const char* behaviorLabels[] = {"PERSONAL.", "IDLE", "REAZIONI", "NOIA"};
const int behaviorItems = 4;
int behaviorIndex = 0;
const char* displayLabels[] = {"LUMIN.", "TIMEOUT", "OROLOGIO", "ANIMAZ."};
const int displayItems = 4;
int displayIndex = 0;
const char* controlLabels[] = {"MAPPATURA", "MENU HOLD"};
int controlIndex = 0;
const char* soundLabels[] = {"VOLUME", "SUONI UI", "REAZIONI"};
int soundIndex = 0;

enum EyeModel { EYE_CLASSIC, EYE_ROUND, EYE_SQUARE, EYE_PIXEL, EYE_CYBER, EYE_CUTE, EYE_MINIMAL, EYE_COZMO, EYE_ANIME, EYE_ORBIT };
const int EYE_MODEL_COUNT = 10;
const char* eyeModelNames[EYE_MODEL_COUNT] = {"CLASSIC", "ROUND", "SQUARE", "PIXEL", "CYBER", "CUTE", "MINIMAL", "COZMO", "ANIME", "ORBIT"};
int eyeModelIndex = 0;

enum EyeExpression { EXPR_DEFAULT, EXPR_HAPPY, EXPR_ANGRY, EXPR_TIRED, EXPR_CURIOUS };
const int EYE_EXPRESSION_COUNT = 5;
const char* eyeExpressionNames[EYE_EXPRESSION_COUNT] = {"DEFAULT", "HAPPY", "ANGRY", "TIRED", "CURIOUS"};
int eyeExpressionIndex = 0;

float customEyeX = 0.0f, customEyeY = 0.0f;
unsigned long customBlinkUntil = 0;
unsigned long customNextBlink = 0;
bool customBlinking = false;

bool clockEnabled = true;
int displayBrightness = 255;
int displayTimeout = 0;
bool uiSounds = true;
bool displayAvailable = false;
bool sensorAvailable = false;
int sensorX = 0;
int sensorY = 0;
unsigned long lastSensorSample = 0;
unsigned long lastRobotRender = 0;
bool lastUp = HIGH, lastDown = HIGH, lastBack = HIGH, lastSelect = HIGH;
const unsigned long INPUT_DEBOUNCE = 60;
unsigned long lastInputTime = 0;
const unsigned long MENU_HOLD_TIME = 2500;
unsigned long bothButtonsStart = 0;
bool menuHoldTriggered = false;

bool buttonPressed(int pin, bool &lastState) {
  bool state = digitalRead(pin);
  bool pressed = (lastState == HIGH && state == LOW);
  lastState = state;
  if (pressed && millis() - lastInputTime >= INPUT_DEBOUNCE) { lastInputTime = millis(); return true; }
  return false;
}

// ==================================================
// STATO PULSANTI
// ==================================================

bool lastButton1 = HIGH;
bool lastButton2 = HIGH;
bool lastButton3 = HIGH;
bool lastButton4 = HIGH;

// ==================================================
// STATO ROBOT
// ==================================================

bool tiredMode = false;
bool wasReacting = false;
bool wasShaking = false;

unsigned long reactionUntil = 0;
unsigned long shakeUntil = 0;

// ==================================================
// SETUP
// ==================================================

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== DESKTOP ROBOT ===");

    // ------------------------------------------------
    // I2C
    // ------------------------------------------------

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);

    // ------------------------------------------------
    // OLED
    // ------------------------------------------------

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("ERRORE OLED!");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("OLED OK!");

    // ------------------------------------------------
    // MPU-6050
    // ------------------------------------------------

    if (!mpu.begin(0x68, &Wire)) {
        Serial.println("ERRORE MPU!");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("MPU OK!");

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // ------------------------------------------------
    // ROBO EYES
    // ------------------------------------------------

    roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(OFF);
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(DEFAULT);

    // ------------------------------------------------
    // PULSANTI
    // ------------------------------------------------

    pinMode(BUTTON_PIN_1, INPUT_PULLUP);
    pinMode(BUTTON_PIN_2, INPUT_PULLUP);
    pinMode(BUTTON_PIN_3, INPUT_PULLUP);
    pinMode(BUTTON_PIN_4, INPUT_PULLUP);

    Serial.println("PULSANTE BIANCO -> GPIO19");
    Serial.println("PULSANTE ROSSO  -> GPIO23");
    Serial.println("PULSANTE BLU    -> GPIO18");
    Serial.println("PULSANTE NERO   -> GPIO5");
    Serial.println("ROBOT PRONTO!");
}

// ==================================================
// LOOP
// ==================================================

void loop() {

    // RoboEyes deve essere aggiornato continuamente
    roboEyes.update();

    // ==================================================
    // LETTURA PULSANTI
    // ==================================================

    bool button1 = digitalRead(BUTTON_PIN_1);
    bool button2 = digitalRead(BUTTON_PIN_2);
    bool button3 = digitalRead(BUTTON_PIN_3);
    bool button4 = digitalRead(BUTTON_PIN_4);

    // --------------------------------------------------
    // BIANCO -> HAPPY
    // --------------------------------------------------

    if (lastButton1 == HIGH && button1 == LOW) {
        Serial.println(">>> BIANCO GPIO19 -> PREMUTO");
        roboEyes.setMood(HAPPY);
        roboEyes.anim_laugh();
        reactionUntil = millis() + 2000;
        wasReacting = true;
    }
  }
}

    // --------------------------------------------------
    // ROSSO -> TIRED ON/OFF
    // --------------------------------------------------

    if (lastButton2 == HIGH && button2 == LOW) {
        tiredMode = !tiredMode;

        if (tiredMode) {
            Serial.println(">>> ROSSO GPIO23 -> PREMUTO / TIRED ON");
            roboEyes.setMood(TIRED);
        } else {
            Serial.println(">>> ROSSO GPIO23 -> PREMUTO / TIRED OFF");
            roboEyes.setMood(DEFAULT);
        }
    }

    // --------------------------------------------------
    // BLU -> ANGRY
    // --------------------------------------------------

    if (lastButton3 == HIGH && button3 == LOW) {
        Serial.println(">>> BLU GPIO18 -> PREMUTO");
        roboEyes.setMood(ANGRY);
        reactionUntil = millis() + 1000;
        wasReacting = true;
    }

    // --------------------------------------------------
    // NERO -> DEFAULT
    // --------------------------------------------------

    if (lastButton4 == HIGH && button4 == LOW) {
        Serial.println(">>> NERO GPIO5 -> PREMUTO");
        roboEyes.setMood(DEFAULT);
        reactionUntil = millis() + 1000;
        wasReacting = true;
    }

    lastButton1 = button1;
    lastButton2 = button2;
    lastButton3 = button3;
    lastButton4 = button4;

    // --------------------------------------------------
    // Fine reazione
    // --------------------------------------------------

    if (wasReacting && millis() >= reactionUntil) {
        if (tiredMode) {
            roboEyes.setMood(TIRED);
        } else {
            roboEyes.setMood(DEFAULT);
        }

        wasReacting = false;
        Serial.println(">>> REAZIONE FINITA");
    }

    // ==================================================
    // LETTURA MPU
    // ==================================================

    sensors_event_t a;
    sensors_event_t g;
    sensors_event_t temp;

    mpu.getEvent(&a, &g, &temp);

    float x = a.acceleration.x;
    float y = a.acceleration.y;

    float gyroX = g.gyro.x;
    float gyroY = g.gyro.y;
    float gyroZ = g.gyro.z;

    // ==================================================
    // RILEVAMENTO SCOSSA
    // ==================================================

    float movimento =
        abs(gyroX) +
        abs(gyroY) +
        abs(gyroZ);

    if (movimento > 8.0 && !wasShaking) {
        Serial.println(">>> SCOSSA!");
        roboEyes.setMood(ANGRY);
        shakeUntil = millis() + 800;
        wasShaking = true;
    }

    if (wasShaking && millis() > shakeUntil) {
        if (tiredMode) {
            roboEyes.setMood(TIRED);
        } else {
            roboEyes.setMood(DEFAULT);
        }

        wasShaking = false;
        Serial.println(">>> SCOSSA FINITA");
    }

    // ==================================================
    // DIREZIONE MPU
    // ==================================================

    const float threshold = 2.5;

    bool right = x > threshold;
    bool left = x < -threshold;
    bool forward = y < -threshold;
    bool backward = y > threshold;

    // ==================================================
    // MAPPA OCCHI
    // ==================================================

    if (right && backward) {
        roboEyes.setPosition(NE);
    }
    else if (right && forward) {
        roboEyes.setPosition(NW);
    }
    else if (left && backward) {
        roboEyes.setPosition(SE);
    }
    else if (left && forward) {
        roboEyes.setPosition(SW);
    }
    else if (right) {
        roboEyes.setPosition(N);
    }
    else if (left) {
        roboEyes.setPosition(S);
    }
    else if (backward) {
        roboEyes.setPosition(E);
    }
    else if (forward) {
        roboEyes.setPosition(W);
    }
    else {
        roboEyes.setPosition(DEFAULT);
    }
  }
  else if (eyeModelIndex == EYE_CYBER) {
    int top = 17 + by, bottom = 43 + by;
    int ox = bx;
    if (!customBlinking) {
      display.drawLine(20 + ox, bottom, 30 + ox, top, SSD1306_WHITE);
      display.drawLine(30 + ox, top, 55 + ox, top + 6, SSD1306_WHITE);
      display.drawLine(20 + ox, bottom, 55 + ox, bottom, SSD1306_WHITE);
      display.drawLine(73 + ox, top + 6, 98 + ox, top, SSD1306_WHITE);
      display.drawLine(98 + ox, top, 108 + ox, bottom, SSD1306_WHITE);
      display.drawLine(73 + ox, bottom, 108 + ox, bottom, SSD1306_WHITE);
      display.fillRect(36 + ox, 27 + by, 9, 5, SSD1306_BLACK);
      display.fillRect(83 + ox, 27 + by, 9, 5, SSD1306_BLACK);
    } else {
      display.drawLine(24 + ox, 31 + by, 52 + ox, 31 + by, SSD1306_WHITE);
      display.drawLine(76 + ox, 31 + by, 104 + ox, 31 + by, SSD1306_WHITE);
    }
  }
  else if (eyeModelIndex == EYE_CUTE) {
    int h = max(2, (int)(27 * blinkScale));
    int cy = 31 + by;
    display.fillRoundRect(28 + bx, cy - h / 2, 27, h, h / 2, SSD1306_WHITE);
    display.fillRoundRect(73 + bx, cy - h / 2, 27, h, h / 2, SSD1306_WHITE);
    if (!customBlinking) {
      display.fillCircle(41 + bx, cy, 4, SSD1306_BLACK);
      display.fillCircle(86 + bx, cy, 4, SSD1306_BLACK);
      display.drawPixel(43 + bx, cy - 2, SSD1306_WHITE);
      display.drawPixel(88 + bx, cy - 2, SSD1306_WHITE);
    }
  }
  else if (eyeModelIndex == EYE_MINIMAL) {
    int h = max(2, (int)(6 * blinkScale));
    int cy = 31 + by;
    display.fillRoundRect(22 + bx, cy - h / 2, 36, h, h / 2, SSD1306_WHITE);
    display.fillRoundRect(70 + bx, cy - h / 2, 36, h, h / 2, SSD1306_WHITE);
    if (!customBlinking) {
      display.fillRect(36 + bx, cy - h / 2, 8, h, SSD1306_BLACK);
      display.fillRect(84 + bx, cy - h / 2, 8, h, SSD1306_BLACK);
    }
  }
  else if (eyeModelIndex == EYE_COZMO) {
    int h = max(2, (int)(27 * blinkScale));
    int cy = 31 + by;
    display.fillRoundRect(18 + bx, cy - h / 2, 39, h, 10, SSD1306_WHITE);
    display.fillRoundRect(71 + bx, cy - h / 2, 39, h, 10, SSD1306_WHITE);
    if (!customBlinking) {
      display.fillCircle(38 + bx, cy, 6, SSD1306_BLACK);
      display.fillCircle(90 + bx, cy, 6, SSD1306_BLACK);
      display.fillCircle(36 + bx, cy - 2, 2, SSD1306_WHITE);
      display.fillCircle(88 + bx, cy - 2, 2, SSD1306_WHITE);
    }
  }
  else if (eyeModelIndex == EYE_ANIME) {
    int h = max(2, (int)(30 * blinkScale));
    int cy = 31 + by;
    int top = cy - h / 2;
    if (!customBlinking) {
      display.fillRoundRect(18 + bx, top, 40, h, 8, SSD1306_WHITE);
      display.fillRoundRect(70 + bx, top, 40, h, 8, SSD1306_WHITE);
      display.fillCircle(38 + bx, cy, 7, SSD1306_BLACK);
      display.fillCircle(90 + bx, cy, 7, SSD1306_BLACK);
      display.fillCircle(38 + bx, cy, 3, SSD1306_WHITE);
      display.fillCircle(90 + bx, cy, 3, SSD1306_WHITE);
      display.drawLine(17 + bx, top, 28 + bx, top - 5, SSD1306_WHITE);
      display.drawLine(111 + bx, top, 100 + bx, top - 5, SSD1306_WHITE);
    } else {
      display.drawLine(19 + bx, cy, 56 + bx, cy, SSD1306_WHITE);
      display.drawLine(72 + bx, cy, 109 + bx, cy, SSD1306_WHITE);
    }
  }
  else if (eyeModelIndex == EYE_ORBIT) {
    int cy = 31 + by;
    int ry = max(2, (int)(12 * blinkScale));
    if (!customBlinking) {
      display.drawCircle(40 + bx, cy, ry, SSD1306_WHITE);
      display.drawCircle(88 + bx, cy, ry, SSD1306_WHITE);
      display.fillCircle(40 + bx, cy, 3, SSD1306_WHITE);
      display.fillCircle(88 + bx, cy, 3, SSD1306_WHITE);
      display.drawPixel(40 + bx, cy, SSD1306_BLACK);
      display.drawPixel(88 + bx, cy, SSD1306_BLACK);
    } else {
      display.drawLine(28 + bx, cy, 52 + bx, cy, SSD1306_WHITE);
      display.drawLine(76 + bx, cy, 100 + bx, cy, SSD1306_WHITE);
    }
  }

  if (activeEyeExpressionIndex == EXPR_HAPPY) {
    display.drawLine(25 + bx, 46 + by, 42 + bx, 41 + by, SSD1306_WHITE);
    display.drawLine(86 + bx, 41 + by, 103 + bx, 46 + by, SSD1306_WHITE);
  } else if (activeEyeExpressionIndex == EXPR_ANGRY) {
    display.drawLine(20 + bx, 17 + by, 54 + bx, 25 + by, SSD1306_WHITE);
    display.drawLine(74 + bx, 25 + by, 108 + bx, 17 + by, SSD1306_WHITE);
  } else if (activeEyeExpressionIndex == EXPR_TIRED) {
    display.drawLine(22 + bx, 32 + by, 55 + bx, 36 + by, SSD1306_WHITE);
    display.drawLine(73 + bx, 36 + by, 106 + bx, 32 + by, SSD1306_WHITE);
  } else if (activeEyeExpressionIndex == EXPR_CURIOUS) {
    display.drawLine(23 + bx, 24 + by, 43 + bx, 18 + by, SSD1306_WHITE);
    display.drawLine(85 + bx, 18 + by, 105 + bx, 24 + by, SSD1306_WHITE);
    display.drawPixel(112 + bx, 16 + by, SSD1306_WHITE);
  }
}

    delay(20);
}
