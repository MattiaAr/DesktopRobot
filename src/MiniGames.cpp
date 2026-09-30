#include "MiniGames.h"

void SnakeGame::begin(uint32_t now) {
    length = 3; xs[0] = 10; ys[0] = 6; xs[1] = 9; ys[1] = 6; xs[2] = 8; ys[2] = 6;
    dx = 1; dy = 0; points = 0; lastStep = now; gameState = MiniGameState::PLAYING; placeFood();
}
void SnakeGame::placeFood() {
    for (uint16_t tries = 0; tries < 128; ++tries) {
        foodX = random(COLS); foodY = random(ROWS);
        bool occupied = false;
        for (uint8_t i = 0; i < length; ++i) if (xs[i] == foodX && ys[i] == foodY) occupied = true;
        if (!occupied) return;
    }
}
void SnakeGame::input(bool up, bool down, bool select, bool back) {
    if (back) { gameState = MiniGameState::EXITED; return; }
    if (gameState == MiniGameState::GAME_OVER) { if (up || down || select) begin(millis()); return; }
    int8_t nx = dx, ny = dy;
    if (up) { nx = 0; ny = -1; } else if (down) { nx = 0; ny = 1; }
    if ((nx != -dx || ny != -dy) && (nx != dx || ny != dy)) { dx = nx; dy = ny; }
}
void SnakeGame::update(uint32_t now) {
    if (gameState != MiniGameState::PLAYING || now - lastStep < (uint32_t)max(65, 150 - (int)points * 3)) return;
    lastStep = now;
    int16_t nx = (int16_t)xs[0] + dx, ny = (int16_t)ys[0] + dy;
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { gameState = MiniGameState::GAME_OVER; return; }
    bool eat = nx == foodX && ny == foodY;
    uint8_t checkLen = eat ? length : length - 1;
    for (uint8_t i = 0; i < checkLen; ++i) if (xs[i] == nx && ys[i] == ny) { gameState = MiniGameState::GAME_OVER; return; }
    if (eat && length < MAX_LEN) ++length;
    for (int16_t i = length - 1; i > 0; --i) { xs[i] = xs[i - 1]; ys[i] = ys[i - 1]; }
    xs[0] = nx; ys[0] = ny;
    if (eat) {
        ++points;
        if (length >= MAX_LEN) gameState = MiniGameState::GAME_OVER;
        else placeFood();
    }
}
void SnakeGame::render(Adafruit_SSD1306& d) const {
    d.clearDisplay(); d.setTextSize(1); d.setTextColor(SSD1306_WHITE); d.setCursor(0, 0); d.printf("SNAKE %lu", (unsigned long)points);
    d.drawRect(0, 10, 128, 54, SSD1306_WHITE);
    d.fillRect(foodX * 4 + 1, foodY * 4 + 11, 3, 3, SSD1306_WHITE);
    for (uint8_t i = 0; i < length; ++i) d.fillRect(xs[i] * 4 + 1, ys[i] * 4 + 11, 3, 3, SSD1306_WHITE);
    if (gameState == MiniGameState::GAME_OVER) { d.fillRect(16, 26, 96, 18, SSD1306_BLACK); d.drawRect(16, 26, 96, 18, SSD1306_WHITE); d.setCursor(21, 32); d.print("N/R=RETRY BL=EXIT"); }
    d.display();
}

void DinoGame::begin(uint32_t now) { dinoY = 0; velocity = 0; obstacleX = 128; points = 0; lastTick = now; gameState = MiniGameState::PLAYING; }
void DinoGame::input(bool up, bool, bool select, bool back) {
    if (back) { gameState = MiniGameState::EXITED; return; }
    if (gameState == MiniGameState::GAME_OVER) { if (select || up) begin(millis()); return; }
    if ((up || select) && dinoY == 0) velocity = -7;
}
void DinoGame::update(uint32_t now) {
    if (gameState != MiniGameState::PLAYING || now - lastTick < 30) return;
    lastTick = now; ++points;
    dinoY += velocity; velocity += 1; if (dinoY > 0) { dinoY = 0; velocity = 0; }
    obstacleX -= (int16_t)(2 + points / 500);
    if (obstacleX < -5) obstacleX = 128 + random(0, 45);
    if (obstacleX < 22 && obstacleX > 8 && dinoY > -12) gameState = MiniGameState::GAME_OVER;
}
void DinoGame::render(Adafruit_SSD1306& d) const {
    d.clearDisplay(); d.setTextSize(1); d.setTextColor(SSD1306_WHITE); d.setCursor(0, 0); d.printf("DINO %lu", (unsigned long)points);
    d.drawLine(0, 54, 127, 54, SSD1306_WHITE); d.fillRect(13, 42 + dinoY, 9, 12, SSD1306_WHITE); d.fillRect(obstacleX, 42, 5, 12, SSD1306_WHITE);
    if (gameState == MiniGameState::GAME_OVER) { d.setCursor(11, 27); d.print("N/R=RETRY BL=EXIT"); }
    d.display();
}

void PongGame::begin(uint32_t now) { ballX = 64; ballY = 32; vx = 2; vy = 1; playerY = cpuY = 25; playerScore = cpuScore = 0; lastTick = now; gameState = MiniGameState::PLAYING; }
void PongGame::input(bool up, bool down, bool select, bool back) {
    if (back) { gameState = MiniGameState::EXITED; return; }
    if (gameState == MiniGameState::GAME_OVER) { if (select || up || down) begin(millis()); return; }
    if (up && playerY > 11) playerY -= 4; if (down && playerY < 49) playerY += 4;
}
void PongGame::update(uint32_t now) {
    if (gameState != MiniGameState::PLAYING || now - lastTick < 28) return;
    lastTick = now; ballX += vx; ballY += vy;
    if (ballY < 12 || ballY > 62) vy = -vy;
    if (vx < 0 && ballX < 11 && ballY >= playerY && ballY <= playerY + 14) { vx = -vx * 1.04f; vy += (ballY - (playerY + 7)) * 0.12f; }
    if (vx > 0 && ballX > 117 && ballY >= cpuY && ballY <= cpuY + 14) { vx = -vx * 1.03f; vy += (ballY - (cpuY + 7)) * 0.08f; }
    if (cpuY + 7 < ballY && cpuY < 49) ++cpuY; else if (cpuY + 7 > ballY && cpuY > 11) --cpuY;
    if (ballX < 0) { ++cpuScore; ballX = 64; ballY = 32; vx = 2; vy = 1; }
    if (ballX > 127) { ++playerScore; ballX = 64; ballY = 32; vx = -2; vy = 1; }
    if (playerScore >= 5 || cpuScore >= 5) gameState = MiniGameState::GAME_OVER;
}
void PongGame::render(Adafruit_SSD1306& d) const {
    d.clearDisplay(); d.setTextSize(1); d.setTextColor(SSD1306_WHITE); d.setCursor(43, 0); d.printf("%u : %u", playerScore, cpuScore);
    d.drawLine(0, 10, 127, 10, SSD1306_WHITE); d.fillRect(4, playerY, 3, 14, SSD1306_WHITE); d.fillRect(121, cpuY, 3, 14, SSD1306_WHITE); d.fillCircle((int16_t)ballX, (int16_t)ballY, 2, SSD1306_WHITE);
    if (gameState == MiniGameState::GAME_OVER) { d.setCursor(11, 29); d.print("N/R=RETRY BL=EXIT"); }
    d.display();
}
