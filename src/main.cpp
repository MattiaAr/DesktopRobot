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
  ROBOT, MENU, GAMES, GAMES_LIST, GAMES_MODE, GAMES_RECORDS, SETTINGS,
  SYSTEM_MENU, SYSTEM_INFO, SYSTEM_RESOURCES, SYSTEM_HARDWARE, SYSTEM_DIAGNOSTICS,
  SYSTEM_REBOOT, EYE_MENU, EYE_MODEL_CAROUSEL, EYE_EXPRESSION,
  SETTINGS_BEHAVIOR, SETTINGS_DISPLAY, SETTINGS_CONTROLS, SETTINGS_SOUND, SETTINGS_RESET
};
Screen currentScreen = ROBOT;

const char* menuLabels[] = {"GIOCHI", "ROBOT", "IMPOSTAZIONI", "SISTEMA"};
const int menuItems = 4;
int menuIndex = 0;
const char* gameLabels[] = {"GIOCA", "MODALITA", "RECORD"};
const int gameItems = 3;
int gameIndex = 0;
const char* gameListLabels[] = {"SNAKE", "DINO", "PONG"};
const int gameListItems = 3;
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

enum EyeModel { EYE_CLASSIC, EYE_ROUND, EYE_SQUARE, EYE_PIXEL, EYE_CYBER, EYE_CUTE, EYE_MINIMAL };
const int EYE_MODEL_COUNT = 7;
const char* eyeModelNames[EYE_MODEL_COUNT] = {"CLASSIC", "ROUND", "SQUARE", "PIXEL", "CYBER", "CUTE", "MINIMAL"};
int eyeModelIndex = 0;

enum EyeExpression { EXPR_DEFAULT, EXPR_HAPPY, EXPR_ANGRY, EXPR_TIRED, EXPR_CURIOUS };
const int EYE_EXPRESSION_COUNT = 5;
const char* eyeExpressionNames[EYE_EXPRESSION_COUNT] = {"DEFAULT", "HAPPY", "ANGRY", "TIRED", "CURIOUS"};
int eyeExpressionIndex = 0;

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

bool buttonPressed(int pin, bool &lastState) {
  bool state = digitalRead(pin);
  bool pressed = (lastState == HIGH && state == LOW);
  lastState = state;
  if (pressed && millis() - lastInputTime >= INPUT_DEBOUNCE) { lastInputTime = millis(); return true; }
  return false;
}

void handleMenuHold();
void handleInput();
void drawRobot();
void drawMenu();
void drawGames();
void drawGameList();
void drawGameMode();
void drawGameRecords();
void drawSettings();
void drawSystemMenu();
void drawEyeMenu();
void drawEyeModelCarousel();
void drawEyeExpression();
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
void drawList(const char* title, const char* const* labels, int count, int selected);
void drawBar(int x, int y, int w, int h, int percent);
void drawCustomEyes(float dirX, float dirY);
void drawEyePair(int leftX, int rightX, int topY, int w, int h, int pupilX, int pupilY, bool round, bool outline);
void drawEyeModelPreview(int index, int cx, int cy, int scale);
void applyEyeModel();
void applyEyeExpression();
void saveSettings();
void loadSettings();
void resetSettings();

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) while (true) delay(1000);
  if (!mpu.begin(0x68, &Wire)) while (true) delay(1000);
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
  applyEyeExpression();
}

void loop() {
  handleMenuHold();
  handleInput();
  if (currentScreen == ROBOT) drawRobot();
}

