#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FluxGarage_RoboEyes.h>
#include <Preferences.h>

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
Preferences prefs;

enum Screen {
    ROBOT,
    MENU,
    GAMES,
    SETTINGS,
    SYSTEM_INFO,
    SETTINGS_EYES,
    SETTINGS_BEHAVIOR,
    SETTINGS_DISPLAY,
    SETTINGS_CONTROLS,
    SETTINGS_SOUND,
    SETTINGS_RESET,
    SYSTEM_RESOURCES,
    SYSTEM_HARDWARE,
    SYSTEM_DIAGNOSTICS,
    SYSTEM_REBOOT,
    EYE_CAROUSEL
};

Screen currentScreen = ROBOT;
int menuIndex = 0;
const int menuItems = 4;
const char* menuLabels[menuItems] = {"GIOCHI", "ROBOT", "IMPOSTAZIONI", "SISTEMA"};

int settingsIndex = 0;
const int settingsItems = 6;
const char* settingsLabels[settingsItems] = {"OCCHI", "COMPORTAMENTO", "DISPLAY", "CONTROLLI", "SUONI", "RIPRISTINA"};

int systemIndex = 0;
const int systemItems = 5;
const char* systemLabels[systemItems] = {"INFO ROBOT", "RISORSE", "HARDWARE", "DIAGNOSTICA", "RIAVVIA"};

int behaviorIndex = 0;
const int behaviorItems = 4;
const char* behaviorLabels[behaviorItems] = {"PERSONALITA", "IDLE", "REAZIONI", "NOIA"};

int displayIndex = 0;
const int displayItems = 4;
const char* displayLabels[displayItems] = {"LUMINOSITA", "TIMEOUT", "OROLOGIO", "ANIMAZIONI"};

int controlsIndex = 0;
const int controlsItems = 2;
const char* controlsLabels[controlsItems] = {"MAPPATURA", "MENU HOLD"};

int soundIndex = 0;
const int soundItems = 3;
const char* soundLabels[soundItems] = {"VOLUME", "SUONI UI", "REAZIONI"};

bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastBack = HIGH;
bool lastSelect = HIGH;

const unsigned long INPUT_DEBOUNCE = 60;
unsigned long lastInputTime = 0;

const unsigned long MENU_HOLD_TIME = 2500;
unsigned long bothButtonsStart = 0;
bool menuHoldTriggered = false;
unsigned long lastDebugPrint = 0;

// Impostazioni persistenti
int eyeIndex = 0;
bool clockEnabled = true;
int displayBrightness = 255;
int displayTimeout = 0;
bool uiSounds = true;

const int EYE_COUNT = 6;
const char* eyeNames[EYE_COUNT] = {"DEFAULT", "HAPPY", "ANGRY", "TIRED", "CURIOUS", "SLEEPY"};

void drawMenu();
void drawSettingsMenu();
void drawSystemMenu();
void drawRobot();
void debugMenuCombo();
void handleInput();
void enterSelectedItem();
void drawInfoScreen(const char* title, const char* line1, const char* line2);
void drawListScreen(const char* title, const char* const* labels, int count, int selected);
void applyEyeModel();
void drawEyeCarousel();
void drawResources();
void drawHardware();
void drawDiagnostics();
void drawBehaviorMenu();
void drawDisplayMenu();
void drawControlsMenu();
void drawSoundMenu();
void drawResetScreen();
void drawConfirmReboot();
void saveSettings();
void loadSettings();
void resetSettings();

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

    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_BACK, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);

    loadSettings();
    applyEyeModel();

    Serial.println("BIANCO GPIO19 -> SU");
    Serial.println("ROSSO GPIO23  -> GIU");
    Serial.println("BLU GPIO18    -> INDIETRO");
    Serial.println("NERO GPIO5    -> SELEZIONA");
    Serial.println("BIANCO + ROSSO 2.5s -> MENU OS");
    Serial.println("ROBOT PRONTO!");
}

void loop() {
    debugMenuCombo();
    handleInput();

    if (currentScreen == ROBOT) {
        drawRobot();
    }
}

