/**
 * A radio receiver for LW, MW, SW (AM/SSB modes) and FM.
 * Uses the SI4732 radio IC with ESP32 with ST7789 display.
 * 
 * Features:
 * - Multiple band support (FM, MW, SW, LW)
 * - AM, LSB, USB demodulation modes
 * - AGC and bandwidth control
 * - Visual frequency display with step indicator
 * 
 * Hardware:
 * - ESP32 Dev Module
 * - SI4732 Radio IC
 * - ST7789 240x320 TFT Display
 * - Rotary encoder
 * - Control buttons
 */

#include <SI4735.h>
#include <LovyanGFX.hpp>
#include <SPI.h>
#include "Rotary.h"
#include "LGFX_ST7789.h"
#include <patch_ssb_compressed.h>

// Project headers
#include "config.h"
#include "types.h"
#include "constants.h"
#include "button_handler.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

LGFX display;
SI4735 radio;
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

// Sprites for display
LGFX_Sprite freqUnitSprite(&display);
LGFX_Sprite bfoSprite(&display);    // bfo 
LGFX_Sprite freqSprite(&display);   // Frequency
LGFX_Sprite stepSprite(&display);
LGFX_Sprite signalStrengthSprite(&display);
LGFX_Sprite bandSprite(&display);
LGFX_Sprite modeSprite(&display);
LGFX_Sprite agcSprite(&display);
LGFX_Sprite bandwidthSprite(&display);

// ============================================================================
// STATE VARIABLES
// ============================================================================

RadioState radioState = {
  .currentFrequency = 0,
  .currentStep = 1,
  .currentMode = MODE_FM,
  .currentBFO = 0,
  .savedBFO = 0,
  .currentBFOStep = BFO_STEP_SIZE,
  .stepPosition = 0,
  .encoderDirection = DIRECTION_UP,
  .ssbPatchLoaded = false,
  .bfoEnabled = false,
  .bfoMode = false
};

AGCState agcState = {
  .index = 0,
  .attenuation = 0,
  .disabled = false
};

// Display cache - initialized with invalid values to force initial display update
DisplayCache displayCache = {
  .frequency = 0xFFFF,     // Invalid value to force initial draw
  .bandIndex = 255,        // Invalid value to force initial draw
  .mode = (RadioMode)255,  // Invalid value to force initial draw
  .rssi = 255,             // Invalid value to force initial draw
  .bandwidth = -1,
  .agc = -1,
  .stereo = false
};

// Button states for debouncing
ButtonState modeButtonState;
ButtonState bandwidthButtonState;
ButtonState bandButtonState;
ButtonState seekButtonState;
ButtonState agcButtonState;
ButtonState stepButtonState;
ButtonState bfoToggleButtonState;

// Current band selection
uint8_t currentBandIndex = DEFAULT_BAND_INDEX;

// Bandwidth indices
int8_t ssbBandwidthIndex = DEFAULT_SSB_BANDWIDTH_INDEX;
int8_t amBandwidthIndex = DEFAULT_AM_BANDWIDTH_INDEX;

// Signal quality
uint8_t currentRSSI = 0;
uint8_t currentSNR = 0;
uint8_t volume = DEFAULT_VOLUME;

// Encoder state (modified by interrupt)
volatile int encoderCount = 0;

// Set true while any button is in hold mode — prevents encoder tuning frequency simultaneously
bool buttonHeld = false;

// Timing
unsigned long lastRSSIUpdate = 0;

// ============================================================================
// Per-band memory
// ============================================================================
uint16_t bandFrequencyMemory[BAND_COUNT];  // Store last frequency for each band
bool bandVisited[BAND_COUNT];              // Track first-visit per band (false = not yet visited)

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Setup and initialization
void setupHardware();
void setupDisplay();
void showSplashScreen();
void initializeRadio();

// Encoder handling
void IRAM_ATTR encoderInterruptHandler();
void handleEncoderInput();

// Button handling
void handleButtons();
void handleModeButton();
void handleBandwidthButton(int8_t direction);
void handleBandButton(int8_t direction);
void handleSeekButton();
void handleAGCButton(int8_t direction);
void handleStepButton(int8_t direction);
void handleBFOToggleButton();

// Radio control
void switchBand(int8_t direction);
void loadSSBPatch();
void configureRadioForBand();
void cycleModeForward();
void adjustBandwidth(int8_t direction);
void adjustAGC(int8_t direction);
void adjustStepPosition(int8_t direction);
void performSeek();

// Frequency control
void updateFrequencyStep();
void tuneFrequency(int8_t direction);
void tuneBFO(int8_t direction);
long calculateDisplayFrequency();

// Display functions
void updateDisplay();
void showFrequency();
void showMode();
void showBandwidth();
void showAGC();
void showRSSI();
void showStepIndicator();
void drawLabel(LGFX_Sprite &sprite, int width, int height, const char* text,
               uint16_t bgColor, uint16_t borderColor);

// Utility functions
const char* getModeDescription(RadioMode mode);
bool shouldUpdateRSSI();
void updateRSSIIfNeeded();
uint8_t calculateSMeterReading(uint8_t rssi);

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("SI4735 Radio Controller - Starting...");
  
  // Initialize frequency memory with default frequencies, and mark all bands as unvisited
  for (uint8_t i = 0; i < BAND_COUNT; i++) {
    bandFrequencyMemory[i] = BAND_TABLE[i].defaultFreq;
    bandVisited[i] = false;
  }
  
  setupHardware();
  setupDisplay();
  showSplashScreen();
  initializeRadio();
  
  Serial.println("Initialization complete");
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================

