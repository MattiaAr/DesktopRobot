#include <Arduino.h>

#define SERVO_PIN 18
#define GND_PIN 4

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== GPIO18 DIRECT OUTPUT TEST ===");
    Serial.println("Misura VDC tra GPIO18 e GND.");

    pinMode(SERVO_PIN, OUTPUT);

    Serial.println("GPIO18 = HIGH per 5 secondi (atteso ~3.3V)");
    digitalWrite(SERVO_PIN, HIGH);
    delay(5000);

    Serial.println("GPIO18 = LOW per 5 secondi (atteso ~0V)");
    digitalWrite(SERVO_PIN, LOW);
    delay(5000);

    Serial.println("=== TEST COMPLETATO ===");
}

void loop() {
    delay(1000);
}