void handleMenuHold() {
  if (currentScreen != ROBOT) { bothButtonsStart = 0; menuHoldTriggered = false; return; }
  bool whiteHeld = digitalRead(BUTTON_UP) == LOW;
  bool redHeld = digitalRead(BUTTON_DOWN) == LOW;
  if (whiteHeld && redHeld) {
    if (bothButtonsStart == 0) bothButtonsStart = millis();
    if (!menuHoldTriggered && millis() - bothButtonsStart >= MENU_HOLD_TIME) {
      currentScreen = MENU;
      menuIndex = 0;
      menuHoldTriggered = true;
      bothButtonsStart = 0;
      lastUp = LOW;
      lastDown = LOW;
      drawMenu();
    }
  } else { bothButtonsStart = 0; menuHoldTriggered = false; }
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
      if (menuIndex == 0) { currentScreen = GAMES; gameIndex = 0; drawGames(); }
      else if (menuIndex == 1) { currentScreen = ROBOT; applyEyeModel(); applyEyeExpression(); }
      else if (menuIndex == 2) { currentScreen = SETTINGS; settingsIndex = 0; drawSettings(); }
      else { currentScreen = SYSTEM_MENU; systemIndex = 0; drawSystemMenu(); }
    }
    return;
  }

  if (currentScreen == GAMES) {
    if (up) { gameIndex = (gameIndex + gameItems - 1) % gameItems; drawGames(); }
    if (down) { gameIndex = (gameIndex + 1) % gameItems; drawGames(); }
    if (select) {
      if (gameIndex == 0) { currentScreen = GAMES_LIST; gameListIndex = 0; drawGameList(); }
      else if (gameIndex == 1) { currentScreen = GAMES_MODE; gameModeIndex = 0; drawGameMode(); }
      else { currentScreen = GAMES_RECORDS; drawGameRecords(); }
    }
    if (back) { currentScreen = MENU; drawMenu(); }
    return;
  }

  if (currentScreen == GAMES_LIST) {
    if (up) { gameListIndex = (gameListIndex + gameListItems - 1) % gameListItems; drawGameList(); }
    if (down) { gameListIndex = (gameListIndex + 1) % gameListItems; drawGameList(); }
    if (back) { currentScreen = GAMES; drawGames(); }
    return;
  }

  if (currentScreen == GAMES_MODE) {
    if (up) { gameModeIndex = (gameModeIndex + gameModeItems - 1) % gameModeItems; drawGameMode(); }
    if (down) { gameModeIndex = (gameModeIndex + 1) % gameModeItems; drawGameMode(); }
    if (back) { currentScreen = GAMES; drawGames(); }
    return;
  }

  if (currentScreen == GAMES_RECORDS) {
    if (back) { currentScreen = GAMES; drawGames(); }
    return;
  }

  if (currentScreen == SETTINGS) {
    if (up) { settingsIndex = (settingsIndex + settingsItems - 1) % settingsItems; drawSettings(); }
    if (down) { settingsIndex = (settingsIndex + 1) % settingsItems; drawSettings(); }
    if (select) {
      if (settingsIndex == 0) { currentScreen = EYE_MENU; drawEyeMenu(); }
      else if (settingsIndex == 1) { currentScreen = SETTINGS_BEHAVIOR; behaviorIndex = 0; drawBehavior(); }
      else if (settingsIndex == 2) { currentScreen = SETTINGS_DISPLAY; displayIndex = 0; drawDisplaySettings(); }
      else if (settingsIndex == 3) { currentScreen = SETTINGS_CONTROLS; controlIndex = 0; drawControls(); }
      else if (settingsIndex == 4) { currentScreen = SETTINGS_SOUND; soundIndex = 0; drawSounds(); }
      else { currentScreen = SETTINGS_RESET; drawReset(); }
    }
    if (back) { currentScreen = MENU; drawMenu(); }
    return;
  }

  if (currentScreen == EYE_MENU) {
    if (up || down || select) { currentScreen = EYE_MODEL_CAROUSEL; drawEyeModelCarousel(); }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == EYE_MODEL_CAROUSEL) {
    if (up) { eyeModelIndex = (eyeModelIndex + EYE_MODEL_COUNT - 1) % EYE_MODEL_COUNT; applyEyeModel(); drawEyeModelCarousel(); }
    if (down) { eyeModelIndex = (eyeModelIndex + 1) % EYE_MODEL_COUNT; applyEyeModel(); drawEyeModelCarousel(); }
    if (select) { saveSettings(); currentScreen = EYE_EXPRESSION; drawEyeExpression(); }
    if (back) { currentScreen = EYE_MENU; drawEyeMenu(); }
    return;
  }

  if (currentScreen == EYE_EXPRESSION) {
    if (up) { eyeExpressionIndex = (eyeExpressionIndex + EYE_EXPRESSION_COUNT - 1) % EYE_EXPRESSION_COUNT; applyEyeExpression(); drawEyeExpression(); }
    if (down) { eyeExpressionIndex = (eyeExpressionIndex + 1) % EYE_EXPRESSION_COUNT; applyEyeExpression(); drawEyeExpression(); }
    if (select) { saveSettings(); currentScreen = EYE_MENU; drawEyeMenu(); }
    if (back) { currentScreen = EYE_MENU; drawEyeMenu(); }
    return;
  }

  if (currentScreen == SYSTEM_MENU) {
    if (up) { systemIndex = (systemIndex + systemItems - 1) % systemItems; drawSystemMenu(); }
    if (down) { systemIndex = (systemIndex + 1) % systemItems; drawSystemMenu(); }
    if (select) {
      if (systemIndex == 0) { currentScreen = SYSTEM_INFO; drawSystemInfo(); }
      else if (systemIndex == 1) { currentScreen = SYSTEM_RESOURCES; drawResources(); }
      else if (systemIndex == 2) { currentScreen = SYSTEM_HARDWARE; drawHardware(); }
      else if (systemIndex == 3) { currentScreen = SYSTEM_DIAGNOSTICS; drawDiagnostics(); }
      else { currentScreen = SYSTEM_REBOOT; drawReboot(); }
    }
    if (back) { currentScreen = MENU; drawMenu(); }
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
      if (displayIndex == 0) { displayBrightness += 32; if (displayBrightness > 255) displayBrightness = 32; display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(displayBrightness); }
      else if (displayIndex == 2) clockEnabled = !clockEnabled;
      saveSettings(); drawDisplaySettings();
    }
    if (back) { currentScreen = SETTINGS; drawSettings(); }
    return;
  }

  if (currentScreen == SETTINGS_CONTROLS) {
    if (up || down) { controlIndex = 1 - controlIndex; drawControls(); }
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

  if (currentScreen == SYSTEM_DIAGNOSTICS) {
    if (select) drawDiagnostics();
    if (back) { currentScreen = SYSTEM_MENU; drawSystemMenu(); }
    return;
  }

  if (currentScreen == SYSTEM_INFO || currentScreen == SYSTEM_RESOURCES || currentScreen == SYSTEM_HARDWARE) {
    if (back) { currentScreen = SYSTEM_MENU; drawSystemMenu(); }
  }
}

