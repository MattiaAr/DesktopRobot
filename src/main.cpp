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
#define BUTTON_UP 19
#define BUTTON_DOWN 23
#define BUTTON_BACK 18
#define BUTTON_SELECT 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
RoboEyes<Adafruit_SSD1306> roboEyes(display);
Preferences prefs;

enum Screen {
  ROBOT, MENU, GAMES, SETTINGS, SYSTEM_MENU, SYSTEM_INFO, SYSTEM_RESOURCES,
  SYSTEM_HARDWARE, SYSTEM_DIAGNOSTICS, SYSTEM_REBOOT, EYE_CAROUSEL,
  SETTINGS_BEHAVIOR, SETTINGS_DISPLAY, SETTINGS_CONTROLS, SETTINGS_SOUND, SETTINGS_RESET
};

Screen currentScreen = ROBOT;

const char* menuLabels[] = {"GIOCHI", "ROBOT", "IMPOSTAZIONI", "SISTEMA"};
const int menuItems = 4;
int menuIndex = 0;

const char* settingsLabels[] = {"OCCHI", "COMPORTAMENTO", "DISPLAY", "CONTROLLI", "SUONI", "RIPRISTINA"};
const int settingsItems = 6;
int settingsIndex = 0;

const char* systemLabels[] = {"INFO ROBOT", "RISORSE", "HARDWARE", "DIAGNOSTICA", "RIAVVIA"};
const int systemItems = 5;
int systemIndex = 0;

const char* behaviorLabels[] = {"PERSONALITA", "IDLE", "REAZIONI", "NOIA"};
const int behaviorItems = 4;
int behaviorIndex = 0;

const char* displayLabels[] = {"LUMINOSITA", "TIMEOUT", "OROLOGIO", "ANIMAZIONI"};
const int displayItems = 4;
int displayIndex = 0;

const char* controlLabels[] = {"MAPPATURA", "MENU HOLD"};
int controlIndex = 0;

const char* soundLabels[] = {"VOLUME", "SUONI UI", "REAZIONI"};
int soundIndex = 0;

const int EYE_COUNT = 6;
const char* eyeNames[EYE_COUNT] = {"DEFAULT", "HAPPY", "ANGRY", "TIRED", "CURIOUS", "SLEEPY"};
int eyeIndex = 0;

bool clockEnabled = true;
int displayBrightness = 255;
int displayTimeout = 0;
bool uiSounds = true;

bool lastUp = HIGH, lastDown = HIGH, lastBack = HIGH, lastSelect = HIGH;
const unsigned long INPUT_DEBOUNCE = 60;
unsigned long lastInputTime = 0;
const unsigned long MENU_HOLD_TIME = 2500;
unsigned long bothButtonsStart = 0;
bool menuHoldTriggered = false;
unsigned long lastDebugPrint = 0;

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