void loop() {
  handleEncoderInput();
  handleButtons();
  updateRSSIIfNeeded();
  
  delay(MAIN_LOOP_DELAY_MS);
}

// ============================================================================
// SETUP FUNCTIONS
// ============================================================================

void setupHardware() {
  // Configure encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  
  // Configure button pins
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(BANDWIDTH_BUTTON, INPUT_PULLUP);
  pinMode(BAND_BUTTON, INPUT_PULLUP);
  pinMode(SEEK_BUTTON, INPUT_PULLUP);
  pinMode(AGC_BUTTON, INPUT_PULLUP);
  pinMode(STEP_BUTTON, INPUT_PULLUP);
  pinMode(BFO_TOGGLE_BUTTON, INPUT_PULLUP);
  
  // Initialize button states
  initButtonState(modeButtonState);
  initButtonState(bandwidthButtonState);
  initButtonState(bandButtonState);
  initButtonState(seekButtonState);
  initButtonState(agcButtonState);
  initButtonState(stepButtonState);
  initButtonState(bfoToggleButtonState);
  
  // Attach encoder interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderInterruptHandler, CHANGE);
}

void setupDisplay() {
  display.init();

  display.setRotation(1);  // Landscape mode (320x240)
  display.setBrightness(DISPLAY_BRIGHTNESS);
  
  // Create sprites
  bfoSprite.createSprite(BFO_SPRITE_WIDTH, BFO_SPRITE_HEIGHT);
  freqSprite.createSprite(FREQ_SPRITE_WIDTH, FREQ_SPRITE_HEIGHT);
  stepSprite.createSprite(STEP_SPRITE_WIDTH, STEP_SPRITE_HEIGHT);
  freqUnitSprite.createSprite(FREQ_UNIT_SPRITE_WIDTH, FREQ_UNIT_SPRITE_HEIGHT);
  signalStrengthSprite.createSprite(SIGNAL_STRENGTH_SPRITE_WIDTH, 
                                    SIGNAL_STRENGTH_SPRITE_HEIGHT);
  
  bandSprite.createSprite(BAND_LABEL_WIDTH, BAND_LABEL_HEIGHT);
  modeSprite.createSprite(MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT);
  agcSprite.createSprite(AGC_LABEL_WIDTH, AGC_LABEL_HEIGHT);
  bandwidthSprite.createSprite(BANDWIDTH_LABEL_WIDTH, BANDWIDTH_LABEL_HEIGHT);
}

void showSplashScreen() {
  display.fillScreen(COLOR_BACKGROUND);
  display.setFont(&fonts::Orbitron_Light_32);
  display.setTextDatum(TC_DATUM);  // Top Center alignment
  
  display.setTextColor(COLOR_WHITE);
  display.drawString("Si4735", 160, 30);
  
  display.setTextColor(COLOR_BLUE);
  display.drawString("AM FM SW LW", 160, 95);
  
  display.setTextColor(COLOR_LIMEGREEN);
  display.drawString("Radio", 160, 160);
  
  delay(2000);
  display.fillScreen(COLOR_BACKGROUND);
}

void initializeRadio() {
  radio.getDeviceI2CAddress(RESET_PIN);
  radio.setup(RESET_PIN, 0, 1, SI473X_ANALOG_AUDIO);
  delay(300);
  
  configureRadioForBand();
  radio.setVolume(volume);
  //radio.setTuneFrequencyFast(1);  // Fast tuning: 0 = Precision mode, 1 = Fast mode

  updateDisplay();
}

// ============================================================================
// ENCODER HANDLING
// ============================================================================

void IRAM_ATTR encoderInterruptHandler() {
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus) {
    encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
  }
}

