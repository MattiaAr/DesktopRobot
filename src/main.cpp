#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>
#include <ESP32Servo.h>
#include "BehaviorEngine.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN_1 19
#define BUTTON_PIN_2 23
#define SERVO_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);
BehaviorEngine behavior;
Servo headServo;

bool lastButton1 = HIGH;
bool lastButton2 = HIGH;
bool tiredMode = false;
bool wasReacting = false;
bool wasShaking = false;
unsigned long reactionUntil = 0;
unsigned long shakeUntil = 0;

void applyBehaviorState() {
    switch (behavior.getState()) {
        case RobotState::STATE_NORMAL:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setCuriosity(OFF); roboEyes.setIdleMode(OFF); roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT); break;
        case RobotState::STATE_TIRED:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setCuriosity(OFF); roboEyes.setIdleMode(OFF); roboEyes.setMood(TIRED); roboEyes.setPosition(DEFAULT); break;
        case RobotState::STATE_HAPPY:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setMood(HAPPY); break;
        case RobotState::STATE_ANGRY:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setMood(ANGRY); break;
        case RobotState::STATE_CURIOUS:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setMood(DEFAULT); roboEyes.setCuriosity(ON); roboEyes.setIdleMode(ON, 1, 1); break;
        case RobotState::STATE_BORED:
            roboEyes.open(); roboEyes.setAutoblinker(ON, 3, 2); roboEyes.setCuriosity(OFF); roboEyes.setIdleMode(OFF); roboEyes.setMood(TIRED); roboEyes.setPosition(S); break;
        case RobotState::STATE_SLEEPING:
            // Disable the automatic blinker: otherwise RoboEyes reopens the eyes after close().
            roboEyes.setAutoblinker(OFF);
            roboEyes.setCuriosity(OFF);
            roboEyes.setIdleMode(OFF);
            roboEyes.close();
            break;
    }
}

void printBehaviorState() {
    Serial.print(">>> NUOVO STATO: ");
    switch (behavior.getState()) {
        case RobotState::STATE_NORMAL: Serial.println("NORMAL"); break;
        case RobotState::STATE_HAPPY: Serial.println("HAPPY"); break;
        case RobotState::STATE_TIRED: Serial.println("TIRED"); break;
        case RobotState::STATE_ANGRY: Serial.println("ANGRY"); break;
        case RobotState::STATE_CURIOUS: Serial.println("CURIOUS"); break;
        case RobotState::STATE_BORED: Serial.println("BORED"); break;
        case RobotState::STATE_SLEEPING: Serial.println("SLEEPING"); break;
    }
}

void setup() {
    Serial.begin(115200); delay(1000);
    Serial.println(); Serial.println("=== DESKTOP ROBOT v0.3 SERVO TEST ===");
    Wire.begin(SDA_PIN, SCL_PIN); Wire.setClock(100000);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { Serial.println("ERRORE OLED!"); while (true) delay(1000); }
    Serial.println("OLED OK!");
    if (!mpu.begin(0x68, &Wire)) { Serial.println("ERRORE MPU!"); while (true) delay(1000); }
    Serial.println("MPU OK!");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(OFF); roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT);
    pinMode(BUTTON_PIN_1, INPUT_PULLUP); pinMode(BUTTON_PIN_2, INPUT_PULLUP);

    // Servo on GPIO18. Power comes from the Shield; GPIO18 carries only the signal.
    headServo.setPeriodHertz(50);
    headServo.attach(SERVO_PIN, 500, 2400);
    headServo.write(90);
    Serial.println("SERVO GPIO18 OK - posizione centrale");
    delay(500);
    headServo.write(75);
    delay(300);
    headServo.write(105);
    delay(300);
    headServo.write(90);
    Serial.println("SERVO TEST AVVIO COMPLETATO");

    behavior.begin();
    Serial.println("PULSANTE 1 -> GPIO19"); Serial.println("PULSANTE 2 -> GPIO23");
    Serial.println("Behavior Engine OK!"); Serial.println("ROBOT PRONTO!");
}

void loop() {
    roboEyes.update(); behavior.update();
    if (behavior.stateChanged()) { printBehaviorState(); applyBehaviorState(); }

    bool button1 = digitalRead(BUTTON_PIN_1);
    if (lastButton1 == HIGH && button1 == LOW) {
        Serial.println(">>> PULSANTE 1 -> FELICE!"); behavior.interaction(); behavior.setState(RobotState::STATE_HAPPY, 2000);
        roboEyes.setMood(HAPPY); roboEyes.anim_laugh(); reactionUntil = millis() + 2000; wasReacting = true;
    }
    lastButton1 = button1;
    if (millis() < reactionUntil) { delay(10); return; }
    if (wasReacting) { Serial.println(">>> REAZIONE FINITA"); behavior.setState(tiredMode ? RobotState::STATE_TIRED : RobotState::STATE_NORMAL); applyBehaviorState(); wasReacting = false; }

    bool button2 = digitalRead(BUTTON_PIN_2);
    if (lastButton2 == HIGH && button2 == LOW) {
        tiredMode = !tiredMode; behavior.interaction();
        if (tiredMode) { Serial.println(">>> MODALITA' TIRED ON"); behavior.setState(RobotState::STATE_TIRED); }
        else { Serial.println(">>> MODALITA' TIRED OFF"); behavior.setState(RobotState::STATE_NORMAL); }
        applyBehaviorState(); delay(150);
    }
    lastButton2 = button2;

    sensors_event_t a, g, temp; mpu.getEvent(&a, &g, &temp);
    float x = a.acceleration.x, y = a.acceleration.y;
    float movimento = abs(g.gyro.x) + abs(g.gyro.y) + abs(g.gyro.z);
    if (movimento > 8.0 && !wasShaking) {
        Serial.println(">>> SCOSSA!"); behavior.interaction(); behavior.setState(RobotState::STATE_ANGRY, 800);
        roboEyes.setMood(ANGRY); shakeUntil = millis() + 800; wasShaking = true;
    }
    if (wasShaking && millis() > shakeUntil) {
        wasShaking = false; behavior.setState(tiredMode ? RobotState::STATE_TIRED : RobotState::STATE_NORMAL); applyBehaviorState(); Serial.println(">>> SCOSSA FINITA");
    }

    if (behavior.getState() != RobotState::STATE_CURIOUS && behavior.getState() != RobotState::STATE_SLEEPING) {
        const float threshold = 2.5;
        bool right = x > threshold, left = x < -threshold, forward = y < -threshold, backward = y > threshold;
        if (right && backward) roboEyes.setPosition(NE);
        else if (right && forward) roboEyes.setPosition(NW);
        else if (left && backward) roboEyes.setPosition(SE);
        else if (left && forward) roboEyes.setPosition(SW);
        else if (right) roboEyes.setPosition(N);
        else if (left) roboEyes.setPosition(S);
        else if (backward) roboEyes.setPosition(E);
        else if (forward) roboEyes.setPosition(W);
        else roboEyes.setPosition(DEFAULT);
    }
    delay(20);
}