void debugMenuCombo() {
    bool whiteHeld = digitalRead(BUTTON_UP) == LOW;
    bool redHeld = digitalRead(BUTTON_DOWN) == LOW;

    if (millis() - lastDebugPrint >= 500) {
        lastDebugPrint = millis();
        Serial.print("DEBUG | Bianco GPIO19: ");
        Serial.print(whiteHeld ? "PREMUTO" : "rilasciato");
        Serial.print(" | Rosso GPIO23: ");
        Serial.print(redHeld ? "PREMUTO" : "rilasciato");
        Serial.print(" | Screen: ");
        Serial.println(currentScreen == ROBOT ? "ROBOT" : "OS");
    }

    if (currentScreen != ROBOT) {
        bothButtonsStart = 0;
        menuHoldTriggered = false;
        return;
    }

    if (whiteHeld && redHeld) {
        if (bothButtonsStart == 0) {
            bothButtonsStart = millis();
            menuHoldTriggered = false;
            Serial.println("DEBUG >>> BIANCO + ROSSO RILEVATI");
        }

        unsigned long heldTime = millis() - bothButtonsStart;
        if (!menuHoldTriggered && heldTime >= MENU_HOLD_TIME) {
            currentScreen = MENU;
            menuIndex = 0;
            menuHoldTriggered = true;
            bothButtonsStart = 0;
            Serial.println(">>> MENU OS APERTO");
            drawMenu();
        }
    } else {
        bothButtonsStart = 0;
        menuHoldTriggered = false;
    }
}

