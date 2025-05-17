#include "mbed.h"
using namespace std::chrono; // Needed for 1s, 2ms, etc.

// Shift register connections (common anode display)
DigitalOut latch(D4);
DigitalOut clkPin(D7); // Renamed from 'clock' to avoid conflict
DigitalOut data(D8);

// Button inputs (Btn1 = reset, Btn2 = show voltage)
DigitalIn btnReset(A1), btnVoltage(A3);

// Analog input from potentiometer
AnalogIn analogInput(A0);

// Seven-segment encoding for digits 0-9 (with active-low logic)
const uint8_t segmentCodes[10] = {
    static_cast<uint8_t>(~0x3F), // 0
    static_cast<uint8_t>(~0x06), // 1
    static_cast<uint8_t>(~0x5B), // 2
    static_cast<uint8_t>(~0x4F), // 3
    static_cast<uint8_t>(~0x66), // 4
    static_cast<uint8_t>(~0x6D), // 5
    static_cast<uint8_t>(~0x7D), // 6
    static_cast<uint8_t>(~0x07), // 7
    static_cast<uint8_t>(~0x7F), // 8
    static_cast<uint8_t>(~0x6F)  // 9
};

// Digit enable control (left to right)
const uint8_t digitSelect[4] = {0x01, 0x02, 0x04, 0x08};

// Time and voltage tracking
volatile int elapsedSeconds = 0;
volatile int elapsedMinutes = 0;
volatile float lowestVoltage = 3.3f;
volatile float highestVoltage = 0.0f;

Ticker timeTicker;

// Function declarations
void incrementTime();
void sendToRegister(uint8_t val);
void outputToDisplay(uint8_t segData, uint8_t digitEnable);
void renderNumber(int num, bool useDecimal = false, int decimalIndex = -1);

// Timer interrupt: updates time each second
void incrementTime() {
    elapsedSeconds++;
    if (elapsedSeconds >= 60) {
        elapsedSeconds = 0;
        elapsedMinutes = (elapsedMinutes + 1) % 100;
    }
}

// Sends a byte via shift register (MSB first)
void sendToRegister(uint8_t val) {
    for (int bit = 7; bit >= 0; bit--) {
        data = (val & (1 << bit)) ? 1 : 0;
        clkPin = 1;
        clkPin = 0;
    }
}

// Loads data into the shift register and enables a digit
void outputToDisplay(uint8_t segData, uint8_t digitEnable) {
    latch = 0;
    sendToRegister(segData);
    sendToRegister(digitEnable);
    latch = 1;
}

// Displays a 4-digit number; optional decimal at a chosen position
void renderNumber(int num, bool useDecimal, int decimalIndex) {
    int digits[4] = {
        (num / 1000) % 10,
        (num / 100) % 10,
        (num / 10) % 10,
        num % 10
    };

    for (int i = 0; i < 4; i++) {
        uint8_t pattern = segmentCodes[digits[i]];
        if (useDecimal && i == decimalIndex) {
            pattern &= ~0x80; // Activate decimal point (assuming MSB is DP)
        }
        outputToDisplay(pattern, digitSelect[i]);
        ThisThread::sleep_for(2ms);
    }
}

int main() {
    // Configure buttons with pull-ups
    btnReset.mode(PullUp);
    btnVoltage.mode(PullUp);

    // Start the timer interrupt every second
    timeTicker.attach(&incrementTime, 1s);

    while (true) {
        // Reset time if reset button is pressed
        if (!btnReset) {
            elapsedSeconds = 0;
            elapsedMinutes = 0;
            ThisThread::sleep_for(200ms); // Debounce delay
        }

        // Read voltage level from potentiometer
        float currentVoltage = analogInput.read() * 3.3f;

        // Track min/max voltage readings
        if (currentVoltage < lowestVoltage) lowestVoltage = currentVoltage;
        if (currentVoltage > highestVoltage) highestVoltage = currentVoltage;

        // Show voltage if button is pressed, else show timer
        if (!btnVoltage) {
            int voltageAsInt = static_cast<int>(currentVoltage * 100); // e.g., 2.47V → 247
            renderNumber(voltageAsInt, true, 1); // Format: X.XX
        } else {
            int displayTime = elapsedMinutes * 100 + elapsedSeconds; // Format: MMSS
            renderNumber(displayTime);
        }
    }
}
