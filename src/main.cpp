#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define BUTTON_PIN_1 19  // Bianco
#define BUTTON_PIN_2 23  // Rosso
#define BUTTON_PIN_3 18  // Blu
#define BUTTON_PIN_4 5   // Nero

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);

bool lastButton1 = HIGH;
bool lastButton2 = HIGH;
bool lastButton3 = HIGH;
bool lastButton4 = HIGH;

bool tiredMode = false;
bool wasReacting = false;
bool wasShaking = false;

unsigned long reactionUntil = 0;
unsigned long shakeUntil = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== DESKTOP ROBOT ===");

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("ERRORE OLED!");
        while (true) delay(1000);
    }

    Serial.println("OLED OK!");

    if (!mpu.begin(0x68, &Wire)) {
        Serial.println("ERRORE MPU!");
        while (true) delay(1000);
    }

    Serial.println("MPU OK!");

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(OFF);
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(DEFAULT);

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

void loop() {
    roboEyes.update();

    bool button1 = digitalRead(BUTTON_PIN_1);
    bool button2 = digitalRead(BUTTON_PIN_2);
    bool button3 = digitalRead(BUTTON_PIN_3);
    bool button4 = digitalRead(BUTTON_PIN_4);

    if (lastButton1 == HIGH && button1 == LOW) {
        Serial.println(">>> BIANCO GPIO19 -> PREMUTO");
        roboEyes.setMood(HAPPY);
        roboEyes.anim_laugh();
        reactionUntil = millis() + 2000;
        wasReacting = true;
    }

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

    if (lastButton3 == HIGH && button3 == LOW) {
        Serial.println(">>> BLU GPIO18 -> PREMUTO");
        roboEyes.setMood(ANGRY);
        reactionUntil = millis() + 1000;
        wasReacting = true;
    }

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

    if (wasReacting && millis() >= reactionUntil) {
        if (tiredMode) roboEyes.setMood(TIRED);
        else roboEyes.setMood(DEFAULT);
        wasReacting = false;
        Serial.println(">>> REAZIONE FINITA");
    }

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float x = a.acceleration.x;
    float y = a.acceleration.y;
    float gyroX = g.gyro.x;
    float gyroY = g.gyro.y;
    float gyroZ = g.gyro.z;

    float movimento = abs(gyroX) + abs(gyroY) + abs(gyroZ);

    if (movimento > 8.0 && !wasShaking) {
        Serial.println(">>> SCOSSA!");
        roboEyes.setMood(ANGRY);
        shakeUntil = millis() + 800;
        wasShaking = true;
    }

    if (wasShaking && millis() > shakeUntil) {
        if (tiredMode) roboEyes.setMood(TIRED);
        else roboEyes.setMood(DEFAULT);
        wasShaking = false;
        Serial.println(">>> SCOSSA FINITA");
    }

    const float threshold = 2.5;
    bool right = x > threshold;
    bool left = x < -threshold;
    bool forward = y < -threshold;
    bool backward = y > threshold;

    if (right && backward) roboEyes.setPosition(NE);
    else if (right && forward) roboEyes.setPosition(NW);
    else if (left && backward) roboEyes.setPosition(SE);
    else if (left && forward) roboEyes.setPosition(SW);
    else if (right) roboEyes.setPosition(N);
    else if (left) roboEyes.setPosition(S);
    else if (backward) roboEyes.setPosition(E);
    else if (forward) roboEyes.setPosition(W);
    else roboEyes.setPosition(DEFAULT);

    delay(20);
}
