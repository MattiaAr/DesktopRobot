#include "SlotMachineGame.h"

// Symbol weights are percentages and apply independently to each reel.
const SlotMachineGame::SymbolWeight SlotMachineGame::symbolWeights[5] = {
    {SlotSymbol::LEMON,  36},
    {SlotSymbol::CHERRY, 27},
    {SlotSymbol::BELL,   17},
    {SlotSymbol::BAR,    12},
    {SlotSymbol::SEVEN,   8}
};

const uint16_t SlotMachineGame::bets[6] = {5, 10, 20, 40, 80, 160};

// Gross payout multipliers, scaled by 2: [symbol][exact pair, triple].
// Examples: 3 means 1.5x; The wager is already debited,
// so the returned payout includes the original wager amount.
namespace {
uint8_t payoutMultiplier(SlotSymbol symbol, uint8_t outcomeIndex) {
    static const uint8_t payoutMultipliers[5][2] = {
        {2,  5},  // Lemon: 1x pair, 2.5x triple
        {2,  5},  // Cherry: 1x pair, 2.5x triple
        {5,  7},  // Bell: 2.5x pair, 3.5x triple
        {8, 10},  // BAR: 4x pair, 5x triple
        {10, 20}  // Seven: 5x pair, 10x triple
    };

    const uint8_t symbolIndex = static_cast<uint8_t>(symbol);
    return symbolIndex < 5 && outcomeIndex < 2 ? payoutMultipliers[symbolIndex][outcomeIndex] : 0U;
}
} // namespace

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

    if (currentState == SlotState::RESULT && now - resultShownAt >= 800) {
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

//uint8_t SlotMachineGame::availableBetCount() const {
//    uint8_t count = balance >= 5000 ? 6 : balance >= 2000 ? 5 : balance >= 1000 ? 4 : balance >= 500 ? 3 : balance >= 100 ? 2 : 1;
//    while (count > 0 && bets[count - 1] > balance) --count;
//    return count;
//}

uint8_t SlotMachineGame::availableBetCount() const {
    uint8_t count = 0;

    while (count < 6 && bets[count] <= balance) {
        ++count;
    }

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
    // Count matching symbols. A triple is evaluated before a pair, so it
    // receives only the triple award and is never paid twice.
    uint8_t matchingSymbol = 0;
    uint8_t matchCount = 0;

    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t count = 0;

        for (uint8_t j = 0; j < 3; ++j) {
            if (result[i] == result[j]) {
                ++count;
            }
        }

        if (count > matchCount) {
            matchCount = count;
            matchingSymbol = static_cast<uint8_t>(result[i]);
        }
    }

    // No pair or invalid symbol: no payout.
    if (matchCount < 2 ||
        matchingSymbol > static_cast<uint8_t>(SlotSymbol::SEVEN)) {
        return 0;
    }

    // 0 = pair, 1 = triple.
    const bool isTriple = (matchCount == 3);

    const uint8_t multiplierX2 = payoutMultiplier(
        static_cast<SlotSymbol>(matchingSymbol),
        isTriple ? 1 : 0
    );

    // Calculate the gross payout using integer arithmetic.
    // The result is then rounded up to the next multiple of 5.
    const uint32_t rawPayout =
        (static_cast<uint32_t>(settledBet) * multiplierX2) / 2U;

    // Round the payout up to the next multiple of 5.
    const uint32_t roundedPayout =
        ((rawPayout + 4U) / 5U) * 5U;

    return static_cast<uint16_t>(roundedPayout);
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