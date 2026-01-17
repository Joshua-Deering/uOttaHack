#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <string>
#include <sys/select.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "./rpi_i2c/public/rpi_i2c.h" // I2C API functions for Raspberry Pi or QNX
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// I2C address for the PCF8574 I/O expander (controls the LCD)
#define I2C_ADDR 0x27
#define BUS 1 // I2C bus number

// LCD configuration
#define LCD_WIDTH 16      // Characters per line
#define LCD_CHR 1         // Sending data
#define LCD_CMD 0         // Sending command
#define LCD_LINE_1 0x80   // Address for line 1
#define LCD_LINE_2 0xC0   // Address for line 2
#define ENABLE 0b00000100 // Enable bit for toggling

// Backlight control (0x08 = on, 0x00 = off)
uint8_t LCD_BACKLIGHT = 0x00;

// Delay constants (in microseconds)
#define E_PULSE_US 500
#define E_DELAY_US 500

// IO functions
int readPin(int);
void writePin(int, bool);
bool kbhit();

// Utility
void sleep_millis(int);

// Game functions
std::vector<int> genSequence(int);
bool readSequence(const std::vector<int> &);

// lcd functions
void lcd_toggle_enable(uint8_t bits);
void lcd_byte(uint8_t bits, uint8_t mode);
void lcd_init();
void lcd_string(const char *message, uint8_t line);
void marquee_smooth(const char *message, uint8_t line, int delay_ms, int cycles);

// randomizer
const unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

// Seed the Mersenne Twister engine
static std::mt19937 gen(seed);

const int min = 0;
const int max = 3;
static std::uniform_int_distribution<int> dist(min, max);

// pin assignments
const int leds[4] = {18, 23, 24, 21};
const int buttons[4] = {25, 12, 16, 20};
const int success_led = 17;
const int fail_led = 27;

// game states
enum GameState {
    IDLE,
    RUNNING
};