void handleEncoderInput() {
  if (encoderCount == 0) return;
  if (buttonHeld) return;  // A button hold is consuming encoderCount directly — don't also tune
  
  int8_t direction = encoderCount;
  encoderCount = 0;
  
  // Normal tuning — button hold modes consume encoderCount directly in handleButtons()
  tuneFrequency(direction);
  showFrequency();
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void handleButtons() {
  // BFO toggle — double-press resets BFO (SSB mode) or frequency (VFO mode), on any band
  handleBFOToggleButton();

  // --- BAND button: single-press steps band, hold+encoder scrolls ---
  {
    int8_t result = handleButtonSingleOrHeld(BAND_BUTTON, bandButtonState, encoderCount);
    if (result == 1) {
      handleBandButton(radioState.encoderDirection == DIRECTION_UP ? 1 : -1);
    } else if (result != 0) {
      handleBandButton(result);
    }
  }

  // --- BANDWIDTH button ---
  {
    int8_t result = handleButtonSingleOrHeld(BANDWIDTH_BUTTON, bandwidthButtonState, encoderCount);
    if (result == 1) {
      handleBandwidthButton(radioState.encoderDirection == DIRECTION_UP ? 1 : -1);
    } else if (result != 0) {
      handleBandwidthButton(result);
    }
  }

  // --- SEEK button — unchanged simple press ---
  if (handleButtonPress(SEEK_BUTTON, seekButtonState)) {
    handleSeekButton();
  }

  // --- AGC button ---
  {
    int8_t result = handleButtonSingleOrHeld(AGC_BUTTON, agcButtonState, encoderCount);
    if (result == 1) {
      handleAGCButton(radioState.encoderDirection == DIRECTION_UP ? 1 : -1);
    } else if (result != 0) {
      handleAGCButton(result);
    }
  }

  // --- STEP button ---
  {
    int8_t result = handleButtonSingleOrHeld(STEP_BUTTON, stepButtonState, encoderCount);
    if (result == 1) {
      handleStepButton(radioState.encoderDirection == DIRECTION_UP ? 1 : -1);
    } else if (result != 0) {
      handleStepButton(result);
    }
  }

  // --- MODE button — unchanged simple press ---
  if (handleButtonPress(MODE_BUTTON, modeButtonState)) {
    handleModeButton();
  }

  // Update global hold flag — true if any button is currently in hold mode.
  // handleEncoderInput() reads this to avoid tuning frequency simultaneously.
  buttonHeld = bandButtonState.holdActive    ||
               bandwidthButtonState.holdActive ||
               agcButtonState.holdActive      ||
               stepButtonState.holdActive;
}

void handleModeButton() {
  cycleModeForward();
}

void handleBandwidthButton(int8_t direction) {
  adjustBandwidth(direction);
}

void handleBandButton(int8_t direction) {
  bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  switchBand(direction);
}

void handleSeekButton() {
  performSeek();
}

void handleAGCButton(int8_t direction) {
  adjustAGC(direction);
}

void handleStepButton(int8_t direction) {
  adjustStepPosition(direction);
}

void handleBFOToggleButton() {
  const Band& band = BAND_TABLE[currentBandIndex];

  uint8_t press = handleButtonPressWithDoubleClick(BFO_TOGGLE_BUTTON, bfoToggleButtonState);

  if (press == 2) {
    // Double press behaviour depends on current tuning mode:
    // - BFO mode active → reset BFO offset to 0
    // - VFO mode (or BFO disabled) → reset frequency to band default
    if (radioState.bfoEnabled && radioState.bfoMode) {
      // Reset BFO to 0
      radioState.currentBFO = 0;
      radioState.savedBFO = 0;
      radio.setSSBBfo(0);
      showFrequency();
      showStepIndicator();
    } else {
      // Reset frequency to band default
      radioState.currentFrequency = band.defaultFreq;
      bandFrequencyMemory[currentBandIndex] = band.defaultFreq;
      radio.setFrequency(radioState.currentFrequency);
      showFrequency();
    }
  }
  else if (press == 1) {
    // Single press only meaningful on SW bands in SSB mode
    if (band.type != BAND_TYPE_SW) return;
    if (radioState.currentMode != MODE_LSB && radioState.currentMode != MODE_USB) return;

    if (!radioState.bfoEnabled) {
      // BFO was disabled — re-enable with saved value
      radioState.bfoEnabled = true;
      radioState.bfoMode = true;
      radioState.currentBFO = radioState.savedBFO;
      radio.setSSBBfo(radioState.currentBFO);
    } else {
      // BFO enabled — toggle between VFO and BFO adjustment modes
      radioState.bfoMode = !radioState.bfoMode;
    }
    showFrequency();
    showStepIndicator();
  }
}

// ============================================================================
// RADIO CONTROL FUNCTIONS
// ============================================================================

void switchBand(int8_t direction) {
  ASSERT(currentBandIndex < BAND_COUNT, "Invalid band index");
  
  // Save current frequency and BFO state
  bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  
  // Get current band type to check if we're leaving/entering SW
  const Band& currentBand = BAND_TABLE[currentBandIndex];
  bool wasOnSW = (currentBand.type == BAND_TYPE_SW);
  
  // Switch band
  if (direction > 0) {
    currentBandIndex = (currentBandIndex < BAND_COUNT - 1) ? currentBandIndex + 1 : 0;
  } else {
    currentBandIndex = (currentBandIndex > 0) ? currentBandIndex - 1 : BAND_COUNT - 1;
  }
  
  // Get new band type
  const Band& newBand = BAND_TABLE[currentBandIndex];
  bool nowOnSW = (newBand.type == BAND_TYPE_SW);
  
  // Handle BFO state transitions
  if (wasOnSW && !nowOnSW) {
    // Leaving SW band - disable BFO display but remember the value
    radioState.bfoEnabled = false;
    radioState.bfoMode = false;
  } else if (!wasOnSW && nowOnSW) {
    // Entering SW band - restore BFO state if it was previously enabled
    // BFO will be re-applied in configureRadioForBand
  }
  
  configureRadioForBand();
}

void loadSSBPatch() {
  const uint16_t contentSize = sizeof(ssb_patch_content);
  const uint16_t cmd0x15Size = sizeof(cmd_0x15);
  
  radio.setI2CFastModeCustom(500000);
  radio.queryLibraryId();
  radio.patchPowerUp();
  delay(50);
  radio.downloadCompressedPatch(ssb_patch_content, contentSize, cmd_0x15, cmd0x15Size);
  radio.setSSBConfig(SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex, 1, 0, 1, 0, 1);
  radio.setI2CStandardMode();
  
  radioState.ssbPatchLoaded = true;
}

void configureRadioForBand() {
  ASSERT(currentBandIndex < BAND_COUNT, "Band index out of range");
  
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Get stored frequency for this band
  uint16_t storedFreq = bandFrequencyMemory[currentBandIndex];
  
  if (band.type == BAND_TYPE_FM) {
    // FM configuration
    radioState.currentMode = MODE_FM;
    radio.setTuneFrequencyAntennaCapacitor(0);
    radio.setFM(band.minimumFreq, band.maximumFreq, storedFreq, band.defaultStep);
    radio.setSeekFmLimits(band.minimumFreq, band.maximumFreq);
    radioState.ssbPatchLoaded = false;
    radioState.stepPosition = 2;  // Default to 10kHz step (FM_STEP_TABLE[2] = 10)
    
    // Disable BFO on FM
    radioState.bfoEnabled = false;
    radioState.bfoMode = false;
  }
  else {
    // AM/SW/MW/LW configuration
    radio.setTuneFrequencyAntennaCapacitor(
      (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) ? 0 : 1
    );
    
    bool isSWBand = (band.type == BAND_TYPE_SW);
    bool isSSBMode = (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB);
    
    if (isSWBand && isSSBMode) {
      // SW band in LSB/USB mode - use SSB demodulator
      if (!radioState.ssbPatchLoaded) {
        loadSSBPatch();
      }
      radio.setSSB(band.minimumFreq, band.maximumFreq, storedFreq,
                   band.defaultStep, radioState.currentMode);
      radio.setSSBAutomaticVolumeControl(0);
      radio.setSSBBfo(radioState.currentBFO);
    }
    else {
      // AM mode on any band (including SW), or MW/LW - use standard AM demodulator
      // If SSB patch was previously loaded, it must be unloaded by power-cycling the chip.
      // radio.setAM() handles this internally via a reset/powerup sequence.
      if (radioState.ssbPatchLoaded) {
        // Force a clean AM setup - the SI4735 library's setAM() will re-initialise
        // the chip, clearing the SSB patch from RAM.
        radioState.ssbPatchLoaded = false;
      }
      
      // Ensure we are in AM mode when switching to a non-SSB configuration
      if (radioState.currentMode != MODE_AM) {
        radioState.currentMode = MODE_AM;
      }
      
      radio.setAM(band.minimumFreq, band.maximumFreq, storedFreq, band.defaultStep);
      
      if (!isSWBand) {
        // Disable BFO on non-SW bands
        radioState.bfoEnabled = false;
        radioState.bfoMode = false;
      }
    }
    
    // Soft mute configuration
    // Set the Am Soft Mute Max Attenuation. The value 0 disables soft mute. 1-8 (Default is 8 dB)
    radio.setAmSoftMuteMaxAttenuation(0); 

    // Set the attack and decay rates when entering or leaving soft mute 1-255 (Default is ~64 - [64 x 4.35 = 278])
    radio.setAMSoftMuteRate(64);

    // Set the AM attenuation slope during soft mute. 1-5 (Default is 1)
    radio.setAMSoftMuteSlop(1);

    // Set the FM Soft Mute Max Attenuation. The value 0 disables soft mute on FM mode.
    radio.setFmSoftMuteMaxAttenuation(16);

    // Set the soft mute release rate. 1–32767 (default is 8192) (approximately 8000 dB/s)
    //radio.setAMSoftMuteReleaseRate(8192);

    // Set the soft mute attack rate. 1–32767 (default is 8192) (approximately 8000 dB/s)
    //radio.setAMSoftMuteAttackRate(8192);

    // Set the SNR threshold to engage soft mute. 0-63 (default is 8) 0 is disabled     
    //radio.setAMSoftMuteSnrThreshold(8);
    
    // AGC configuration
    radio.setAutomaticGainControl(agcState.disabled, agcState.attenuation);
    radio.setSeekAmLimits(band.minimumFreq, band.maximumFreq);
    radio.setSeekAmSpacing((band.defaultStep > 10) ? 10 : band.defaultStep);
    
    // Set default step position
    if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
      radioState.stepPosition = 2;  // 1 kHz step
    } else {
      radioState.stepPosition = DEFAULT_SW_STEP_POSITION;  // 1 kHz for SW
    }
  }
  
  delay(100);
  radioState.currentFrequency = storedFreq;  // Use stored frequency instead of default
  currentRSSI = 0;

  // On first visit to this band, apply the band's default mode.
  // Subsequent visits preserve whatever mode the user last selected.
  if (!bandVisited[currentBandIndex]) {
    bandVisited[currentBandIndex] = true;
    // Only apply if the defaultMode differs from what was just configured —
    // FM and non-SW bands are already set correctly by the block above.
    if (band.type == BAND_TYPE_SW && radioState.currentMode != band.defaultMode) {
      radioState.currentMode = band.defaultMode;
      if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
        // Need SSB patch for SSB default modes
        if (!radioState.ssbPatchLoaded) {
          loadSSBPatch();
        }
        radio.setSSB(band.minimumFreq, band.maximumFreq, storedFreq,
                     band.defaultStep, radioState.currentMode);
        radio.setSSBBfo(radioState.currentBFO);
      }
      // AM default on SW is already configured correctly above — no extra call needed
    }
  }

  // Apply the correct step for this band to both state and radio chip.
  // Must be called explicitly here so FM bands also get radio.setFrequencyStep()
  // applied — updateDisplay() skips showStepIndicator() for FM, which previously
  // left the chip's step register stale when switching to FM from another band.
  updateFrequencyStep();
  
  updateDisplay();
}

