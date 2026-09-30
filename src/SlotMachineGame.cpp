#include "SlotMachineGame.h"

// Explicit configuration: each of the five symbols has 20/100 weight on each reel.
// This transparent baseline is not a calibrated return-to-player model.
const SlotMachineGame::SymbolWeight SlotMachineGame::symbolWeights[5] = {
    {SlotSymbol::LEMON, 20},
    {SlotSymbol::CHERRY, 20},
    {SlotSymbol::BELL, 20},
    {SlotSymbol::BAR, 20},
    {SlotSymbol::SEVEN, 20}
};

const uint16_t SlotMachineGame::bets[6] = {5, 10, 20, 40, 80, 160};

// Entries follow [lemon, cherry, bell, seven], indexed by bet.
const uint16_t SlotMachineGame::payouts[6][4] = {
    {10, 25, 40, 100},
    {20, 35, 55, 150},
    {40, 55, 70, 250},
    {80, 110, 140, 500},
    {160, 220, 280, 1000},
    {320, 440, 560, 2000}
};

void SlotMachineGame::begin() {
    storage.begin("slot", false);
    balance = storage.getUInt("coins", 100);
    bestBalance = storage.getUInt("best", balance);
    spinCount = storage.getUInt("spins", 0);
    winCount = storage.getUInt("wins", 0);
    selectedBet = 5;
    selectedBetIndex = 0;
    lastPayout = 0;
    currentState = SlotState::READY;
    reloadedThisSession = false;
    settleInterruptedSpin();
    randomSeed(esp_random());
    if (balance < 5) currentState = SlotState::CREDITS_EMPTY;
}

void SlotMachineGame::update(uint32_t now) {
    if (currentState == SlotState::SPINNING) {
        const uint32_t elapsed = now - spinStartedAt;
        while (now - lastReelTick >= 65 && settledReels < 3) {
            lastReelTick += 65;
            const uint32_t stopAt = 550UL + static_cast<uint32_t>(settledReels) * 450UL;
            if (elapsed >= stopAt) {
                reels[settledReels] = result[settledReels];
                ++settledReels;
            } else {
                reels[settledReels] = drawSymbol();
            }
        }
        if (settledReels == 3) settle(now);
        return;
    }

    if (currentState == SlotState::RESULT && now - resultShownAt >= 2200) {
        currentState = balance < 5 ? SlotState::CREDITS_EMPTY : SlotState::READY;
    }
}

bool SlotMachineGame::selectPreviousBet() {
    if (currentState != SlotState::READY || selectedBetIndex == 0) return false;
    --selectedBetIndex;
    selectedBet = bets[selectedBetIndex];
    return true;
}

bool SlotMachineGame::selectNextBet() {
    if (currentState != SlotState::READY || selectedBetIndex + 1 >= availableBetCount()) return false;
    ++selectedBetIndex;
    selectedBet = bets[selectedBetIndex];
    return true;
}

bool SlotMachineGame::startSpin(uint32_t now) {
    if (currentState != SlotState::READY || selectedBet == 0 || selectedBet > balance) return false;

    // Persist the recovery snapshot before marking the spin pending or
    // debiting the wager. A reboot can then safely restore the charged loss.
    storage.putUInt("pendbase", balance);
    storage.putUInt("pendbest", bestBalance);
    storage.putUInt("pendspins", spinCount);
    storage.putUInt("pendwins", winCount);
    storage.putUShort("pendbet", selectedBet);
    storage.putBool("pending", true);
    settledBet = selectedBet;
    balance -= selectedBet;
    for (uint8_t i = 0; i < 3; ++i) result[i] = drawSymbol();
    storage.putUInt("coins", balance);

    lastPayout = 0;
    won = false;
    settledReels = 0;
    spinStartedAt = now;
    lastReelTick = now;
    currentState = SlotState::SPINNING;
    return true;
}

bool SlotMachineGame::confirmReload() {
    if (currentState != SlotState::RELOAD_CONFIRM || reloadedThisSession) return false;
    balance = 100;
    reloadedThisSession = true;
    save();
    currentState = SlotState::READY;
    selectedBetIndex = 0;
    selectedBet = bets[0];
    return true;
}

bool SlotMachineGame::confirmExit() {
    if (currentState != SlotState::EXIT_CONFIRM) return false;
    currentState = SlotState::READY;
    return true;
}

