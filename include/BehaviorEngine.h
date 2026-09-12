#pragma once

#include <Arduino.h>

enum class RobotState {
    NORMAL,
    HAPPY,
    TIRED,
    ANGRY,
    CURIOUS,
    BORED,
    SLEEPING
};

class BehaviorEngine {
public:
    BehaviorEngine(unsigned long curiousAfterMs = 15000,
                   unsigned long boredAfterMs = 30000,
                   unsigned long sleepingAfterMs = 60000);

    void begin();
    void update();
    void interaction();

    void setState(RobotState newState, unsigned long durationMs = 0);
    RobotState getState() const;
    bool stateChanged();
    bool hasTimedState() const;

private:
    RobotState state = RobotState::NORMAL;
    RobotState previousState = RobotState::NORMAL;

    unsigned long lastInteraction = 0;
    unsigned long stateUntil = 0;

    unsigned long curiousAfter;
    unsigned long boredAfter;
    unsigned long sleepingAfter;

    bool changed = true;
};