void drawRobot() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float x = a.acceleration.x, y = a.acceleration.y;
  const float threshold = 2.5;
  int dirX = x > threshold ? 1 : (x < -threshold ? -1 : 0);
  int dirY = y > threshold ? 1 : (y < -threshold ? -1 : 0);

  if (eyeModelIndex == EYE_CLASSIC) {
    roboEyes.update();
    if (dirX > 0 && dirY > 0) roboEyes.setPosition(NE);
    else if (dirX > 0 && dirY < 0) roboEyes.setPosition(NW);
    else if (dirX < 0 && dirY > 0) roboEyes.setPosition(SE);
    else if (dirX < 0 && dirY < 0) roboEyes.setPosition(SW);
    else if (dirX > 0) roboEyes.setPosition(N);
    else if (dirX < 0) roboEyes.setPosition(S);
    else if (dirY > 0) roboEyes.setPosition(E);
    else if (dirY < 0) roboEyes.setPosition(W);
    else roboEyes.setPosition(DEFAULT);
  } else {
    display.clearDisplay();
    drawCustomEyes(dirX, dirY);
    display.display();
  }
  delay(20);
}

void drawList(const char* title, const char* const* labels, int count, int selected) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println(title); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  int first = max(0, min(selected - 1, count - 4));
  for (int i = 0; i < 4 && first + i < count; i++) {
    int idx = first + i, y = 14 + i * 12;
    display.setCursor(2, y); display.print(idx == selected ? ">" : " ");
    display.setCursor(12, y); display.println(labels[idx]);
  }
  display.display();
}
void drawMenu() { drawList("DESKTOP OS", menuLabels, menuItems, menuIndex); }
void drawGames() { drawList("GIOCHI", gameLabels, gameItems, gameIndex); }
void drawGameList() { drawList("SCEGLI GIOCO", gameListLabels, gameListItems, gameListIndex); }
void drawGameMode() { drawList("MODALITA", gameModeLabels, gameModeItems, gameModeIndex); }
void drawSettings() { drawList("IMPOSTAZIONI", settingsLabels, settingsItems, settingsIndex); }
void drawSystemMenu() { drawList("SISTEMA", systemLabels, systemItems, systemIndex); }

