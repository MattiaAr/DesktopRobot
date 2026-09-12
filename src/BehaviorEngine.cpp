#include "BehaviorEngine.h"

BehaviorEngine::BehaviorEngine(unsigned long curiousAfterMs,
                               unsigned long boredAfterMs,
                               unsigned long sleepingAfterMs)
    : curiousAfter(curiousAfterMs),
      boredAfter(boredAfterMs),
      sleepingAfter(sleepingAfterMs) {}

void BehaviorEngine::begin() {
    state = RobotState::STATE_NORMAL;
    previousState = RobotState::STATE_NORMAL;
    lastInteraction = millis();
    stateUntil = 0;
    changed = true;
}

void BehaviorEngine::update() {
    const unsigned long now = millis();

    // Timed reactions have priority over idle transitions.
    if (stateUntil != 0) {
        if ((long)(now - stateUntil) < 0) {
            return;
        }

        stateUntil = 0;
        state = RobotState::STATE_NORMAL;
        changed = true;
        return;
    }

    const unsigned long idleTime = now - lastInteraction;

    if (state == RobotState::STATE_SLEEPING) {
        return;
    }

    if (idleTime >= sleepingAfter) {
        setState(RobotState::STATE_SLEEPING);
    }
    else if (idleTime >= boredAfter) {
        setState(RobotState::STATE_BORED);
    }
    else if (idleTime >= curiousAfter) {
        setState(RobotState::STATE_CURIOUS);
    }
    else if (state == RobotState::STATE_CURIOUS || state == RobotState::STATE_BORED) {
        setState(RobotState::STATE_NORMAL);
    }
}

void BehaviorEngine::interaction() {
    lastInteraction = millis();

    if (state == RobotState::STATE_SLEEPING ||
        state == RobotState::STATE_CURIOUS ||
        state == RobotState::STATE_BORED) {
        setState(RobotState::STATE_NORMAL);
    }
}

void BehaviorEngine::setState(RobotState newState, unsigned long durationMs) {
    if (state != newState) {
        previousState = state;
        state = newState;
        changed = true;
    }

    if (durationMs > 0) {
        stateUntil = millis() + durationMs;
    }
}

RobotState BehaviorEngine::getState() const {
    return state;
}

bool BehaviorEngine::stateChanged() {
    const bool result = changed;
    changed = false;
    return result;
}

bool BehaviorEngine::hasTimedState() const {
    return stateUntil != 0;
}