bool SlotMachineGame::requestReload() {
    if (currentState != SlotState::CREDITS_EMPTY || reloadedThisSession) return false;
    currentState = SlotState::RELOAD_CONFIRM;
    return true;
}

void SlotMachineGame::cancelDialog() {
    if (currentState == SlotState::RELOAD_CONFIRM) currentState = SlotState::CREDITS_EMPTY;
    else if (currentState == SlotState::EXIT_CONFIRM) currentState = SlotState::READY;
}

void SlotMachineGame::requestExit() {
    if (currentState == SlotState::READY || currentState == SlotState::RESULT || currentState == SlotState::CREDITS_EMPTY) {
        currentState = SlotState::EXIT_CONFIRM;
    }
}

void SlotMachineGame::resetSlotData() {
    balance = 100;
    bestBalance = 100;
    spinCount = 0;
    winCount = 0;
    lastPayout = 0;
    reloadedThisSession = false;
    storage.clear();
    save();
    currentState = SlotState::READY;
}

SlotSymbol SlotMachineGame::reel(uint8_t index) const {
    return index < 3 ? reels[index] : SlotSymbol::LEMON;
}

uint16_t SlotMachineGame::availableBet(uint8_t index) const {
    return index < availableBetCount() ? bets[index] : 0;
}

uint8_t SlotMachineGame::availableBetCount() const {
    uint8_t count = balance >= 5000 ? 6 : balance >= 2000 ? 5 : balance >= 1000 ? 4 : balance >= 500 ? 3 : balance >= 100 ? 2 : 1;
    while (count > 0 && bets[count - 1] > balance) --count;
    return count;
}

SlotSymbol SlotMachineGame::drawSymbol() {
    uint16_t total = 0;
    for (const SymbolWeight &entry : symbolWeights) total += entry.weight;
    if (total == 0) return SlotSymbol::LEMON;
    uint16_t roll = static_cast<uint16_t>(random(total));
    for (const SymbolWeight &entry : symbolWeights) {
        if (roll < entry.weight) return entry.symbol;
        roll -= entry.weight;
    }
    return SlotSymbol::LEMON;
}

uint16_t SlotMachineGame::evaluatePayout() const {
    if (result[0] != result[1] || result[1] != result[2]) return 0;

    uint8_t category;
    switch (result[0]) {
        case SlotSymbol::LEMON: category = 0; break;
        case SlotSymbol::CHERRY: category = 1; break;
        case SlotSymbol::BELL: category = 2; break;
        case SlotSymbol::SEVEN: category = 3; break;
        case SlotSymbol::BAR: return 0;
        default: return 0;
    }
    return payouts[selectedBetIndex][category];
}

void SlotMachineGame::settle(uint32_t now) {
    lastPayout = evaluatePayout();
    won = lastPayout > 0;
    balance += lastPayout;
    if (balance > bestBalance) bestBalance = balance;
    ++spinCount;
    if (won) ++winCount;
    if (balance >= 5 && selectedBet > balance) {
        selectedBetIndex = availableBetCount() - 1;
        selectedBet = bets[selectedBetIndex];
    }
    save();
    storage.putBool("pending", false);
    resultShownAt = now;
    currentState = SlotState::RESULT;
}

void SlotMachineGame::save() {
    storage.putUInt("coins", balance);
    storage.putUInt("best", bestBalance);
    storage.putUInt("spins", spinCount);
    storage.putUInt("wins", winCount);
}

void SlotMachineGame::settleInterruptedSpin() {
    if (!storage.getBool("pending", false)) return;
    // Drop the interrupted spin and preserve exactly its wager debit. Restore
    // counters and best from the pre-spin snapshot to prevent partial awards.
    const uint32_t baseBalance = storage.getUInt("pendbase", balance + storage.getUShort("pendbet", 0));
    const uint16_t wager = storage.getUShort("pendbet", 0);
    balance = baseBalance >= wager ? baseBalance - wager : 0;
    bestBalance = storage.getUInt("pendbest", bestBalance);
    spinCount = storage.getUInt("pendspins", spinCount);
    winCount = storage.getUInt("pendwins", winCount);
    save();
    storage.putBool("pending", false);
    storage.remove("pendbet");
    storage.remove("pendbase");
    storage.remove("pendbest");
    storage.remove("pendspins");
    storage.remove("pendwins");
}