void drawGameRecords() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("RECORD"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setCursor(5, 22); display.println("SNAKE   ---");
  display.setCursor(5, 34); display.println("DINO    ---");
  display.setCursor(5, 46); display.println("PONG    ---"); display.display();
}

void drawEyeMenu() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("OCCHI"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setCursor(4, 18); display.println("MODELLO"); display.setCursor(82, 18); display.println(eyeModelNames[eyeModelIndex]);
  display.setCursor(4, 31); display.println("ESPRESS."); display.setCursor(82, 31); display.println(eyeExpressionNames[eyeExpressionIndex]);
  drawEyePair(43, 68, 42, 16, 10, 0, 0, true, false);
  display.setCursor(3, 56); display.print("N = MODELLO"); display.setCursor(82, 56); display.print("OK"); display.display();
}

void drawEyeModelPreview(int index, int cx, int cy, int scale) {
  int w = 18 * scale, h = 11 * scale, gap = 7 * scale;
  int left = cx - gap - w, right = cx + gap, top = cy - h / 2;
  if (index == EYE_CLASSIC) {
    display.fillRoundRect(left, top, w, h, 4 * scale, SSD1306_WHITE); display.fillRoundRect(right, top, w, h, 4 * scale, SSD1306_WHITE);
  } else if (index == EYE_ROUND) {
    display.fillCircle(left + w / 2, cy, h / 2 + 1, SSD1306_WHITE); display.fillCircle(right + w / 2, cy, h / 2 + 1, SSD1306_WHITE);
  } else if (index == EYE_SQUARE) {
    display.fillRect(left, top, w, h, SSD1306_WHITE); display.fillRect(right, top, w, h, SSD1306_WHITE);
  } else if (index == EYE_PIXEL) {
    int s = max(1, scale * 2);
    for (int py = 0; py < 5; py++) for (int px = 0; px < 4; px++) if (!((px == 0 || px == 3) && py == 0)) { display.fillRect(left + px * s, top + py * s, s, s, SSD1306_WHITE); display.fillRect(right + px * s, top + py * s, s, s, SSD1306_WHITE); }
  } else if (index == EYE_CYBER) {
    display.drawLine(left, top + h, left + w / 3, top, SSD1306_WHITE); display.drawLine(left + w / 3, top, left + w, top + h / 3, SSD1306_WHITE); display.drawLine(left, top + h, left + w, top + h, SSD1306_WHITE);
    display.drawLine(right, top + h / 3, right + 2 * w / 3, top, SSD1306_WHITE); display.drawLine(right + 2 * w / 3, top, right + w, top + h, SSD1306_WHITE); display.drawLine(right, top + h, right + w, top + h, SSD1306_WHITE);
  } else if (index == EYE_CUTE) {
    display.fillRoundRect(left, top, w, h, h / 2, SSD1306_WHITE); display.fillRoundRect(right, top, w, h, h / 2, SSD1306_WHITE);
    display.fillCircle(left + w / 2, cy - 1, max(1, scale), SSD1306_BLACK); display.fillCircle(right + w / 2, cy - 1, max(1, scale), SSD1306_BLACK);
  } else {
    display.fillRoundRect(left, cy - 2 * scale, w, 4 * scale, 2 * scale, SSD1306_WHITE); display.fillRoundRect(right, cy - 2 * scale, w, 4 * scale, 2 * scale, SSD1306_WHITE);
  }
}

void drawEyeModelCarousel() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("OCCHI / MODELLO"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  int prev = (eyeModelIndex + EYE_MODEL_COUNT - 1) % EYE_MODEL_COUNT, next = (eyeModelIndex + 1) % EYE_MODEL_COUNT;
  display.setCursor(2, 20); display.print("<"); display.setCursor(114, 20); display.print(">");
  display.setCursor(42, 20); display.print(eyeModelNames[eyeModelIndex]);
  drawEyeModelPreview(prev, 18, 39, 1); drawEyeModelPreview(eyeModelIndex, 64, 39, 2); drawEyeModelPreview(next, 110, 39, 1);
  display.setCursor(2, 56); display.print("B/R cambia"); display.setCursor(72, 56); display.print("N OK BL ind"); display.display();
}