void cycleModeForward() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  if (band.type != BAND_TYPE_SW) {
    return;  // Mode cycling only available on SW bands (FM/MW/LW are AM/FM only)
  }
  
  // Cycle: AM → LSB → USB → AM
  switch(radioState.currentMode) {
    case MODE_AM:
      radioState.currentMode = MODE_LSB;
      break;
    case MODE_LSB:
      radioState.currentMode = MODE_USB;
      break;
    case MODE_USB:
      radioState.currentMode = MODE_AM;
      break;
    default:
      radioState.currentMode = MODE_AM;
      break;
  }
  
  showMode();
  
  // Reconfigure radio with new mode (only for SW bands)
  if (band.type == BAND_TYPE_SW) {
    if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
      // Switching TO an SSB mode - load patch if needed and engage SSB demodulator
      if (!radioState.ssbPatchLoaded) {
        loadSSBPatch();
      }
      radio.setSSB(band.minimumFreq, band.maximumFreq, radioState.currentFrequency,
                   radioState.currentStep, radioState.currentMode);
      radio.setSSBBfo(radioState.currentBFO);
      showFrequency();  // Refresh display so BFO sprite appears immediately
    }
    else {
      // Switching BACK to AM mode on SW - disengage SSB demodulator entirely.
      // The SSB patch lives in the chip's RAM; calling setAM() triggers a chip
      // re-initialisation that clears it, so reception returns to true AM demodulation.
      radioState.ssbPatchLoaded = false;
      radioState.bfoEnabled = false;
      radioState.bfoMode = false;
      radio.setAM(band.minimumFreq, band.maximumFreq, radioState.currentFrequency,
                  radioState.currentStep);
      showFrequency();  // Refresh display now BFO is gone
    }
  }
  // For non-SW bands, do nothing (mode cycling doesn't affect frequency)
}

