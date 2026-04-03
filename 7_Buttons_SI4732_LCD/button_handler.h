/**
 * @file button_handler.h
 * @brief Button debouncing and handling functions
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

// ============================================================================
// BUTTON STATE MANAGEMENT
// ============================================================================

/**
 * @brief Handle button press with debouncing
 * 
 * @param pin GPIO pin number
 * @param state Button state structure
 * @return true if button was just pressed (rising edge), false otherwise
 */
bool handleButtonPress(uint8_t pin, ButtonState& state) {
  bool currentlyPressed = (digitalRead(pin) == LOW);
  unsigned long now = millis();
  
  if (currentlyPressed && !state.isPressed) {
    // Button just pressed - check debounce
    if ((now - state.lastPressTime) > DEBOUNCE_DELAY_MS) {
      state.isPressed = true;
      state.lastPressTime = now;
      return true;  // Valid button press detected
    }
  } else if (!currentlyPressed && state.isPressed) {
    // Button released
    state.isPressed = false;
  }
  
  return false;
}

/**
 * @brief Initialize button state structure
 * 
 * @param state Button state to initialize
 */
void initButtonState(ButtonState& state) {
  state.lastPressTime = 0;
  state.firstPressTime = 0;
  state.pressStartTime = 0;
  state.isPressed = false;
  state.holdActive = false;
  state.clickCount = 0;
}

/**
 * @brief Handle button press with double-press detection
 * 
 * @param pin GPIO pin number
 * @param state Button state structure
 * @return 0 = no press, 1 = single press, 2 = double press
 */
uint8_t handleButtonPressWithDoubleClick(uint8_t pin, ButtonState& state) {
  bool currentlyPressed = (digitalRead(pin) == LOW);
  unsigned long now = millis();
  
  if (currentlyPressed && !state.isPressed) {
    // Button just pressed - check debounce
    if ((now - state.lastPressTime) > DEBOUNCE_DELAY_MS) {
      state.isPressed = true;
      state.lastPressTime = now;
      
      // Check if this is within double-press window
      if (state.clickCount == 0) {
        // First press
        state.firstPressTime = now;
        state.clickCount = 1;
      } else if (state.clickCount == 1 && (now - state.firstPressTime) <= DOUBLE_PRESS_DELAY_MS) {
        // Second press within window - double press detected
        state.clickCount = 0;
        return 2;  // Double press
      }
    }
  } else if (!currentlyPressed && state.isPressed) {
    // Button released
    state.isPressed = false;
  }
  
  // Check if single press timeout expired
  if (state.clickCount == 1 && (now - state.firstPressTime) > DOUBLE_PRESS_DELAY_MS) {
    state.clickCount = 0;
    return 1;  // Single press
  }
  
  return 0;  // No press
}

/**
 * @brief Handle a button that supports single-press and hold modes.
 *
 * Call once per loop() iteration. Returns:
 *    0 = nothing to act on yet
 *    1 = single press confirmed (released before hold threshold)
 *    2 = held, encoder turned clockwise  — apply one step forward
 *   -1 = held, encoder turned counter-clockwise — apply one step backward
 *
 * Using 2 (not +1) for clockwise hold keeps single-press (1) unambiguous.
 * On release after a hold, returns 0 (clean exit, no single-press action).
 *
 * @param pin        GPIO pin number (active LOW)
 * @param state      Button state structure
 * @param encCount   Reference to volatile encoderCount global
 * @return int8_t    0, 1, 2, or -1 as described above
 */
int8_t handleButtonSingleOrHeld(uint8_t pin, ButtonState& state, volatile int& encCount) {
  bool currentlyPressed = (digitalRead(pin) == LOW);
  unsigned long now = millis();

  if (currentlyPressed && !state.isPressed) {
    // Debounce check on the falling edge
    if ((now - state.lastPressTime) > DEBOUNCE_DELAY_MS) {
      state.isPressed = true;
      state.pressStartTime = now;
      state.holdActive = false;
      state.lastPressTime = now;
    }
    return 0;
  }

  if (currentlyPressed && state.isPressed) {
    // Button is being held
    if (!state.holdActive && (now - state.pressStartTime) >= BUTTON_HOLD_THRESHOLD_MS) {
      state.holdActive = true;
    }
    if (state.holdActive) {
      // Consume one encoder tick per loop if available.
      // Return 2 for CW, -1 for CCW — 1 is reserved for single press only.
      if (encCount != 0) {
        int8_t dir = (encCount > 0) ? 2 : -1;
        encCount = 0;
        return dir;
      }
    }
    return 0;
  }

  if (!currentlyPressed && state.isPressed) {
    // Button released
    state.isPressed = false;
    if (!state.holdActive) {
      // Released before hold threshold — single press
      return 1;
    }
    // Released after hold — clean exit
    state.holdActive = false;
    return 0;
  }

  return 0;
}

#endif // BUTTON_HANDLER_H
