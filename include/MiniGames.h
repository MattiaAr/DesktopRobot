#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

enum class MiniGameState : uint8_t { IDLE, PLAYING, GAME_OVER, EXITED };

class MiniGame {
public:
    virtual ~MiniGame() {}
    virtual void begin(uint32_t now) = 0;
    virtual void update(uint32_t now) = 0;
    virtual void input(bool up, bool down, bool select, bool back) = 0;
    virtual void render(Adafruit_SSD1306& display) const = 0;
    virtual MiniGameState state() const = 0;
    virtual uint32_t score() const = 0;
};

class SnakeGame : public MiniGame {
public:
    void begin(uint32_t now) override;
    void update(uint32_t now) override;
    void input(bool up, bool down, bool select, bool back) override;
    void render(Adafruit_SSD1306& display) const override;
    MiniGameState state() const override { return gameState; }
    uint32_t score() const override { return points; }
private:
    static const uint8_t COLS = 32, ROWS = 13, MAX_LEN = COLS * ROWS;
    uint8_t xs[MAX_LEN], ys[MAX_LEN], length = 3;
    int8_t dx = 1, dy = 0;
    uint8_t foodX = 12, foodY = 6;
    uint32_t points = 0, lastStep = 0;
    MiniGameState gameState = MiniGameState::IDLE;
    void placeFood();
};

class DinoGame : public MiniGame {
public:
    void begin(uint32_t now) override;
    void update(uint32_t now) override;
    void input(bool up, bool down, bool select, bool back) override;
    void render(Adafruit_SSD1306& display) const override;
    MiniGameState state() const override { return gameState; }
    uint32_t score() const override { return points; }
private:
    int16_t dinoY = 0, velocity = 0, obstacleX = 128;
    uint32_t points = 0, lastTick = 0;
    MiniGameState gameState = MiniGameState::IDLE;
};

class PongGame : public MiniGame {
public:
    void begin(uint32_t now) override;
    void update(uint32_t now) override;
    void input(bool up, bool down, bool select, bool back) override;
    void render(Adafruit_SSD1306& display) const override;
    MiniGameState state() const override { return gameState; }
    uint32_t score() const override { return playerScore; }
private:
    float ballX = 64, ballY = 32, vx = 2, vy = 1;
    int8_t playerY = 25, cpuY = 25;
    uint8_t playerScore = 0, cpuScore = 0;
    uint32_t lastTick = 0;
    MiniGameState gameState = MiniGameState::IDLE;
};