void drawEyeExpression() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("OCCHI / ESPRESS."); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  int prev = (eyeExpressionIndex + EYE_EXPRESSION_COUNT - 1) % EYE_EXPRESSION_COUNT, next = (eyeExpressionIndex + 1) % EYE_EXPRESSION_COUNT;
  display.setCursor(3, 20); display.print("< "); display.print(eyeExpressionNames[prev]);
  display.setCursor(47, 20); display.print(eyeExpressionNames[eyeExpressionIndex]);
  display.setCursor(93, 20); display.print(eyeExpressionNames[next]); display.print(" >");
  drawEyeModelPreview(eyeModelIndex, 64, 39, 2);
  display.setCursor(4, 56); display.print("B/R cambia"); display.setCursor(76, 56); display.print("N OK"); display.display();
}

void applyEyeModel() { applyEyeExpression(); }
void applyEyeExpression() {
  if (eyeModelIndex != EYE_CLASSIC) return;
  if (eyeExpressionIndex == EXPR_HAPPY) roboEyes.setMood(HAPPY);
  else if (eyeExpressionIndex == EXPR_ANGRY) roboEyes.setMood(ANGRY);
  else if (eyeExpressionIndex == EXPR_TIRED) roboEyes.setMood(TIRED);
  else roboEyes.setMood(DEFAULT);
}

void drawCustomEyes(float dirX, float dirY) {
  int pupilX = (int)(dirX * 4), pupilY = (int)(dirY * 3);
  if (eyeModelIndex == EYE_ROUND) drawEyePair(27, 72, 27, 28, 25, pupilX, pupilY, true, false);
  else if (eyeModelIndex == EYE_SQUARE) drawEyePair(25, 73, 27, 30, 25, pupilX, pupilY, false, false);
  else if (eyeModelIndex == EYE_PIXEL) {
    for (int side = 0; side < 2; side++) { int bx = side == 0 ? 27 : 75; for (int yy = 0; yy < 5; yy++) for (int xx = 0; xx < 5; xx++) if (!(yy == 0 && (xx == 0 || xx == 4))) display.fillRect(bx + xx * 4, 18 + yy * 5, 4, 5, SSD1306_WHITE); display.fillRect(bx + 8 + pupilX, 25 + pupilY, 4, 5, SSD1306_BLACK); }
  } else if (eyeModelIndex == EYE_CYBER) {
    int top = 17, bottom = 43;
    display.drawLine(20, bottom, 30, top, SSD1306_WHITE); display.drawLine(30, top, 55, top + 6, SSD1306_WHITE); display.drawLine(20, bottom, 55, bottom, SSD1306_WHITE);
    display.drawLine(73, top + 6, 98, top, SSD1306_WHITE); display.drawLine(98, top, 108, bottom, SSD1306_WHITE); display.drawLine(73, bottom, 108, bottom, SSD1306_WHITE);
    display.fillRect(36 + pupilX, 27 + pupilY, 9, 5, SSD1306_WHITE); display.fillRect(83 + pupilX, 27 + pupilY, 9, 5, SSD1306_WHITE);
  } else if (eyeModelIndex == EYE_CUTE) {
    drawEyePair(28, 72, 27, 27, 27, pupilX, pupilY, true, false);
    display.fillCircle(38 + pupilX, 28 + pupilY, 4, SSD1306_BLACK); display.fillCircle(82 + pupilX, 28 + pupilY, 4, SSD1306_BLACK);
    display.drawPixel(40 + pupilX, 26 + pupilY, SSD1306_WHITE); display.drawPixel(84 + pupilX, 26 + pupilY, SSD1306_WHITE);
  } else if (eyeModelIndex == EYE_MINIMAL) {
    display.fillRoundRect(22, 25, 36, 6, 3, SSD1306_WHITE); display.fillRoundRect(70, 25, 36, 6, 3, SSD1306_WHITE);
    display.fillRect(36 + pupilX, 25 + pupilY, 8, 6, SSD1306_BLACK); display.fillRect(84 + pupilX, 25 + pupilY, 8, 6, SSD1306_BLACK);
  }
  if (eyeExpressionIndex == EXPR_HAPPY) { display.drawLine(25, 45, 42, 40, SSD1306_WHITE); display.drawLine(86, 40, 103, 45, SSD1306_WHITE); }
  else if (eyeExpressionIndex == EXPR_ANGRY) { display.drawLine(20, 17, 54, 25, SSD1306_WHITE); display.drawLine(74, 25, 108, 17, SSD1306_WHITE); }
  else if (eyeExpressionIndex == EXPR_TIRED) { display.drawLine(22, 32, 55, 36, SSD1306_WHITE); display.drawLine(73, 36, 106, 32, SSD1306_WHITE); }
}

