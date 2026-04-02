/**
 * @file types.h
 * @brief Type definitions, enumerations, and data structures
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @brief Radio demodulation modes
 */
enum RadioMode : uint8_t {
  MODE_FM = 0,
  MODE_LSB = 1,
  MODE_USB = 2,
  MODE_AM = 3
};

/**
 * @brief Band types for frequency range classification
 */
enum BandType : uint8_t {
  BAND_TYPE_FM = 0,
  BAND_TYPE_MW = 1,
  BAND_TYPE_SW = 2,
  BAND_TYPE_LW = 3
};

/**
 * @brief Encoder/seek direction - used for tuning, seeking, and single-press step direction
 */
enum EncoderDirection : uint8_t {
  DIRECTION_DOWN = 0,
  DIRECTION_UP = 1
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Bandwidth configuration
 */
struct Bandwidth {
  uint8_t deviceIndex;      // SI473X device bandwidth index
  const char *description;  // Human-readable bandwidth description
};

/**
 * @brief Band configuration
 */
struct Band {
  const char *name;         // Band name/label (e.g., "FM", "SW1")
  BandType type;            // Band type classification
  uint16_t minimumFreq;     // Minimum frequency (in device units)
  uint16_t maximumFreq;     // Maximum frequency (in device units)
  uint16_t defaultFreq;     // Default/starting frequency
  uint16_t defaultStep;     // Default tuning step
  RadioMode defaultMode;    // Default demodulation mode for first visit
};

/**
 * @brief Radio state - groups all radio-related state
 */
struct RadioState {
  uint16_t currentFrequency;
  uint16_t currentStep;
  RadioMode currentMode;
  int currentBFO;
  int savedBFO;             // BFO value when disabled (for restoration)
  uint8_t currentBFOStep;
  int stepPosition;
  EncoderDirection encoderDirection;
  bool ssbPatchLoaded;
  bool bfoEnabled;          // BFO enabled/disabled state
  bool bfoMode;             // true = adjusting BFO, false = adjusting VFO
};

/**
 * @brief Button state for debouncing and hold detection
 */
struct ButtonState {
  unsigned long lastPressTime;
  unsigned long firstPressTime;   // Time of first press for double-press detection
  unsigned long pressStartTime;   // Time button went low (for hold threshold)
  bool isPressed;
  bool holdActive;                // true once button has been held past BUTTON_HOLD_THRESHOLD_MS
  uint8_t clickCount;             // Number of clicks detected
};

/**
 * @brief AGC (Automatic Gain Control) state
 */
struct AGCState {
  int8_t index;
  int8_t attenuation;
  bool disabled;
};

/**
 * @brief Display cache - stores last displayed values to avoid unnecessary redraws
 */
struct DisplayCache {
  uint16_t frequency;
  uint8_t bandIndex;
  RadioMode mode;
  uint8_t rssi;
  int8_t bandwidth;
  int8_t agc;
  bool stereo;
};

// ============================================================================
// CONSTANTS AND LOOKUP TABLES
// ============================================================================

// Mode descriptions for display (index matches RadioMode enum values)
// NOTE: MODE_FM (0) is handled separately in showMode() — this entry is a fallback only
const char* const MODE_DESCRIPTIONS[] = {"FM", "LSB", "USB", "AM"};
const uint8_t MODE_DESCRIPTION_COUNT = 4;

// FM tuning step lookup table (in 10kHz units)
const uint16_t FM_STEP_TABLE[] = {1000, 100, 10};
const uint8_t FM_STEP_COUNT = 3;

// MW/LW tuning step lookup table (in kHz)
const uint16_t MW_STEP_TABLE[] = {100, 10, 1};
const uint8_t MW_STEP_COUNT = 3;

// SW kHz tuning step lookup table (positions 0-4)
const uint16_t SW_KHZ_STEP_TABLE[] = {10000, 1000, 100, 10, 1};
const uint8_t SW_KHZ_STEP_COUNT = 5;

// FM step indicator positions (pixel X coordinates)
const int FM_STEP_POSITIONS[] = {96, 133, 173};

// MW/LW step indicator positions (pixel X coordinates)
const int MW_STEP_POSITIONS[] = {109, 141, 173};

// SW step indicator positions (pixel X coordinates)
const int SW_STEP_POSITIONS[] = {30, 65, 109, 141, 173};
//const uint8_t SW_STEP_POSITION_COUNT = 8;
const uint8_t SW_STEP_POSITION_COUNT = 5;

#endif // TYPES_H
