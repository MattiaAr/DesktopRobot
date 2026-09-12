#pragma once

#include <Arduino.h>

// RoboEyes defines several mood names as preprocessor symbols (HAPPY, TIRED,
// ANGRY, etc.). Prefixing our enum values avoids preprocessor collisions.
enum class RobotState {
    STATE_NORMAL,
    STATE_HAPPY,
    STATE_TIRED,
    STATE_ANGRY,
    STATE_CURIOUS,
    STATE_BORED,
    STATE_SLEEPING
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
    RobotState state = RobotState::STATE_NORMAL;
    RobotState previousState = RobotState::STATE_NORMAL;

    unsigned long lastInteraction = 0;
    unsigned long stateUntil = 0;

    unsigned long curiousAfter;
    unsigned long boredAfter;
    unsigned long sleepingAfter;

    bool changed = true;
};