void drawEyePair(int leftX, int rightX, int topY, int w, int h, int pupilX, int pupilY, bool round, bool outline) {
  int radius = round ? h / 2 : 2;
  if (outline) { display.drawRoundRect(leftX, topY, w, h, radius, SSD1306_WHITE); display.drawRoundRect(rightX, topY, w, h, radius, SSD1306_WHITE); }
  else { display.fillRoundRect(leftX, topY, w, h, radius, SSD1306_WHITE); display.fillRoundRect(rightX, topY, w, h, radius, SSD1306_WHITE); }
  int p = max(2, h / 4);
  int lp = constrain(leftX + w / 2 - p / 2 + pupilX, leftX + 2, leftX + w - p - 2);
  int rp = constrain(rightX + w / 2 - p / 2 + pupilX, rightX + 2, rightX + w - p - 2);
  int py = constrain(topY + h / 2 - p / 2 + pupilY, topY + 2, topY + h - p - 2);
  display.fillCircle(lp + p / 2, py + p / 2, max(1, p / 2), SSD1306_BLACK);
  display.fillCircle(rp + p / 2, py + p / 2, max(1, p / 2), SSD1306_BLACK);
}

void drawBehavior() { drawList("COMPORTAMENTO", behaviorLabels, behaviorItems, behaviorIndex); }
void drawDisplaySettings() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("DISPLAY"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  int bright = displayBrightness * 100 / 255; const char* clockText = clockEnabled ? "ON" : "OFF"; const char* timeoutText = displayTimeout == 0 ? "OFF" : "30s";
  for (int i = 0; i < displayItems; i++) { int y = 14 + i * 12; display.setCursor(2, y); display.print(i == displayIndex ? ">" : " "); display.setCursor(12, y); display.print(displayLabels[i]); if (i == 0) { display.setCursor(92, y); display.printf("%d%%", bright); } if (i == 1) { display.setCursor(92, y); display.print(timeoutText); } if (i == 2) { display.setCursor(92, y); display.print(clockText); } if (i == 3) { display.setCursor(92, y); display.print("ON"); } }
  display.display();
}
void drawControls() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("CONTROLLI"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setCursor(2, 14); display.print(controlIndex == 0 ? "> " : "  "); display.println("MAPPATURA"); display.setCursor(8, 26); display.println("B SU   R GIU"); display.setCursor(8, 38); display.println("BL IND   N OK"); display.setCursor(2, 50); display.print(controlIndex == 1 ? "> " : "  "); display.println("MENU HOLD 2.5s"); display.display();
}
void drawSounds() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("SUONI"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) { int y = 14 + i * 12; display.setCursor(2, y); display.print(i == soundIndex ? ">" : " "); display.setCursor(12, y); display.print(soundLabels[i]); if (i == 0) { display.setCursor(94, y); display.print("--"); } if (i == 1) { display.setCursor(94, y); display.print(uiSounds ? "ON" : "OFF"); } if (i == 2) { display.setCursor(94, y); display.print("ON"); } }
  display.display();
}
void drawReset() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("RESET"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(4, 24); display.println("Ripristina default?"); display.setCursor(4, 40); display.println("N = SI"); display.setCursor(4, 52); display.println("BL = NO"); display.display();
}
void drawSystemInfo() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("INFO ROBOT"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(2, 14); display.println("DESKTOPROBOT"); display.setCursor(2, 26); display.println("OS v0.3"); display.setCursor(2, 38); display.println("FW v0.3.0"); display.setCursor(2, 50); display.printf("UP %lus", millis() / 1000UL); display.display();
}
void drawResources() {
  uint32_t freeHeap = ESP.getFreeHeap(), totalHeap = ESP.getHeapSize(), minHeap = ESP.getMinFreeHeap(), sketch = ESP.getSketchSize(), freeSketch = ESP.getFreeSketchSpace();
  int ramPct = totalHeap ? (int)((totalHeap - freeHeap) * 100UL / totalHeap) : 0; int flashPct = (sketch + freeSketch) ? (int)(sketch * 100UL / (sketch + freeSketch)) : 0;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("RISORSE"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(2, 13); display.printf("CPU %uMHz", ESP.getCpuFreqMHz()); display.setCursor(2, 25); display.printf("RAM %3d%%", ramPct); drawBar(67, 25, 58, 6, ramPct); display.setCursor(2, 37); display.printf("FREE %luK", (unsigned long)(freeHeap / 1024)); display.setCursor(2, 49); display.printf("MIN %luK", (unsigned long)(minHeap / 1024)); display.setCursor(70, 37); display.printf("FLASH %d%%", flashPct); display.display();
}
void drawBar(int x, int y, int w, int h, int percent) { percent = constrain(percent, 0, 100); display.drawRect(x, y, w, h, SSD1306_WHITE); int fill = (w - 2) * percent / 100; if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE); }
void drawHardware() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("HARDWARE"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(2, 14); display.println("ESP32 OK"); display.setCursor(2, 25); display.println("OLED 0x3C OK"); display.setCursor(2, 36); display.println("MPU 0x68 OK"); display.setCursor(2, 47); display.println("BTN 19/23/18/5"); display.display();
}
void drawDiagnostics() {
  Wire.beginTransmission(OLED_ADDR); bool oledBus = Wire.endTransmission() == 0; Wire.beginTransmission(0x68); bool mpuBus = Wire.endTransmission() == 0;
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("DIAGNOSTICA"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(2, 14); display.printf("I2C %s", oledBus && mpuBus ? "OK" : "ERR"); display.setCursor(2, 26); display.printf("OLED %s", oledBus ? "OK" : "ERR"); display.setCursor(2, 38); display.printf("MPU6050 %s", mpuBus ? "OK" : "ERR"); display.setCursor(2, 50); display.println("N = RITESTA"); display.display();
}
void drawReboot() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0); display.println("RIAVVIA ROBOT"); display.drawLine(0, 9, 127, 9, SSD1306_WHITE); display.setCursor(5, 25); display.println("N = RIAVVIA"); display.setCursor(5, 39); display.println("BL = ANNULLA"); display.display();
}
void loadSettings() {
  prefs.begin("desktop", false); eyeModelIndex = prefs.getInt("model", 0); eyeExpressionIndex = prefs.getInt("expr", 0); clockEnabled = prefs.getBool("clock", true); displayBrightness = prefs.getInt("bright", 255); displayTimeout = prefs.getInt("timeout", 0); uiSounds = prefs.getBool("uisound", true);
  eyeModelIndex = constrain(eyeModelIndex, 0, EYE_MODEL_COUNT - 1); eyeExpressionIndex = constrain(eyeExpressionIndex, 0, EYE_EXPRESSION_COUNT - 1); displayBrightness = constrain(displayBrightness, 32, 255); display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(displayBrightness);
}
void saveSettings() { prefs.putInt("model", eyeModelIndex); prefs.putInt("expr", eyeExpressionIndex); prefs.putBool("clock", clockEnabled); prefs.putInt("bright", displayBrightness); prefs.putInt("timeout", displayTimeout); prefs.putBool("uisound", uiSounds); }
void resetSettings() { eyeModelIndex = EYE_CLASSIC; eyeExpressionIndex = EXPR_DEFAULT; clockEnabled = true; displayBrightness = 255; displayTimeout = 0; uiSounds = true; saveSettings(); display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(255); applyEyeModel(); applyEyeExpression(); }