void adjustBandwidth(int8_t direction) {
  if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    // SSB bandwidth
    ssbBandwidthIndex += direction;
    if (ssbBandwidthIndex >= SSB_BANDWIDTH_COUNT) ssbBandwidthIndex = 0;
    if (ssbBandwidthIndex < 0) ssbBandwidthIndex = SSB_BANDWIDTH_COUNT - 1;
    
    radio.setSSBAudioBandwidth(SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex);
    
    // Set sideband cutoff filter based on bandwidth
    if (SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex == 0 ||
        SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex == 4 ||
        SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex == 5) {
      radio.setSSBSidebandCutoffFilter(0);
    } else {
      radio.setSSBSidebandCutoffFilter(1);
    }
  }
  else if (radioState.currentMode == MODE_AM) {
    // AM bandwidth
    amBandwidthIndex += direction;
    if (amBandwidthIndex >= AM_BANDWIDTH_COUNT) amBandwidthIndex = 0;
    if (amBandwidthIndex < 0) amBandwidthIndex = AM_BANDWIDTH_COUNT - 1;
    
    radio.setBandwidth(AM_BANDWIDTHS[amBandwidthIndex].deviceIndex, 1);
  }
  
  showBandwidth();
}

void adjustAGC(int8_t direction) {
  agcState.index += direction;
  
  if (agcState.index < AGC_INDEX_MIN) {
    agcState.index = AGC_INDEX_MAX;
  }
  else if (agcState.index > AGC_INDEX_MAX) {
    agcState.index = AGC_INDEX_MIN;
  }
  
  agcState.disabled = (agcState.index > AGC_DISABLED_THRESHOLD);
  
  if (agcState.index > AGC_DISABLED_THRESHOLD) {
    agcState.attenuation = agcState.index - 1;
  } else {
    agcState.attenuation = 0;
  }
  
  radio.setAutomaticGainControl(agcState.disabled, agcState.attenuation);
  showAGC();
}

void adjustStepPosition(int8_t direction) {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Determine max position based on band type
  int maxPosition;
  if (band.type == BAND_TYPE_FM) {
    maxPosition = FM_STEP_COUNT - 1;
  }
  else if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
    maxPosition = MW_STEP_COUNT - 1;
  }
  else {
    // For SW bands, use only kHz step positions (0-4)
    maxPosition = STEP_POSITION_HZ_THRESHOLD - 1;
  }
  
  // Move position with wraparound
  radioState.stepPosition += direction;
  if (radioState.stepPosition > maxPosition) {
    radioState.stepPosition = 0;
  }
  else if (radioState.stepPosition < 0) {
    radioState.stepPosition = maxPosition;
  }
  
  updateFrequencyStep();
  showStepIndicator();
}

void performSeek() {
  // Callback for showing frequency during seek
  auto seekCallback = [](uint16_t freq) {
    radioState.currentFrequency = freq;
    showFrequency();
  };
  
  radio.seekStationProgress(seekCallback, radioState.encoderDirection);
  radioState.currentFrequency = radio.getFrequency();
  
  // Update frequency memory
  bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
}

// ============================================================================
// FREQUENCY CONTROL
// ============================================================================

