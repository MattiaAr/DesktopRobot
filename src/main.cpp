#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 18
#define BUTTON_PIN 19

Servo motor;

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    motor.setPeriodHertz(50);
    motor.attach(SERVO_PIN, 500, 2400);
    motor.write(90);

    Serial.println("=== SERVO TEST MINIMAL ===");
    Serial.println("Premi il pulsante bianco per muovere il servo.");
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("MOVIMENTO SERVO");

        motor.write(0);
        delay(1000);
        motor.write(180);
        delay(1000);
        motor.write(0);
        delay(1000);

        while (digitalRead(BUTTON_PIN) == LOW) {
            delay(10);
        }
    }
}