int main() {
    GameState game_state = GameState::IDLE;
    GameState prevState = GameState::IDLE;

    // Initialize the LCD
    lcd_init();

    // Enable backlight
    LCD_BACKLIGHT = 0x08;

    // Display messages
    lcd_string("Press Any Button", LCD_LINE_1);
    lcd_string("    To Begin!", LCD_LINE_2);

    // Configure pins
    for (int led : leds)
        system(("gpio-rp1 set " + std::to_string(led) + " op").c_str());
    for (int button : buttons)
        system(("gpio-rp1 set " + std::to_string(button) + " ip").c_str());

    system(("gpio-rp1 set " + std::to_string(success_led) + " op").c_str());
    system(("gpio-rp1 set " + std::to_string(fail_led) + " op").c_str());

    std::cout << "Press Ctrl+C to quit.\n";

    // Set stdin non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    std::string input;

    int cur_level = 1;
    int lives = 3;
    while (true) {
        switch (game_state) {
        case GameState::IDLE:
            if (prevState == GameState::RUNNING) {
                lcd_string("Press Any Button", LCD_LINE_1);
                lcd_string("    To Begin!", LCD_LINE_2);
            }
            // Read buttons for input
            for (int i = 0; i < 4; ++i) {
                int state = readPin(buttons[i]);
                if (state == 1) {
                    game_state = GameState::RUNNING;
                    lcd_byte(0x01, LCD_CMD);  // Clear display
                    break;
                }
            }
            prevState = GameState::IDLE;
            break;
        case GameState::RUNNING:
            if (prevState == GameState::IDLE) {
                cur_level = 1;
                lives = 10;
                lcd_string(("Level: " + std::to_string(cur_level)).c_str(), LCD_LINE_1);
                lcd_string(("Lives: " + std::to_string(lives)).c_str(), LCD_LINE_2);
            }

            sleep_millis(500);
            // generate and show sequence
            auto rng = genSequence(cur_level / 3 + 3);
            for (int i = 0; i < rng.size(); i++) {
                writePin(leds[rng[i]], true);
                sleep_millis(500);
                writePin(leds[rng[i]], false);
                sleep_millis(100);
            }

            cur_level++;
            // get user input
            if (readSequence(rng)) {
                writePin(success_led, true);
                sleep_millis(2000);
                writePin(success_led, false);
                lcd_byte(0x01, LCD_CMD);  // Clear display
                lcd_string(("Level: " + std::to_string(cur_level)).c_str(), LCD_LINE_1);
                lcd_string(("Lives: " + std::to_string(lives)).c_str(), LCD_LINE_2);
            } else {
                writePin(fail_led, true);
                lives--;

                lcd_byte(0x01, LCD_CMD);  // Clear display
                lcd_string("Wrong!", LCD_LINE_1);
                lcd_string(("Lives Left: " + std::to_string(lives)).c_str(), LCD_LINE_2);

                sleep_millis(1500);
                writePin(fail_led, false);

                lcd_string(("Level: " + std::to_string(cur_level)).c_str(), LCD_LINE_1);
                lcd_string(("Lives: " + std::to_string(lives)).c_str(), LCD_LINE_2);

                sleep_millis(1000);

                if (lives == 0) {
                    lcd_byte(0x01, LCD_CMD);  // Clear display
                    lcd_string("You Lose!", LCD_LINE_1);
                    lcd_string(("Level Reached: " + std::to_string(cur_level)).c_str(), LCD_LINE_2);
                    sleep_millis(1500);
                    cur_level = 1;
                    lcd_byte(0x01, LCD_CMD);  // Clear display
                    game_state = GameState::IDLE;
                }
            }

            // Read buttons and set LEDs
            for (int i = 0; i < 4; ++i) {
                int state = readPin(buttons[i]);
                if (state == 1)
                    writePin(leds[i], true);
                else
                    writePin(leds[i], false);
            }
            prevState = GameState::RUNNING;
            break;
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
    // Clean up I2C
    smbus_cleanup(BUS);
    std::cout << "Program exited.\n";
    return 0;
}

std::vector<int> genSequence(int count) {
    std::vector<int> seq;
    seq.reserve(count);
    for (int i = 0; i < count; i++) {
        seq.push_back(dist(gen));
    }
    return seq;
}

bool readSequence(const std::vector<int> &seq) {
    // auto start = std::chrono::system_clock::now();
    auto last_input = std::chrono::system_clock::now();

    int cur_idx = 0;

    bool prev_state[4] = {false, false, false, false};

    while (true) {
        // 5s timeout if no inputs
        if (std::chrono::system_clock::now() - last_input > std::chrono::seconds(5)) {
            return false;
        }
        if (cur_idx >= seq.size())
            return true;

        int active_btn = -1;
        // Read buttons and set LEDs
        for (int i = 0; i < 4; ++i) {
            bool state = readPin(buttons[i]);

            writePin(leds[i], state);

            // Rising edge detection
            if (state && !prev_state[i]) {
                active_btn = i;
                last_input = std::chrono::system_clock::now();
            }

            prev_state[i] = state;
        }

        if (active_btn == -1) {
            sleep_millis(10);
            continue;
        }

        if (active_btn == seq.at(cur_idx)) {
            cur_idx++;
        } else {
            return false;
        }

        // avoid busywaiting
        sleep_millis(10);
    }
    return true;
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

// LCD functions (from https://gitlab.com/qnx/projects/hardware-component-samples/-/blob/main/lcd-1602/c/src/lcd-1602.c)
// Initialize the LCD with standard commands
void lcd_init() {
    lcd_byte(0b00110011, LCD_CMD); // Initialization step 1
    lcd_byte(0b00110010, LCD_CMD); // Initialization step 2
    lcd_byte(0b00000110, LCD_CMD); // Set cursor move direction
    lcd_byte(0b00001100, LCD_CMD); // Display ON, cursor OFF, blink OFF
    lcd_byte(0b00101000, LCD_CMD); // Function set: 2 lines, 5x8 font
    lcd_byte(0b00000001, LCD_CMD); // Clear display
    usleep(E_DELAY_US);            // Short delay
}

// Send byte to LCD (as command or data), split into two 4-bit transfers
void lcd_byte(uint8_t bits, uint8_t mode) {
    uint8_t bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    uint8_t bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    smbus_write_byte_data(BUS, I2C_ADDR, 0, bits_high);
    lcd_toggle_enable(bits_high);

    smbus_write_byte_data(BUS, I2C_ADDR, 0, bits_low);
    lcd_toggle_enable(bits_low);
}

// Toggle the enable bit to latch data into the LCD
void lcd_toggle_enable(uint8_t bits) {
    usleep(E_DELAY_US);
    smbus_write_byte_data(BUS, I2C_ADDR, 0, bits | ENABLE);
    usleep(E_PULSE_US);
    smbus_write_byte_data(BUS, I2C_ADDR, 0, bits & ~ENABLE);
    usleep(E_DELAY_US);
}

// Display a string on a specified LCD line
void lcd_string(const char *message, uint8_t line) {
    char padded[LCD_WIDTH + 1];
    int len = strlen(message);

    // Pad or truncate message to LCD width
    for (int i = 0; i < LCD_WIDTH; i++) {
        if (i < len) {
            padded[i] = message[i];
        } else {
            padded[i] = ' ';
        }
    }
    padded[LCD_WIDTH] = '\0'; // Safe null-termination

    lcd_byte(line, LCD_CMD); // Set cursor position
    for (int i = 0; i < LCD_WIDTH; i++) {
        lcd_byte(padded[i], LCD_CHR); // Send each character
    }
}

// Smooth scrolling marquee text across a single LCD line
void marquee_smooth(const char *message, uint8_t line, int delay_ms, int cycles) {
    int msg_len = strlen(message);
    char scroll_text[64];                                                             // Buffer to hold scrollable text
    snprintf(scroll_text, sizeof(scroll_text), "%-*s", msg_len + LCD_WIDTH, message); // Pad with spaces

    for (int c = 0; c < cycles; c++) {
        for (int pos = 0; pos < msg_len + LCD_WIDTH; pos++) {
            char window[LCD_WIDTH + 1];
            for (int i = 0; i < LCD_WIDTH; i++) {
                window[i] = scroll_text[(pos + i) % (msg_len + LCD_WIDTH)];
            }
            window[LCD_WIDTH] = '\0';
            lcd_string(window, line);
            usleep(delay_ms * 1000); // Delay between shifts
        }
    }
}