void updateFrequencyStep() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  if (band.type == BAND_TYPE_FM) {
    // FM step lookup
    int pos = min(radioState.stepPosition, FM_STEP_COUNT - 1);
    radioState.currentStep = FM_STEP_TABLE[pos];
  }
  else if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
    // MW/LW step lookup
    int pos = min(radioState.stepPosition, MW_STEP_COUNT - 1);
    radioState.currentStep = MW_STEP_TABLE[pos];
  }
  else {
    // SW band - always use kHz steps (0-4)
    int pos = min(radioState.stepPosition, STEP_POSITION_HZ_THRESHOLD - 1);
    radioState.currentStep = SW_KHZ_STEP_TABLE[pos];
  }
  
  radio.setFrequencyStep(radioState.currentStep);
}

void tuneFrequency(int8_t direction) {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Check if we should use BFO tuning
  // BFO tuning is active when:
  // 1. We're on a SW band AND
  // 2. BFO is enabled AND
  // 3. We're in BFO mode (not VFO mode)
  bool useBFOTuning = (band.type == BAND_TYPE_SW) && 
                      radioState.bfoEnabled && 
                      radioState.bfoMode;
  
  if (useBFOTuning) {
    tuneBFO(direction);
  } else {
    // Normal VFO tuning
    if (direction > 0) {
      radio.frequencyUp();
      radioState.encoderDirection = DIRECTION_UP;
    } else {
      radio.frequencyDown();
      radioState.encoderDirection = DIRECTION_DOWN;
    }
    radioState.currentFrequency = radio.getFrequency();
    
    // Update frequency memory
    bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  }
}

void tuneBFO(int8_t direction) {
  // MODIFIED: BFO now operates independently without affecting VFO
  int newBFO = (direction > 0) ? 
               (radioState.currentBFO + BFO_STEP_SIZE) :
               (radioState.currentBFO - BFO_STEP_SIZE);
  
  // Constrain BFO to ±8000 Hz range
  if (newBFO > 8000) {
    newBFO = 8000;
  } else if (newBFO < -8000) {
    newBFO = -8000;
  }
  
  radioState.currentBFO = newBFO;
  radioState.savedBFO = newBFO;  // Save for potential re-enable
  radio.setSSBBfo(radioState.currentBFO);
}

// ****************** IS THIS NEEDED ******************
long calculateDisplayFrequency() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // MODIFIED: Display frequency is ALWAYS just the VFO frequency
  // BFO only affects the actual received frequency, not the display
  return (long)radioState.currentFrequency * 1000L;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void updateDisplay() {
  display.fillScreen(COLOR_BACKGROUND);
  
  // Reset display cache to force redraw
  displayCache.bandIndex = 255;
  displayCache.frequency = 0xFFFF;
  displayCache.mode = (RadioMode)255;  // Force mode redraw
  displayCache.rssi = 255;
  
  showMode();
  
  // Show frequency unit
  const Band& band = BAND_TABLE[currentBandIndex];
  const char* unit = (band.type == BAND_TYPE_FM || band.type == BAND_TYPE_SW) ? "MHz" : "kHz";
  
  freqUnitSprite.fillSprite(TFT_BLACK);
  freqUnitSprite.setFont(&fonts::Orbitron_Light_24);
  freqUnitSprite.setTextSize(1);
  freqUnitSprite.setTextColor(COLOR_WHITE);
  freqUnitSprite.drawString(unit, 0, 0);
  freqUnitSprite.pushSprite(FREQ_UNIT_X_POS, FREQ_UNIT_Y_POS);
  
  // Show other displays
  if (band.type != BAND_TYPE_FM) {
    showStepIndicator();
    showAGC();
    showBandwidth();
  }
  
  showRSSI();
  showFrequency();
}