void handleInput() {
    if (currentScreen == ROBOT) return;

    bool up = buttonPressed(BUTTON_UP, lastUp);
    bool down = buttonPressed(BUTTON_DOWN, lastDown);
    bool back = buttonPressed(BUTTON_BACK, lastBack);
    bool select = buttonPressed(BUTTON_SELECT, lastSelect);

    if (currentScreen == MENU) {
        if (up) { menuIndex--; if (menuIndex < 0) menuIndex = menuItems - 1; drawMenu(); }
        if (down) { menuIndex++; if (menuIndex >= menuItems) menuIndex = 0; drawMenu(); }
        if (select) enterSelectedItem();
        return;
    }

    if (currentScreen == SETTINGS) {
        if (up) { settingsIndex--; if (settingsIndex < 0) settingsIndex = settingsItems - 1; drawSettingsMenu(); }
        if (down) { settingsIndex++; if (settingsIndex >= settingsItems) settingsIndex = 0; drawSettingsMenu(); }
        if (select) {
            switch (settingsIndex) {
                case 0: currentScreen = SETTINGS_EYES; drawSettingsMenu(); break;
                case 1: currentScreen = SETTINGS_BEHAVIOR; behaviorIndex = 0; drawBehaviorMenu(); break;
                case 2: currentScreen = SETTINGS_DISPLAY; displayIndex = 0; drawDisplayMenu(); break;
                case 3: currentScreen = SETTINGS_CONTROLS; controlsIndex = 0; drawControlsMenu(); break;
                case 4: currentScreen = SETTINGS_SOUND; soundIndex = 0; drawSoundMenu(); break;
                case 5: currentScreen = SETTINGS_RESET; drawResetScreen(); break;
            }
        }
        return;
    }

    if (currentScreen == SYSTEM_INFO) {
        if (back || select) { currentScreen = SYSTEM_INFO; }
        if (back) { currentScreen = MENU; drawMenu(); }
        return;
    }

    if (currentScreen == SYSTEM_RESOURCES) {
        if (back) { currentScreen = SYSTEM_INFO; drawSystemMenu(); }
        return;
    }

    if (currentScreen == SYSTEM_HARDWARE) {
        if (back) { currentScreen = SYSTEM_INFO; drawSystemMenu(); }
        return;
    }

    if (currentScreen == SYSTEM_DIAGNOSTICS) {
        if (select) drawDiagnostics();
        if (back) { currentScreen = SYSTEM_INFO; drawSystemMenu(); }
        return;
    }

    if (currentScreen == SYSTEM_REBOOT) {
        if (up || down) { drawConfirmReboot(); }
        if (select) {
            Serial.println("RIAVVIO RICHIESTO");
            delay(200);
            ESP.restart();
        }
        if (back) { currentScreen = SYSTEM_INFO; drawSystemMenu(); }
        return;
    }

    if (currentScreen == EYE_CAROUSEL) {
        if (up) { eyeIndex--; if (eyeIndex < 0) eyeIndex = EYE_COUNT - 1; applyEyeModel(); drawEyeCarousel(); }
        if (down) { eyeIndex++; if (eyeIndex >= EYE_COUNT) eyeIndex = 0; applyEyeModel(); drawEyeCarousel(); }
        if (select) { saveSettings(); currentScreen = SETTINGS; drawSettingsMenu(); }
        if (back) { applyEyeModel(); currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    if (currentScreen == SETTINGS_EYES) {
        currentScreen = EYE_CAROUSEL;
        drawEyeCarousel();
        return;
    }

    if (currentScreen == SETTINGS_BEHAVIOR) {
        if (up) { behaviorIndex--; if (behaviorIndex < 0) behaviorIndex = behaviorItems - 1; drawBehaviorMenu(); }
        if (down) { behaviorIndex++; if (behaviorIndex >= behaviorItems) behaviorIndex = 0; drawBehaviorMenu(); }
        if (select) { Serial.print("BEHAVIOR > "); Serial.println(behaviorLabels[behaviorIndex]); }
        if (back) { currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    if (currentScreen == SETTINGS_DISPLAY) {
        if (up) { displayIndex--; if (displayIndex < 0) displayIndex = displayItems - 1; drawDisplayMenu(); }
        if (down) { displayIndex++; if (displayIndex >= displayItems) displayIndex = 0; drawDisplayMenu(); }
        if (select) {
            if (displayIndex == 0) { displayBrightness += 32; if (displayBrightness > 255) displayBrightness = 32; display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(displayBrightness); }
            if (displayIndex == 2) clockEnabled = !clockEnabled;
            saveSettings(); drawDisplayMenu();
        }
        if (back) { currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    if (currentScreen == SETTINGS_CONTROLS) {
        if (up) { controlsIndex--; if (controlsIndex < 0) controlsIndex = controlsItems - 1; drawControlsMenu(); }
        if (down) { controlsIndex++; if (controlsIndex >= controlsItems) controlsIndex = 0; drawControlsMenu(); }
        if (back) { currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    if (currentScreen == SETTINGS_SOUND) {
        if (up) { soundIndex--; if (soundIndex < 0) soundIndex = soundItems - 1; drawSoundMenu(); }
        if (down) { soundIndex++; if (soundIndex >= soundItems) soundIndex = 0; drawSoundMenu(); }
        if (select && soundIndex == 1) { uiSounds = !uiSounds; saveSettings(); drawSoundMenu(); }
        if (back) { currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    if (currentScreen == SETTINGS_RESET) {
        if (select) { resetSettings(); currentScreen = SETTINGS; drawSettingsMenu(); }
        if (back) { currentScreen = SETTINGS; drawSettingsMenu(); }
        return;
    }

    // Schermate semplici
    if (currentScreen == GAMES) {
        if (back) { currentScreen = MENU; drawMenu(); }
        return;
    }
}

void enterSelectedItem() {
    switch (menuIndex) {
        case 0:
            currentScreen = GAMES;
            drawInfoScreen("GIOCHI", "Nessun gioco", "in questa versione");
            break;
        case 1:
            currentScreen = ROBOT;
            roboEyes.setIdleMode(OFF);
            applyEyeModel();
            break;
        case 2:
            currentScreen = SETTINGS;
            settingsIndex = 0;
            drawSettingsMenu();
            break;
        case 3:
            currentScreen = SYSTEM_INFO;
            systemIndex = 0;
            drawSystemMenu();
            break;
    }
}

void drawRobot() {
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

void drawMenu() {
    drawListScreen("DESKTOP ROBOT OS", menuLabels, menuItems, menuIndex);
}

void drawSettingsMenu() {
    drawListScreen("IMPOSTAZIONI", settingsLabels, settingsItems, settingsIndex);
}

void drawSystemMenu() {
    drawListScreen("SISTEMA", systemLabels, systemItems, systemIndex);
}

void drawListScreen(const char* title, const char* const* labels, int count, int selected) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // Mostra massimo 4 voci, con selezione scorrevole.
    int first = selected - 2;
    if (first < 0) first = 0;
    if (first > count - 4) first = max(0, count - 4);

    for (int i = 0; i < 4 && first + i < count; i++) {
        int idx = first + i;
        int y = 14 + i * 12;
        display.setCursor(2, y);
        display.print(idx == selected ? ">" : " ");
        display.setCursor(12, y);
        display.println(labels[idx]);
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
    display.setCursor(5, 24); display.println(line1);
    display.setCursor(5, 38); display.println(line2);
    display.setCursor(5, 55); display.println("BLU = indietro");
    display.display();
}

void drawEyeCarousel() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MODELLO OCCHI");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    int prev = (eyeIndex - 1 + EYE_COUNT) % EYE_COUNT;
    int next = (eyeIndex + 1) % EYE_COUNT;

    display.setCursor(2, 29);
    display.print("< ");
    display.println(eyeNames[prev]);

    display.setCursor(41, 29);
    display.print("[");
    display.print(eyeNames[eyeIndex]);
    display.print("]");

    display.setCursor(83, 29);
    display.print(eyeNames[next]);
    display.print(" >");

    // Anteprima grafica semplice; il modello reale viene mostrato appena confermato.
    display.drawCircle(48, 49, 3, SSD1306_WHITE);
    display.drawCircle(80, 49, 3, SSD1306_WHITE);
    display.setCursor(3, 55);
    display.print("SU/GIU cambia  NERO ok");
    display.display();
}

void applyEyeModel() {
    switch (eyeIndex) {
        case 0: roboEyes.setMood(DEFAULT); break;
        case 1: roboEyes.setMood(HAPPY); break;
        case 2: roboEyes.setMood(ANGRY); break;
        case 3: roboEyes.setMood(TIRED); break;
        case 4: roboEyes.setMood(DEFAULT); break;
        case 5: roboEyes.setMood(TIRED); break;
    }
}

void drawBehaviorMenu() {
    drawListScreen("COMPORTAMENTO", behaviorLabels, behaviorItems, behaviorIndex);
}

void drawDisplayMenu() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("DISPLAY");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    const char* values[4];
    static char brightnessText[12];
    static char timeoutText[12];
    snprintf(brightnessText, sizeof(brightnessText), "%d%%", (displayBrightness * 100) / 255);
    snprintf(timeoutText, sizeof(timeoutText), "%s", displayTimeout == 0 ? "OFF" : "30s");
    values[0] = brightnessText;
    values[1] = timeoutText;
    values[2] = clockEnabled ? "ON" : "OFF";
    values[3] = "ON";

    for (int i = 0; i < displayItems; i++) {
        int y = 14 + i * 12;
        display.setCursor(2, y); display.print(i == displayIndex ? ">" : " ");
        display.setCursor(12, y); display.print(displayLabels[i]);
        display.setCursor(92, y); display.println(values[i]);
    }
    display.display();
}

void drawControlsMenu() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("CONTROLLI");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    if (controlsIndex == 0) {
        display.setCursor(2, 14); display.println("> MAPPATURA");
        display.setCursor(8, 26); display.println("B  SU   R  GIU");
        display.setCursor(8, 38); display.println("BL INDIETRO  N OK");
    } else {
        display.setCursor(2, 14); display.println("> MENU HOLD");
        display.setCursor(8, 28); display.println("B + R = 2.5 sec");
    }
    display.setCursor(5, 55); display.println("BLU = indietro");
    display.display();
}

void drawSoundMenu() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("SUONI");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    for (int i = 0; i < soundItems; i++) {
        int y = 14 + i * 12;
        display.setCursor(2, y); display.print(i == soundIndex ? ">" : " ");
        display.setCursor(12, y); display.println(soundLabels[i]);
        if (i == 0) { display.setCursor(90, y); display.println("--"); }
        if (i == 1) { display.setCursor(90, y); display.println(uiSounds ? "ON" : "OFF"); }
        if (i == 2) { display.setCursor(90, y); display.println("ON"); }
    }
    display.display();
}

void drawResources() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("RISORSE");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t minHeap = ESP.getMinFreeHeap();
    uint32_t sketch = ESP.getSketchSize();
    uint32_t freeSketch = ESP.getFreeSketchSpace();

    int ramPct = totalHeap ? (int)((totalHeap - freeHeap) * 100UL / totalHeap) : 0;
    int flashPct = (sketch + freeSketch) ? (int)(sketch * 100UL / (sketch + freeSketch)) : 0;

    display.setCursor(2, 13); display.printf("CPU  %u MHz", ESP.getCpuFreqMHz());
    display.setCursor(2, 25); display.printf("RAM  %3d%%", ramPct); drawBar(67, 25, 58, 6, ramPct);
    display.setCursor(2, 37); display.printf("FREE %lu KB", (unsigned long)(freeHeap / 1024));
    display.setCursor(2, 49); display.printf("MIN  %lu KB", (unsigned long)(minHeap / 1024));
    display.setCursor(67, 37); display.printf("FLASH %d%%", flashPct);
    display.display();
}

void drawBar(int x, int y, int w, int h, int percent) {
    percent = constrain(percent, 0, 100);
    display.drawRect(x, y, w, h, SSD1306_WHITE);
    int fill = (w - 2) * percent / 100;
    if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void drawHardware() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("HARDWARE");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    display.setCursor(2, 14); display.println("ESP32        OK");
    display.setCursor(2, 25); display.println("OLED  0x3C   OK");
    display.setCursor(2, 36); display.println("MPU   0x68   OK");
    display.setCursor(2, 47); display.println("BTN   19/23/18/5");
    display.display();
}

void drawDiagnostics() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("DIAGNOSTICA");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    bool oledOk = display.width() == 128;
    bool mpuOk = mpu.begin(0x68, &Wire);
    Wire.beginTransmission(OLED_ADDR);
    bool i2cOk = Wire.endTransmission() == 0;

    display.setCursor(2, 14); display.printf("I2C       %s", i2cOk ? "OK" : "ERR");
    display.setCursor(2, 26); display.printf("OLED      %s", oledOk ? "OK" : "ERR");
    display.setCursor(2, 38); display.printf("MPU6050   %s", mpuOk ? "OK" : "ERR");
    display.setCursor(2, 50); display.println("NERO = ritesta");
    display.display();
}

void drawResetScreen() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("RIPRISTINA");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    display.setCursor(4, 23); display.println("Ripristina impostazioni");
    display.setCursor(4, 35); display.println("predefinite?");
    display.setCursor(4, 51); display.println("NERO = SI   BLU = NO");
    display.display();
}

void drawConfirmReboot() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0); display.println("RIAVVIA ROBOT");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    display.setCursor(8, 25); display.println("NERO = RIAVVIA");
    display.setCursor(8, 39); display.println("BLU  = ANNULLA");
    display.display();
}

void loadSettings() {
    prefs.begin("desktop", false);
    eyeIndex = prefs.getInt("eye", 0);
    clockEnabled = prefs.getBool("clock", true);
    displayBrightness = prefs.getInt("bright", 255);
    displayTimeout = prefs.getInt("timeout", 0);
    uiSounds = prefs.getBool("uisound", true);
    eyeIndex = constrain(eyeIndex, 0, EYE_COUNT - 1);
    displayBrightness = constrain(displayBrightness, 32, 255);
}

void saveSettings() {
    prefs.putInt("eye", eyeIndex);
    prefs.putBool("clock", clockEnabled);
    prefs.putInt("bright", displayBrightness);
    prefs.putInt("timeout", displayTimeout);
    prefs.putBool("uisound", uiSounds);
}

void resetSettings() {
    eyeIndex = 0;
    clockEnabled = true;
    displayBrightness = 255;
    displayTimeout = 0;
    uiSounds = true;
    saveSettings();
    applyEyeModel();
}
