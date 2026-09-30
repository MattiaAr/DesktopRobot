#pragma once

#include <Arduino.h>
#include <Preferences.h>

enum class SlotSymbol : uint8_t {
    LEMON,
    CHERRY,
    BELL,
    BAR,
    SEVEN
};

enum class SlotState : uint8_t {
    READY,
    SPINNING,
    RESULT,
    CREDITS_EMPTY,
    RELOAD_CONFIRM,
    EXIT_CONFIRM
};

// Virtual-coin slot machine. A pending spin is discarded on reboot after its
// wager has been debited; this guarantees that a payout cannot be replayed.
class SlotMachineGame {
public:
    void begin();
    void update(uint32_t now);
    bool selectPreviousBet();
    bool selectNextBet();
    bool startSpin(uint32_t now);
    bool confirmReload();
    bool confirmExit();
    bool requestReload();
    void cancelDialog();
    void requestExit();
    void resetSlotData();

    SlotState state() const { return currentState; }
    SlotSymbol reel(uint8_t index) const;
    uint32_t coins() const { return balance; }
    uint32_t best() const { return bestBalance; }
    uint32_t spins() const { return spinCount; }
    uint32_t wins() const { return winCount; }
    uint16_t bet() const { return selectedBet; }
    uint16_t lastBet() const { return settledBet; }
    uint16_t payout() const { return lastPayout; }
    uint16_t availableBet(uint8_t index) const;
    uint8_t availableBetCount() const;
    bool lastSpinWon() const { return won; }
    bool reloadAvailable() const { return !reloadedThisSession; }

private:
    struct SymbolWeight {
        SlotSymbol symbol;
        uint8_t weight;
    };

    static const SymbolWeight symbolWeights[5];
    static const uint16_t bets[6];
    static const uint16_t payouts[6][4];

    Preferences storage;
    SlotState currentState = SlotState::READY;
    SlotSymbol reels[3] = {SlotSymbol::LEMON, SlotSymbol::CHERRY, SlotSymbol::BELL};
    SlotSymbol result[3] = {SlotSymbol::LEMON, SlotSymbol::CHERRY, SlotSymbol::BELL};
    uint32_t balance = 100;
    uint32_t bestBalance = 100;
    uint32_t spinCount = 0;
    uint32_t winCount = 0;
    uint16_t selectedBet = 5;
    uint16_t settledBet = 5;
    uint16_t lastPayout = 0;
    uint8_t selectedBetIndex = 0;
    uint8_t settledReels = 0;
    uint32_t spinStartedAt = 0;
    uint32_t lastReelTick = 0;
    uint32_t resultShownAt = 0;
    bool won = false;
    bool reloadedThisSession = false;

    SlotSymbol drawSymbol();
    uint16_t evaluatePayout() const;
    void settle(uint32_t now);
    void save();
    void settleInterruptedSpin();
};
