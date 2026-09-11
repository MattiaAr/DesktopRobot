#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define BUTTON_PIN_1 19
#define BUTTON_PIN_2 23

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);


// =========================
// STATO
// =========================

bool lastButton1 = HIGH;
bool lastButton2 = HIGH;

bool tiredMode = false;
bool wasReacting = false;
bool wasShaking = false;

unsigned long reactionUntil = 0;
unsigned long shakeUntil = 0;


// =========================
// SETUP
// =========================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== ROBOT COMPLETO ===");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);


  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {

    Serial.println("ERRORE OLED!");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("OLED OK!");


  // MPU
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


  // RoboEyes
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);

  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(OFF);

  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);


  // Pulsanti
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);


  Serial.println("ROBOT PRONTO!");
}


// =========================
// LOOP
// =========================

void loop() {

  roboEyes.update();


  // ==================================================
  // PULSANTE 1 - FELICE
  // ==================================================

  bool button1 = digitalRead(BUTTON_PIN_1);

  if (lastButton1 == HIGH && button1 == LOW) {

    Serial.println(">>> PULSANTE 1 -> FELICE!");

    roboEyes.setMood(HAPPY);
    roboEyes.anim_laugh();

    reactionUntil = millis() + 2000;
    wasReacting = true;
  }

  lastButton1 = button1;


  // Durante la reazione al pulsante 1
  if (millis() < reactionUntil) {

    delay(10);
    return;
  }


  // Fine reazione
  if (wasReacting) {

    Serial.println(">>> REAZIONE FINITA");

    roboEyes.setMood(tiredMode ? TIRED : DEFAULT);

    wasReacting = false;
  }


  // ==================================================
  // PULSANTE 2 - MODALITÀ TIRED
  // ==================================================

  bool button2 = digitalRead(BUTTON_PIN_2);

  if (lastButton2 == HIGH && button2 == LOW) {

    tiredMode = !tiredMode;

    if (tiredMode) {

      Serial.println(">>> MODALITA' TIRED ON");

      roboEyes.setMood(TIRED);
    }
    else {

      Serial.println(">>> MODALITA' TIRED OFF");

      roboEyes.setMood(DEFAULT);
    }

    delay(150);
  }

  lastButton2 = button2;


  // ==================================================
  // LETTURA MPU
  // ==================================================

  sensors_event_t a, g, temp;

  mpu.getEvent(&a, &g, &temp);

  float x = a.acceleration.x;
  float y = a.acceleration.y;

  float gyroX = g.gyro.x;
  float gyroY = g.gyro.y;
  float gyroZ = g.gyro.z;


  // ==================================================
  // SCOSSA
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

    roboEyes.setMood(tiredMode ? TIRED : DEFAULT);

    wasShaking = false;

    Serial.println(">>> SCOSSA FINITA");
  }


  // ==================================================
  // DIREZIONE
  // ==================================================

  const float threshold = 2.5;

  bool right    = x > threshold;
  bool left     = x < -threshold;

  bool forward  = y < -threshold;
  bool backward = y > threshold;


  // ==================================================
  // MAPPA OCCHI
  // ==================================================

  if (right && backward) {

    // Destra + Indietro -> ↗
    roboEyes.setPosition(NE);

  }

  else if (right && forward) {

    // Destra + Avanti -> ↖
    roboEyes.setPosition(NW);

  }

  else if (left && backward) {

    // Sinistra + Indietro -> ↘
    roboEyes.setPosition(SE);

  }

  else if (left && forward) {

    // Sinistra + Avanti -> ↙
    roboEyes.setPosition(SW);

  }

  else if (right) {

    // Destra -> Su
    roboEyes.setPosition(N);

  }

  else if (left) {

    // Sinistra -> Giù
    roboEyes.setPosition(S);

  }

  else if (backward) {

    // Indietro -> Destra
    roboEyes.setPosition(E);

  }

  else if (forward) {

    // Avanti -> Sinistra
    roboEyes.setPosition(W);

  }

  else {

    // Fermo -> Centro
    roboEyes.setPosition(DEFAULT);
  }


  delay(20);
}