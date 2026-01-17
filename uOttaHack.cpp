#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/select.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <random>

// IO functions
int readPin(int);
void writePin(int, bool);
bool kbhit();

// Utility
void sleep_millis(int);

// Game functions
std::vector<int> genSequence(int);

// randomizer
const unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); 

// Seed the Mersenne Twister engine
static std::mt19937 gen(seed); 

const int min = 0;
const int max = 3;
static std::uniform_int_distribution<int> dist(min, max); 

int main() {
    int leds[4] = {18, 23, 24, 21};
    int buttons[4] = {25, 12, 16, 20};

    // Configure pins
    for (int led : leds)
        system(("gpio-rp1 set " + std::to_string(led) + " op").c_str());
    for (int button : buttons)
        system(("gpio-rp1 set " + std::to_string(button) + " ip").c_str());

    std::cout << "Press 'exit' + Enter or Ctrl+C to quit.\n";

    // Set stdin non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    std::string input;

    while (true) {
        auto rng = genSequence(10);
        for (int i = 0; i < 10; i++) {
            writePin(leds[rng[i]], true);
            sleep_millis(500);
            writePin(leds[rng[i]], false);
            sleep_millis(100);
        }

        // Read buttons and set LEDs
        for (int i = 0; i < 4; ++i) {
            int state = readPin(buttons[i]);
            if (state == 1)
                writePin(leds[i], true);
            else
                writePin(leds[i], false);
        }

        // Check for console input to exit
        if (kbhit()) {
            std::getline(std::cin, input);
            if (input == "exit")
                break;
        }

        sleep_millis(50);
    }

    // Turn off LEDs
    for (int led : leds)
        writePin(led, false);
    std::cout << "Program exited.\n";
    return 0;
}

std::vector<int> genSequence(int count) {
    std::vector<int> seq;
    seq.reserve(count);
    for (int i = 0; i < count; i++) {
        seq[i] = dist(gen);
    }
    return seq;
}

void sleep_millis(int count) {
    std::this_thread::sleep_for(std::chrono::milliseconds(count));
}

// Read pin state via gpio-rp1
int readPin(int pin) {
    std::string cmd = "gpio-rp1 get " + std::to_string(pin);
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return -1;

    char buffer[16];
    int value = -1;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        value = std::atoi(std::string(buffer).substr(14, 2).c_str());
    }

    pclose(pipe);
    return value;
}

// Set pin high/low
void writePin(int pin, bool high) {
    std::string cmd = "gpio-rp1 set " + std::to_string(pin) + (high ? " dh" : " dl");
    system(cmd.c_str());
}

// Check if there is input on the stdin
bool kbhit() {
    fd_set set;
    struct timeval tv = {0, 0};
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    return select(STDIN_FILENO + 1, &set, nullptr, nullptr, &tv) > 0;
}