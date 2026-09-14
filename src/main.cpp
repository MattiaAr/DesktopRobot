#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define BUTTON_UP 19       // Bianco
#define BUTTON_DOWN 23     // Rosso
#define BUTTON_BACK 18     // Blu
#define BUTTON_SELECT 5    // Nero

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);

enum Screen {
    MENU,
    GAMES,
    ROBOT,
    SETTINGS,
    SYSTEM_INFO
};

Screen currentScreen = MENU;
int menuIndex = 0;
const int menuItems = 4;
const char* menuLabels[menuItems] = {
    "GIOCHI",
    "ROBOT",
    "IMPOSTAZIONI",
    "SISTEMA"
};

bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastBack = HIGH;
bool lastSelect = HIGH;

// Debounce ridotto per rendere i pulsanti piu reattivi.
const unsigned long INPUT_DEBOUNCE = 60;
unsigned long lastInputTime = 0;

void drawMenu();
void drawInfoScreen(const char* title, const char* line1, const char* line2);
void handleInput();
void enterSelectedItem();

bool buttonPressed(int pin, bool &lastState) {
    bool state = digitalRead(pin);
    bool pressed = (lastState == HIGH && state == LOW);
    lastState = state;

    if (pressed && millis() - lastInputTime >= INPUT_DEBOUNCE) {
        lastInputTime = millis();
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== DESKTOP ROBOT OS v0.3 ===");

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

    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_BACK, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);

    Serial.println("BIANCO GPIO19 -> SU");
    Serial.println("ROSSO GPIO23  -> GIU");
    Serial.println("BLU GPIO18    -> INDIETRO");
    Serial.println("NERO GPIO5    -> SELEZIONA");
    Serial.println("OS PRONTO!");

    drawMenu();
}

void loop() {
    handleInput();

    if (currentScreen == ROBOT) {
        roboEyes.update();

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        float x = a.acceleration.x;
        float y = a.acceleration.y;
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
}

void handleInput() {
    bool up = buttonPressed(BUTTON_UP, lastUp);
    bool down = buttonPressed(BUTTON_DOWN, lastDown);
    bool back = buttonPressed(BUTTON_BACK, lastBack);
    bool select = buttonPressed(BUTTON_SELECT, lastSelect);

    if (currentScreen == MENU) {
        if (up) {
            menuIndex--;
            if (menuIndex < 0) menuIndex = menuItems - 1;
            drawMenu();
        }

        if (down) {
            menuIndex++;
            if (menuIndex >= menuItems) menuIndex = 0;
            drawMenu();
        }

        if (select) {
            enterSelectedItem();
        }
    } else {
        if (back) {
            currentScreen = MENU;
            drawMenu();
        }
    }
}

void enterSelectedItem() {
    switch (menuIndex) {
        case 0:
            currentScreen = GAMES;
            drawInfoScreen("GIOCHI", "Nessun gioco", "in questa versione");
            Serial.println("> GIOCHI");
            break;

        case 1:
            currentScreen = ROBOT;
            Serial.println("> ROBOT");
            roboEyes.setMood(DEFAULT);
            roboEyes.setPosition(DEFAULT);
            break;

        case 2:
            currentScreen = SETTINGS;
            drawInfoScreen("IMPOSTAZIONI", "In sviluppo", "v0.3");
            Serial.println("> IMPOSTAZIONI");
            break;

        case 3:
            currentScreen = SYSTEM_INFO;
            drawInfoScreen("SISTEMA", "DesktopRobot OS", "Versione 0.3");
            Serial.println("> SISTEMA");
            break;
    }
}

void drawMenu() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(24, 0);
    display.println("DESKTOP ROBOT OS");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    for (int i = 0; i < menuItems; i++) {
        int y = 16 + (i * 11);

        display.setCursor(8, y);
        if (i == menuIndex) display.print(">");
        else display.print(" ");

        display.setCursor(20, y);
        display.println(menuLabels[i]);
    }

    display.display();
}

void drawInfoScreen(const char* title, const char* line1, const char* line2) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(8, 25);
    display.println(line1);
    display.setCursor(8, 40);
    display.println(line2);

    display.setCursor(8, 55);
    display.println("BLU = indietro");

    display.display();
}