void applyEyeModel();
void drawMenu();
void drawSettings();
void drawSystemMenu();
void drawEyeCarousel();
void drawBehavior();
void drawDisplaySettings();
void drawControls();
void drawSounds();
void drawReset();
void drawSystemInfo();
void drawResources();
void drawHardware();
void drawDiagnostics();
void drawReboot();
void drawGames();
void drawBar(int x, int y, int w, int h, int percent);
void saveSettings();
void loadSettings();
void resetSettings();

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
  if (currentScreen == ROBOT) drawRobot();
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
      Serial.println("DEBUG >>> BIANCO + ROSSO RILEVATI");
    }
    if (!menuHoldTriggered && millis() - bothButtonsStart >= MENU_HOLD_TIME) {
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
    if (up) { menuIndex = (menuIndex + menuItems - 1) % menuItems; drawMenu(); }
    if (down) { menuIndex = (menuIndex + 1) % menuItems; drawMenu(); }
    if (select) {
      if (menuIndex == 0) { currentScreen = GAMES; drawGames(); }
      if (menuIndex == 1) { currentScreen = ROBOT; applyEyeModel(); }
      if (menuIndex == 2) { currentScreen = SETTINGS; settingsIndex = 0; drawSettings(); }
      if (menuIndex == 3) { currentScreen = SYSTEM_MENU; systemIndex = 0; drawSystemMenu(); }
    }
    return;
  }

  if (currentScreen == SETTINGS) {
    if (up) { settingsIndex = (settingsIndex + settingsItems - 1) % settingsItems; drawSettings(); }
    if (down) { settingsIndex = (settingsIndex + 1) % settingsItems; drawSettings(); }
    if (select) {
      switch (settingsIndex) {
        case 0: currentScreen = EYE_CAROUSEL; drawEyeCarousel(); break;
        case 1: currentScreen = SETTINGS_BEHAVIOR; behaviorIndex = 0; drawBehavior(); break;
        case 2: currentScreen = SETTINGS_DISPLAY; displayIndex = 0; drawDisplaySettings(); break;
        case 3: currentScreen = SETTINGS_CONTROLS; controlIndex = 0; drawControls(); break;
        case 4: currentScreen = SETTINGS_SOUND; soundIndex = 0; drawSounds(); break;
        case 5: currentScreen = SETTINGS_RESET; drawReset(); break;
      }
    }
    if (back) { currentScreen = MENU; drawMenu(); }
    return;
  }

  if (currentScreen == SYSTEM_MENU) {
    if (up) { systemIndex = (systemIndex + systemItems - 1) % systemItems; drawSystemMenu(); }
    if (down) { systemIndex = (systemIndex + 1) % systemItems; drawSystemMenu(); }
    if (select) {
      switch (systemIndex) {
        case 0: currentScreen = SYSTEM_INFO; drawSystemInfo(); break;
        case 1: currentScreen = SYSTEM_RESOURCES; drawResources(); break;
        case 2: currentScreen = SYSTEM_HARDWARE; drawHardware(); break;
        case 3: currentScreen = SYSTEM_DIAGNOSTICS; drawDiagnostics(); break;
        case 4: currentScreen = SYSTEM_REBOOT; drawReboot(); break;
      }
    }
    if (back) { currentScreen = MENU; drawMenu(); }
    return;
  }

  if (currentScreen == EYE_CAROUSEL) {
    if (up) { eyeIndex = (eyeIndex + EYE_COUNT - 1) % EYE_COUNT; applyEyeModel(); drawEyeCarousel(); }
    if (down) { eyeIndex = (eyeIndex + 1) % EYE_COUNT; applyEyeModel(); drawEyeCarousel(); }
    if (select) { saveSettings(); currentScreen = SETTINGS; drawSettings(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_BEHAVIOR) {
    if (up) { behaviorIndex = (behaviorIndex + behaviorItems - 1) % behaviorItems; drawBehavior(); }
    if (down) { behaviorIndex = (behaviorIndex + 1) % behaviorItems; drawBehavior(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_DISPLAY) {
    if (up) { displayIndex = (displayIndex + displayItems - 1) % displayItems; drawDisplaySettings(); }
    if (down) { displayIndex = (displayIndex + 1) % displayItems; drawDisplaySettings(); }
    if (select) {
      if (displayIndex == 0) {
        displayBrightness += 32;
        if (displayBrightness > 255) displayBrightness = 32;
        display.ssd1306_command(SSD1306_SETCONTRAST);
        display.ssd1306_command(displayBrightness);
      } else if (displayIndex == 2) clockEnabled = !clockEnabled;
      saveSettings();
      drawDisplaySettings();
    }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_CONTROLS) {
    if (up) { controlIndex = 1 - controlIndex; drawControls(); }
    if (down) { controlIndex = 1 - controlIndex; drawControls(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_SOUND) {
    if (up) { soundIndex = (soundIndex + 2) % 3; drawSounds(); }
    if (down) { soundIndex = (soundIndex + 1) % 3; drawSounds(); }
    if (select && soundIndex == 1) { uiSounds = !uiSounds; saveSettings(); drawSounds(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_RESET) {
    if (select) { resetSettings(); currentScreen = SETTINGS; drawSettings(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SYSTEM_REBOOT) {
    if (select) ESP.restart();
    if (back) { currentScreen = SYSTEM_MENU; drawSystemMenu(); }
    return;
  }

  if (back) { currentScreen = SYSTEM_MENU; drawSystemMenu(); }
  if (select && currentScreen == SYSTEM_DIAGNOSTICS) drawDiagnostics();
}

void drawRobot() {
  roboEyes.update();
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float x = a.acceleration.x, y = a.acceleration.y;
  const float threshold = 2.5;
  bool right = x > threshold, left = x < -threshold;
  bool forward = y < -threshold, backward = y > threshold;
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

void drawList(const char* title, const char* const* labels, int count, int selected) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println(title); display.drawLine(0,9,127,9,SSD1306_WHITE);
  int first = max(0, min(selected - 1, count - 4));
  for (int i=0; i<4 && first+i<count; i++) {
    int idx=first+i, y=14+i*12;
    display.setCursor(2,y); display.print(idx==selected ? ">" : " ");
    display.setCursor(12,y); display.println(labels[idx]);
  }
  display.display();
}

void drawMenu() { drawList("DESKTOP ROBOT OS", menuLabels, menuItems, menuIndex); }
void drawSettings() { drawList("IMPOSTAZIONI", settingsLabels, settingsItems, settingsIndex); }
void drawSystemMenu() { drawList("SISTEMA", systemLabels, systemItems, systemIndex); }

void drawEyeCarousel() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("MODELLO OCCHI"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  int prev=(eyeIndex+EYE_COUNT-1)%EYE_COUNT, next=(eyeIndex+1)%EYE_COUNT;
  display.setCursor(0,24); display.print("< "); display.println(eyeNames[prev]);
  display.setCursor(38,24); display.print("["); display.print(eyeNames[eyeIndex]); display.print("]");
  display.setCursor(83,24); display.print(eyeNames[next]); display.print(" >");
  // Anteprima monocromatica
  display.fillRoundRect(43,39,17,10,3,SSD1306_WHITE);
  display.fillRoundRect(68,39,17,10,3,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK); display.setCursor(47,40); display.print("o"); display.setCursor(72,40); display.print("o");
  display.setTextColor(SSD1306_WHITE); display.setCursor(2,55); display.print("SU/GIU cambia  NERO OK");
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

void drawBehavior() { drawList("COMPORTAMENTO", behaviorLabels, behaviorItems, behaviorIndex); }

void drawDisplaySettings() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("DISPLAY"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  int bright=(displayBrightness*100)/255;
  const char* clockText=clockEnabled?"ON":"OFF";
  const char* timeoutText=displayTimeout==0?"OFF":"30s";
  const char* vals[]={"","","", "ON"};
  for(int i=0;i<4;i++){
    int y=14+i*12; display.setCursor(2,y); display.print(i==displayIndex?">":" ");
    display.setCursor(12,y); display.print(displayLabels[i]);
    if(i==0){display.setCursor(92,y);display.printf("%d%%",bright);}
    if(i==1){display.setCursor(92,y);display.print(timeoutText);}
    if(i==2){display.setCursor(92,y);display.print(clockText);}
    if(i==3){display.setCursor(92,y);display.print(vals[i]);}
  }
  display.display();
}

void drawControls() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("CONTROLLI"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(2,14); display.print(controlIndex==0?"> ":"  "); display.println("MAPPATURA");
  display.setCursor(8,26); display.println("B  SU   R  GIU");
  display.setCursor(8,38); display.println("BL INDIETRO  N OK");
  display.setCursor(2,50); display.print(controlIndex==1?"> ":"  "); display.println("MENU HOLD 2.5s");
  display.display();
}

void drawSounds() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("SUONI"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  for(int i=0;i<3;i++){
    int y=14+i*12; display.setCursor(2,y); display.print(i==soundIndex?">":" ");
    display.setCursor(12,y); display.print(soundLabels[i]);
    if(i==0){display.setCursor(94,y);display.print("--");}
    if(i==1){display.setCursor(94,y);display.print(uiSounds?"ON":"OFF");}
    if(i==2){display.setCursor(94,y);display.print("ON");}
  }
  display.display();
}

void drawReset() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("RIPRISTINA"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(4,24); display.println("Impostazioni predefinite?");
  display.setCursor(4,40); display.println("NERO = SI");
  display.setCursor(4,52); display.println("BLU = NO"); display.display();
}

void drawSystemInfo() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("INFO ROBOT"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(2,14); display.println("DESKTOPROBOT");
  display.setCursor(2,26); display.println("OS       v0.3");
  display.setCursor(2,38); display.println("FIRMWARE v0.3.0");
  display.setCursor(2,50); display.printf("UPTIME %lus", millis()/1000UL);
  display.display();
}

void drawResources() {
  uint32_t freeHeap=ESP.getFreeHeap(), totalHeap=ESP.getHeapSize();
  uint32_t minHeap=ESP.getMinFreeHeap();
  uint32_t sketch=ESP.getSketchSize(), freeSketch=ESP.getFreeSketchSpace();
  int ramPct=totalHeap ? (int)((totalHeap-freeHeap)*100UL/totalHeap) : 0;
  int flashPct=(sketch+freeSketch) ? (int)(sketch*100UL/(sketch+freeSketch)) : 0;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("RISORSE"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(2,13); display.printf("CPU  %u MHz",ESP.getCpuFreqMHz());
  display.setCursor(2,25); display.printf("RAM  %3d%%",ramPct); drawBar(67,25,58,6,ramPct);
  display.setCursor(2,37); display.printf("FREE %lu KB",(unsigned long)(freeHeap/1024));
  display.setCursor(2,49); display.printf("MIN  %lu KB",(unsigned long)(minHeap/1024));
  display.setCursor(70,37); display.printf("FLASH %d%%",flashPct);
  display.display();
}

void drawBar(int x,int y,int w,int h,int percent){
  percent=constrain(percent,0,100); display.drawRect(x,y,w,h,SSD1306_WHITE);
  int fill=(w-2)*percent/100; if(fill>0) display.fillRect(x+1,y+1,fill,h-2,SSD1306_WHITE);
}

void drawHardware() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("HARDWARE"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(2,14); display.println("ESP32        OK");
  display.setCursor(2,25); display.println("OLED  0x3C   OK");
  display.setCursor(2,36); display.println("MPU   0x68   OK");
  display.setCursor(2,47); display.println("BTN   19/23/18/5");
  display.display();
}

void drawDiagnostics() {
  Wire.beginTransmission(OLED_ADDR); bool oledBus=Wire.endTransmission()==0;
  Wire.beginTransmission(0x68); bool mpuBus=Wire.endTransmission()==0;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("DIAGNOSTICA"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(2,14); display.printf("I2C       %s",oledBus&&mpuBus?"OK":"ERR");
  display.setCursor(2,26); display.printf("OLED      %s",oledBus?"OK":"ERR");
  display.setCursor(2,38); display.printf("MPU6050   %s",mpuBus?"OK":"ERR");
  display.setCursor(2,50); display.println("NERO = RITESTA");
  display.display();
}

void drawReboot() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("RIAVVIA ROBOT"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(5,25); display.println("NERO = RIAVVIA");
  display.setCursor(5,39); display.println("BLU  = ANNULLA"); display.display();
}

void drawGames() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println("GIOCHI"); display.drawLine(0,9,127,9,SSD1306_WHITE);
  display.setCursor(5,25); display.println("In sviluppo");
  display.setCursor(5,39); display.println("Snake / Dino / Pong");
  display.setCursor(5,55); display.println("BLU = indietro"); display.display();
}

void loadSettings() {
  prefs.begin("desktop",false);
  eyeIndex=prefs.getInt("eye",0); clockEnabled=prefs.getBool("clock",true);
  displayBrightness=prefs.getInt("bright",255); displayTimeout=prefs.getInt("timeout",0);
  uiSounds=prefs.getBool("uisound",true);
  eyeIndex=constrain(eyeIndex,0,EYE_COUNT-1); displayBrightness=constrain(displayBrightness,32,255);
  display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(displayBrightness);
}

void saveSettings() {
  prefs.putInt("eye",eyeIndex); prefs.putBool("clock",clockEnabled);
  prefs.putInt("bright",displayBrightness); prefs.putInt("timeout",displayTimeout);
  prefs.putBool("uisound",uiSounds);
}

void resetSettings() {
  eyeIndex=0; clockEnabled=true; displayBrightness=255; displayTimeout=0; uiSounds=true;
  saveSettings(); display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(255); applyEyeModel();
}