void showFrequency() {
  const Band& band = BAND_TABLE[currentBandIndex];
  long actualFreqHz = calculateDisplayFrequency();
  
  // Clear sprites
  freqSprite.fillSprite(TFT_BLACK);
  bfoSprite.fillSprite(TFT_BLACK);
  
  // Configure sprite
  freqSprite.setTextFont(&fonts::Font7);
  freqSprite.setTextColor(COLOR_WHITE);
  freqSprite.setCursor(0, 0);
  freqSprite.setTextDatum(TR_DATUM);
  
  char buffer[10];
  
  if (band.type == BAND_TYPE_FM) {
    // FM: XX.X MHz
    long millions = actualFreqHz / 100000;
    long thousands = (actualFreqHz / 10000) % 10;
    snprintf(buffer, sizeof(buffer), "%ld.%01ld", millions, thousands);
  }
  else if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
    // MW/LW: XXXX kHz
    long thousands = actualFreqHz / 1000;
    snprintf(buffer, sizeof(buffer), "%ld", thousands);
  }
  else {
    // SW: XX.XXX MHz
    if (actualFreqHz >= 1000) {
      long millions = actualFreqHz / 1000000;
      long thousands = (actualFreqHz / 1000) % 1000;
      snprintf(buffer, sizeof(buffer), "%ld.%03ld", millions, thousands);
    }
  }
  
  //freqSprite.drawRoundRect(0, 0, FREQ_SPRITE_WIDTH, FREQ_SPRITE_HEIGHT, 5, COLOR_WHITE); // Draw rectangle only when debuging
  freqSprite.drawString(buffer, FREQ_SPRITE_WIDTH, 0);
  
  // Show BFO only on SW bands in SSB mode (LSB/USB) - not in AM mode
  bool showBFO = (band.type == BAND_TYPE_SW) &&
                 (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB);
  if (showBFO) {
    // Choose background color based on whether BFO mode is active
    uint16_t bfoBackgroundColor;
    if (radioState.bfoEnabled && radioState.bfoMode) {
      // BFO mode active - use highlight color
      bfoBackgroundColor = COLOR_BFO_SELECTED;  // Highlight color
    } else {
      // BFO mode not active - use normal color
      bfoBackgroundColor = COLOR_BFO;  // Normal color
    }
    
    bfoSprite.fillRoundRect(0, 0, BFO_SPRITE_WIDTH, BFO_SPRITE_HEIGHT, 5, bfoBackgroundColor);
    
    // Determine sign character
    char signChar = ' ';
    int absValue = radioState.currentBFO;
    
    if (radioState.currentBFO > 0) {
      signChar = '+';
    } else if (radioState.currentBFO < 0) {
      signChar = '-';
      absValue = -radioState.currentBFO;  // Make positive for display
    }
    
    // Draw "BFO: +" or "BFO: -" label at the top
    bfoSprite.setTextFont(&fonts::DejaVu40); //Orbitron_Light_24);
    bfoSprite.setTextColor(COLOR_WHITE);
    bfoSprite.setTextDatum(TL_DATUM);  // Top-Left alignment
    
    char labelBuffer[8];
    snprintf(labelBuffer, sizeof(labelBuffer), "BFO: %c", signChar);
    bfoSprite.drawString(labelBuffer, 10, 10);
    
    // Draw the absolute value below the label with Font7
    bfoSprite.setTextFont(&fonts::Font7);
    bfoSprite.setTextDatum(BR_DATUM);  // Bottom-Right alignment
    
    char bfoBuffer[10];
    snprintf(bfoBuffer, sizeof(bfoBuffer), "%5d", absValue);
  

    //bfoSprite.drawString(bfoBuffer, BFO_SPRITE_WIDTH -10, 52); // Aligned right
    bfoSprite.drawString(bfoBuffer, BFO_SPRITE_WIDTH -10, 110); // Aligned right
  }
  
  // Push to display
  freqSprite.pushSprite(FREQ_X_POS, FREQ_Y_POS);
  bfoSprite.pushRotateZoom(BFO_X_POS, BFO_Y_POS, 0, BFO_ZOOM, BFO_ZOOM);
  
  showStepIndicator();
  
  displayCache.frequency = radioState.currentFrequency;
}

void showMode() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Show band name
  if (displayCache.bandIndex != currentBandIndex) {
    drawLabel(bandSprite, BAND_LABEL_WIDTH, BAND_LABEL_HEIGHT,
              band.name, BAND_LABEL_BACKGROUND, BAND_LABEL_BORDER);
    bandSprite.pushSprite(BAND_LABEL_X_POS, BAND_LABEL_Y_POS);
    displayCache.bandIndex = currentBandIndex;
  }
  
  // Show mode
  // For FM, check actual stereo status; for others check mode change
  bool needsUpdate = (displayCache.mode != radioState.currentMode);
  
  if (needsUpdate) {
    if (band.type == BAND_TYPE_FM && radio.getCurrentPilot()) {
      drawLabel(modeSprite, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT,
                "Stereo", COLOR_DODGERBLUE, COLOR_BLUE);
    }
    else if (band.type == BAND_TYPE_FM) {
      drawLabel(modeSprite, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT,
                "Mono", TFT_DARKGREY, TFT_LIGHTGREY);
    }
    else {
      drawLabel(modeSprite, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT,
                getModeDescription(radioState.currentMode),
                MODE_LABEL_BACKGROUND, MODE_LABEL_BORDER);
    }
    
    modeSprite.pushSprite(MODE_LABEL_X_POS, MODE_LABEL_Y_POS);
    displayCache.mode = radioState.currentMode;
  }
}

void showBandwidth() {
  char buffer[20];
  
  if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    snprintf(buffer, sizeof(buffer), "BW: %s",
             SSB_BANDWIDTHS[ssbBandwidthIndex].description);
  }
  else if (radioState.currentMode == MODE_AM) {
    snprintf(buffer, sizeof(buffer), "BW: %s",
             AM_BANDWIDTHS[amBandwidthIndex].description);
  }
  else {
    buffer[0] = '\0';
  }
  
  if (buffer[0] != '\0') {
    drawLabel(bandwidthSprite, BANDWIDTH_LABEL_WIDTH, BANDWIDTH_LABEL_HEIGHT,
              buffer, BANDWIDTH_LABEL_BACKGROUND, BANDWIDTH_LABEL_BORDER);
    bandwidthSprite.pushSprite(BANDWIDTH_LABEL_X_POS, BANDWIDTH_LABEL_Y_POS);
  }
}

