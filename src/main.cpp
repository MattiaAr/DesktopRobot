#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 18

Servo testServo;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== DESKTOP ROBOT - SERVO ISOLATED TEST ===");
    Serial.println("GPIO18 / 50Hz / impulsi diretti");

    testServo.setPeriodHertz(50);
    int channel = testServo.attach(SERVO_PIN, 500, 2400);

    Serial.print("SERVO attach channel = ");
    Serial.println(channel);
    Serial.print("SERVO attached = ");
    Serial.println(testServo.attached() ? "YES" : "NO");

    if (!testServo.attached()) {
        Serial.println("ERRORE: PWM servo non configurato.");
        return;
    }

    // Test con impulsi espliciti. Il servo dovrebbe muoversi chiaramente.
    Serial.println("TEST 1: 1500us (centro) - 2 secondi");
    testServo.writeMicroseconds(1500);
    delay(2000);

    Serial.println("TEST 2: 1000us (~0 gradi) - 2 secondi");
    testServo.writeMicroseconds(1000);
    delay(2000);

    Serial.println("TEST 3: 2000us (~180 gradi) - 2 secondi");
    testServo.writeMicroseconds(2000);
    delay(2000);

    Serial.println("TEST 4: 1000us (~0 gradi) - 2 secondi");
    testServo.writeMicroseconds(1000);
    delay(2000);

    Serial.println("=== TEST COMPLETATO ===");
}

void loop() {
    // Il test viene eseguito una sola volta al boot.
    delay(1000);
}
