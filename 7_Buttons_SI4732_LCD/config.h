/**
 * @file config.h
 * @brief Configuration constants and pin definitions for SI4735 Radio Controller
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS
// ============================================================================

#define RESET_PIN 33

// Encoder pins
#define ENCODER_PIN_A 16
#define ENCODER_PIN_B 17

// Button pins
#define MODE_BUTTON 4
#define BANDWIDTH_BUTTON 5
#define BAND_BUTTON 32
#define SEEK_BUTTON 15
#define AGC_BUTTON 13
#define STEP_BUTTON 14
#define BFO_TOGGLE_BUTTON 19
#define AUDIO_MUTE_PIN 1  // External AUDIO MUTE circuit control (not attached)

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

#define DEBOUNCE_DELAY_MS 50          // Button debounce time
#define DOUBLE_PRESS_DELAY_MS 400     // Maximum time between presses for double-press detection
#define BUTTON_HOLD_THRESHOLD_MS 300  // Hold duration before button is treated as held (not single press)
#define BUTTON_REPEAT_DELAY_MS 300    // Time between button press handling
#define COMMAND_TIMEOUT_MS 500        // Time before command mode auto-disables (Was 2500)
#define RSSI_UPDATE_INTERVAL_MS 900   // RSSI update interval (150ms * 6)
#define MAIN_LOOP_DELAY_MS 1          // Small delay in main loop
#define ENCODER_COMMAND_RATE_LIMIT_MS 150  // Rate limit for encoder in command modes

// ============================================================================
// RADIO CONFIGURATION
// ============================================================================

#define DEFAULT_VOLUME 55
#define BFO_RESET_THRESHOLD 1000      // Hz threshold for VFO/BFO adjustment
#define BFO_STEP_SIZE 10              // BFO adjustment step size in Hz

// Step position constants
#define STEP_POSITION_HZ_THRESHOLD 5  // Positions >= 5 are Hz tuning
#define DEFAULT_SW_STEP_POSITION 4    // Default 1 kHz step for SW bands

// RSSI thresholds for S-meter
#define RSSI_S4_THRESHOLD 2
#define RSSI_S5_THRESHOLD 4
#define RSSI_S6_THRESHOLD 12
#define RSSI_S7_THRESHOLD 25
#define RSSI_S8_THRESHOLD 50

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

#define DISPLAY_BRIGHTNESS 255         // Range: 0-255

// Display positions - Signal Strength
#define SIGNAL_STRENGTH_X_POS 5
#define SIGNAL_STRENGTH_Y_POS 5
#define SIGNAL_STRENGTH_COLOR TFT_GREEN

// Display positions - Frequency
#define FREQ_UNIT_X_POS 250
#define FREQ_UNIT_Y_POS 5

// Display positions - Frequency unit
#define FREQ_X_POS 15 // 20
#define FREQ_Y_POS 60 

// Display positions - BFO
#define BFO_X_POS 258 
#define BFO_Y_POS 80
#define BFO_ZOOM 0.6

// Display positions - Step indicator
#define STEP_INDICATOR_Y_POS 125

// Band label dimensions and position
#define BAND_LABEL_HEIGHT 36
#define BAND_LABEL_WIDTH 145
#define BAND_LABEL_X_POS 5
#define BAND_LABEL_Y_POS 143
#define BAND_LABEL_BACKGROUND TFT_NAVY
#define BAND_LABEL_BORDER TFT_BLUE

// Mode label dimensions and position
#define MODE_LABEL_HEIGHT 36
#define MODE_LABEL_WIDTH 145
#define MODE_LABEL_X_POS 165
#define MODE_LABEL_Y_POS 143
#define MODE_LABEL_BACKGROUND TFT_DARKGREEN
#define MODE_LABEL_BORDER TFT_GREEN

// AGC label dimensions and position
#define AGC_LABEL_HEIGHT 36
#define AGC_LABEL_WIDTH 145
#define AGC_LABEL_X_POS 5
#define AGC_LABEL_Y_POS 194
#define AGC_LABEL_BACKGROUND TFT_MAROON
#define AGC_LABEL_BORDER TFT_RED

// Bandwidth label dimensions and position
#define BANDWIDTH_LABEL_HEIGHT 36
#define BANDWIDTH_LABEL_WIDTH 145
#define BANDWIDTH_LABEL_X_POS 165
#define BANDWIDTH_LABEL_Y_POS 194
#define BANDWIDTH_LABEL_BACKGROUND TFT_VIOLET
#define BANDWIDTH_LABEL_BORDER TFT_SKYBLUE

// Sprite dimensions
#define BFO_SPRITE_WIDTH 170
#define BFO_SPRITE_HEIGHT 120
#define FREQ_SPRITE_WIDTH 175
#define FREQ_SPRITE_HEIGHT 48
#define STEP_SPRITE_WIDTH 185
#define STEP_SPRITE_HEIGHT 4
#define FREQ_UNIT_SPRITE_WIDTH 60
#define FREQ_UNIT_SPRITE_HEIGHT 24
#define SIGNAL_STRENGTH_SPRITE_WIDTH 140
#define SIGNAL_STRENGTH_SPRITE_HEIGHT 24

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================

// Uncomment to enable debug assertions
// #define ENABLE_DEBUG_ASSERTIONS

#ifdef ENABLE_DEBUG_ASSERTIONS
  #define ASSERT(condition, message) \
    if (!(condition)) { \
      Serial.print("ASSERT FAILED: "); \
      Serial.println(message); \
      while(1); \
    }
#else
  #define ASSERT(condition, message)
#endif

#endif // CONFIG_H