void showAGC() {
  char buffer[20];
  
  if (agcState.index == 0) {
    snprintf(buffer, sizeof(buffer), "AGC: ON");
  } else {
    char attenStr[10];
    radio.convertToChar(agcState.attenuation, attenStr, 2, 0, '.');
    snprintf(buffer, sizeof(buffer), "ATT: %s", attenStr);
  }
  
  drawLabel(agcSprite, AGC_LABEL_WIDTH, AGC_LABEL_HEIGHT,
            buffer, AGC_LABEL_BACKGROUND, AGC_LABEL_BORDER);
  agcSprite.pushSprite(AGC_LABEL_X_POS, AGC_LABEL_Y_POS);
}

void showRSSI() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Handle stereo/mono indicator for FM
  // Check stereo status and update mode display if it changed
  if (band.type == BAND_TYPE_FM) {
    bool currentStereo = radio.getCurrentPilot();
    if (displayCache.stereo != currentStereo) {
      // Force mode update by temporarily invalidating the cache
      RadioMode savedMode = displayCache.mode;
      displayCache.mode = (RadioMode)255;  // Force update
      showMode();
      displayCache.stereo = currentStereo;
    }
  }
  
  // Calculate S-meter reading
  uint8_t sMeter = calculateSMeterReading(currentRSSI);
  
  if (displayCache.rssi != sMeter) {
    signalStrengthSprite.fillSprite(TFT_BLACK);
    signalStrengthSprite.setFont(&fonts::Orbitron_Light_24);
    signalStrengthSprite.setTextSize(1);
    signalStrengthSprite.setTextColor(SIGNAL_STRENGTH_COLOR);
    
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "S%d%s", sMeter, (sMeter == 9) ? "+" : "");
    signalStrengthSprite.drawString(buffer, 0, 0);
    
    // Draw signal strength bars
    for (int i = 1; i < 10; i++) {
      if (sMeter >= i) {
        signalStrengthSprite.fillRect(60 + (i * 7), 10, 5, 14, TFT_GREEN);
      } else {
        signalStrengthSprite.drawRect(60 + (i * 7), 10, 5, 14, TFT_LIGHTGREY);
      }
    }
    
    signalStrengthSprite.pushSprite(SIGNAL_STRENGTH_X_POS, SIGNAL_STRENGTH_Y_POS);
    displayCache.rssi = sMeter;
  }
}

void showStepIndicator() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  stepSprite.fillSprite(TFT_BLACK);
  
  int xPos = 0;
  bool drawTriangle = true;  // Flag to control whether to draw triangle
  
  // If in BFO mode on SW band in SSB mode, don't draw triangle (BFO sprite background shows mode instead)
  bool isSSBMode = (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB);
  if (band.type == BAND_TYPE_SW && isSSBMode && radioState.bfoEnabled && radioState.bfoMode) {
    drawTriangle = false;  // Skip triangle, BFO background color indicates mode
  }
  else if (band.type == BAND_TYPE_FM) {
    int pos = min(radioState.stepPosition, FM_STEP_COUNT - 1);
    xPos = FM_STEP_POSITIONS[pos];
  }
  else if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
    int pos = min(radioState.stepPosition, MW_STEP_COUNT - 1);
    xPos = MW_STEP_POSITIONS[pos];
  }
  else {
    int pos = min(radioState.stepPosition, SW_STEP_POSITION_COUNT - 1);
    xPos = SW_STEP_POSITIONS[pos];
  }
  
  // Draw triangle indicator (only if not in BFO mode)
  if (drawTriangle) {
    stepSprite.fillTriangle(xPos, 0, xPos - 10, 4, xPos + 10, 4, TFT_RED); // Arrow pointing up
  }
  stepSprite.pushSprite(0, STEP_INDICATOR_Y_POS);
  
  updateFrequencyStep();
}

void drawLabel(LGFX_Sprite &sprite, int width, int height, const char* text,
               uint16_t bgColor, uint16_t borderColor) {
  sprite.fillSprite(TFT_BLACK);
  sprite.fillRoundRect(0, 0, width, height, 5, bgColor);
  sprite.drawRoundRect(0, 0, width, height, 5, borderColor);
  
  sprite.setTextDatum(MC_DATUM);
  sprite.setTextFont(&fonts::Orbitron_Light_24);
  sprite.setTextSize(1);
  sprite.setTextColor(COLOR_WHITE);
  sprite.drawString(text, width / 2, height / 2);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* getModeDescription(RadioMode mode) {
  if (mode >= MODE_DESCRIPTION_COUNT) {
    return "???";
  }
  return MODE_DESCRIPTIONS[mode];
}

bool shouldUpdateRSSI() {
  return (millis() - lastRSSIUpdate) > RSSI_UPDATE_INTERVAL_MS;
}

void updateRSSIIfNeeded() {
  if (!shouldUpdateRSSI()) return;
  
  radio.getCurrentReceivedSignalQuality();
  uint8_t newRSSI = radio.getCurrentRSSI();
  
  if (currentRSSI != newRSSI) {
    currentRSSI = newRSSI;
    currentSNR = radio.getCurrentSNR();
    showRSSI();
  }
  
  lastRSSIUpdate = millis();
}

uint8_t calculateSMeterReading(uint8_t rssi) {
  if (rssi < RSSI_S4_THRESHOLD) return 4;
  if (rssi < RSSI_S5_THRESHOLD) return 5;
  if (rssi < RSSI_S6_THRESHOLD) return 6;
  if (rssi < RSSI_S7_THRESHOLD) return 7;
  if (rssi < RSSI_S8_THRESHOLD) return 8;
  return 9;
}
