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
  .seekDirection = DIRECTION_UP,
  .ssbPatchLoaded = false,
  .bfoEnabled = false,
  .bfoMode = false,
  .stepJustChanged = false
};

CommandState commandState = {
  .active = CMD_NONE,
  .band = false,
  .agc = false,
  .bandwidth = false,
  .step = false
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

// Display update flags - tracks what needs to be redrawn
struct {
  bool frequency;
  bool mode;
  bool bandwidth;
  bool agc;
  bool rssi;
  bool band;
  bool stepIndicator;
  bool frequencyUnit;
  bool fullRedraw;  // Force complete display refresh
} displayNeedsUpdate = {false};

// Bandwidth indices
int8_t ssbBandwidthIndex = DEFAULT_SSB_BANDWIDTH_INDEX;
int8_t amBandwidthIndex = DEFAULT_AM_BANDWIDTH_INDEX;

// Signal quality
uint8_t currentRSSI = 0;
uint8_t currentSNR = 0;
uint8_t volume = DEFAULT_VOLUME;

// Encoder state (modified by interrupt)
volatile int encoderCount = 0;

// Timing
unsigned long lastCommandTime = 0;
unsigned long lastRSSIUpdate = 0;
unsigned long lastEncoderCommandTime = 0;  // Rate limiting for encoder in command modes

// ============================================================================
// NEW: Frequency memory for all bands
// ============================================================================
uint16_t bandFrequencyMemory[BAND_COUNT];  // Store last frequency for each band

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
void handleBandwidthButton();
void handleBandButton();
void handleSeekButton();
void handleAGCButton();
void handleStepButton();
void handleBFOToggleButton();

// Radio control
void switchBand(int8_t direction);
void loadSSBPatch();
void unloadSSBPatch();
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
void processDisplayUpdates();
void showFrequency();
void showFrequencyUnit();
void showMode();
void showBandwidth();
void showAGC();
void showRSSI();
void showStepIndicator();
void drawLabel(LGFX_Sprite &sprite, int width, int height, const char* text,
               uint16_t bgColor, uint16_t borderColor);

// Utility functions
void resetCommandMode(bool* flagToPreserve = nullptr, bool preserveValue = false,
                     void (*displayCallback)() = nullptr);
const char* getModeDescription(RadioMode mode);
bool shouldUpdateRSSI();
void updateRSSIIfNeeded();
void handleCommandTimeout();
uint8_t calculateSMeterReading(uint8_t rssi);

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("SI4735 Radio Controller - Starting...");
  
  // Initialize frequency memory with default frequencies
  for (uint8_t i = 0; i < BAND_COUNT; i++) {
    bandFrequencyMemory[i] = BAND_TABLE[i].defaultFreq;
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
  handleCommandTimeout();
  
  // Process any pending display updates
  processDisplayUpdates();
  
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
  delay(RADIO_INIT_DELAY_MS);
  
  configureRadioForBand();
  radio.setVolume(volume);
  
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
  
  int8_t direction = encoderCount;
  encoderCount = 0;
  
  // Rate limiting for command modes to prevent over-sensitivity
  // Normal tuning doesn't need rate limiting
  if (commandState.active != CMD_NONE) {
    unsigned long now = millis();
    if ((now - lastEncoderCommandTime) < ENCODER_COMMAND_RATE_LIMIT_MS) {
      return;  // Too soon, ignore this encoder input
    }
    lastEncoderCommandTime = now;
  }
  
  // Route encoder input based on active command
  switch(commandState.active) {
    case CMD_STEP:
      adjustStepPosition(direction);
      break;
      
    case CMD_AGC:
      adjustAGC(direction);
      break;
      
    case CMD_BANDWIDTH:
      adjustBandwidth(direction);
      break;
      
    case CMD_BAND:
      switchBand(direction);
      break;
      
    case CMD_NONE:
    default:
      // Normal tuning (no rate limiting needed)
      tuneFrequency(direction);
      break;
  }
  
  // Set display update flag for frequency
  displayNeedsUpdate.frequency = true;
  
  // Process any pending display updates
  processDisplayUpdates();
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void handleButtons() {
  // Check BFO toggle button
  handleBFOToggleButton();
  
  // Handle band button with double-press support
  uint8_t bandPress = handleButtonPressWithDoubleClick(BAND_BUTTON, bandButtonState);
  if (bandPress == 1) {
    handleBandButton();
  } else if (bandPress == 2) {
    // Double press - reset to default frequency
    const Band& band = BAND_TABLE[currentBandIndex];
    radioState.currentFrequency = band.defaultFreq;
    bandFrequencyMemory[currentBandIndex] = band.defaultFreq;  // Update memory
    radio.setFrequency(radioState.currentFrequency);
    displayNeedsUpdate.frequency = true;
  }
  
  // Handle AGC button with double-press support
  uint8_t agcPress = handleButtonPressWithDoubleClick(AGC_BUTTON, agcButtonState);
  if (agcPress == 1) {
    handleAGCButton();
  } else if (agcPress == 2) {
    // Double press - reset to AGC: ON
    agcState.index = 0;
    agcState.disabled = false;
    agcState.attenuation = 0;
    radio.setAutomaticGainControl(agcState.disabled, agcState.attenuation);
    displayNeedsUpdate.agc = true;
  }
  
  // Handle bandwidth button with double-press support
  uint8_t bwPress = handleButtonPressWithDoubleClick(BANDWIDTH_BUTTON, bandwidthButtonState);
  if (bwPress == 1) {
    handleBandwidthButton();
  } else if (bwPress == 2) {
    // Double press - reset to default bandwidth for current mode
    if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
      ssbBandwidthIndex = DEFAULT_SSB_BANDWIDTH_INDEX;
      radio.setSSBAudioBandwidth(SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex);
    } else if (radioState.currentMode == MODE_AM) {
      amBandwidthIndex = DEFAULT_AM_BANDWIDTH_INDEX;
      radio.setBandwidth(AM_BANDWIDTHS[amBandwidthIndex].deviceIndex, 1);
    }
    displayNeedsUpdate.bandwidth = true;
  }
  
  // Other buttons use regular press detection
  if (handleButtonPress(SEEK_BUTTON, seekButtonState)) {
    handleSeekButton();
  }
  
  // Handle step button with double-press support
  uint8_t stepPress = handleButtonPressWithDoubleClick(STEP_BUTTON, stepButtonState);
  if (stepPress == 1) {
    handleStepButton();
  } else if (stepPress == 2) {
    // Double press - reset to default step for current band
    const Band& band = BAND_TABLE[currentBandIndex];
    
    if (band.type == BAND_TYPE_FM) {
      radioState.stepPosition = 0;  // Default FM step
    } else if (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) {
      radioState.stepPosition = 2;  // Default 1 kHz step for MW/LW
    } else {
      radioState.stepPosition = DEFAULT_SW_STEP_POSITION;  // Default 1 kHz for SW
    }
    
    updateFrequencyStep();
    displayNeedsUpdate.stepIndicator = true;
  }
  
  if (handleButtonPress(MODE_BUTTON, modeButtonState)) {
    handleModeButton();
  }
}

void handleModeButton() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Mode button does nothing on FM band
  if (band.type == BAND_TYPE_FM) {
    return;
  }
  
  cycleModeForward();
  lastCommandTime = millis();
}

void handleBandwidthButton() {
  commandState.bandwidth = !commandState.bandwidth;
  commandState.active = commandState.bandwidth ? CMD_BANDWIDTH : CMD_NONE;
  resetCommandMode(&commandState.bandwidth, commandState.bandwidth, nullptr);
  displayNeedsUpdate.bandwidth = true;
  lastCommandTime = millis();
}

void handleBandButton() {
  // Save current frequency before switching
  bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  
  commandState.band = !commandState.band;
  commandState.active = commandState.band ? CMD_BAND : CMD_NONE;
  resetCommandMode(&commandState.band, commandState.band, nullptr);
  lastCommandTime = millis();
}

void handleSeekButton() {
  performSeek();
}

void handleAGCButton() {
  commandState.agc = !commandState.agc;
  commandState.active = commandState.agc ? CMD_AGC : CMD_NONE;
  resetCommandMode(&commandState.agc, commandState.agc, nullptr);
  displayNeedsUpdate.agc = true;
  lastCommandTime = millis();
}

void handleStepButton() {
  commandState.step = !commandState.step;
  commandState.active = commandState.step ? CMD_STEP : CMD_NONE;
  resetCommandMode(&commandState.step, commandState.step, nullptr);
  displayNeedsUpdate.stepIndicator = true;
  lastCommandTime = millis();
}

void handleBFOToggleButton() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // BFO toggle only works in LSB or USB modes
  if (radioState.currentMode != MODE_LSB && radioState.currentMode != MODE_USB) {
    return;  // Do nothing on AM or FM
  }
  
  // Handle single and double press
  uint8_t press = handleButtonPressWithDoubleClick(BFO_TOGGLE_BUTTON, bfoToggleButtonState);
  
  if (press == 2) {
    // Double press - reset BFO to 0
    radioState.currentBFO = 0;
    radioState.savedBFO = 0;
    radio.setSSBBfo(0);
    displayNeedsUpdate.frequency = true;
    displayNeedsUpdate.stepIndicator = true;
  }
  else if (press == 1) {
    // Single press - toggle between VFO and BFO mode
    radioState.bfoMode = !radioState.bfoMode;
    displayNeedsUpdate.frequency = true;
    displayNeedsUpdate.stepIndicator = true;
  }
}

// ============================================================================
// RADIO CONTROL FUNCTIONS
// ============================================================================

void switchBand(int8_t direction) {
  ASSERT(currentBandIndex < BAND_COUNT, "Invalid band index");
  
  // Save current frequency
  bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  
  // Switch band
  if (direction > 0) {
    currentBandIndex = (currentBandIndex < BAND_COUNT - 1) ? currentBandIndex + 1 : 0;
  } else {
    currentBandIndex = (currentBandIndex > 0) ? currentBandIndex - 1 : BAND_COUNT - 1;
  }
  
  configureRadioForBand();
  lastCommandTime = millis();
}

void loadSSBPatch() {
  Serial.println("Loading SSB patch...");
  
  const uint16_t contentSize = sizeof(ssb_patch_content);
  const uint16_t cmd0x15Size = sizeof(cmd_0x15);
  
  radio.setI2CFastModeCustom(500000);
  radio.queryLibraryId();
  radio.patchPowerUp();
  delay(SSB_PATCH_DELAY_MS);
  radio.downloadCompressedPatch(ssb_patch_content, contentSize, cmd_0x15, cmd0x15Size);
  radio.setSSBConfig(SSB_BANDWIDTHS[ssbBandwidthIndex].deviceIndex, 1, 0, 1, 0, 1);
  radio.setI2CStandardMode();
  
  radioState.ssbPatchLoaded = true;
  Serial.println("SSB patch loaded successfully");
}

void unloadSSBPatch() {
  Serial.println("Unloading SSB patch...");
  
  // The Si4735 requires a reset/powerup to unload the patch
  // We'll just mark it as unloaded and reconfigure for AM
  radioState.ssbPatchLoaded = false;
  
  Serial.println("SSB patch unloaded");
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
    radioState.stepPosition = 2;  // Default to 100kHz step
    
    // Disable BFO on FM
    radioState.bfoMode = false;
  }
  else {
    // AM/SW/MW/LW configuration
    radio.setTuneFrequencyAntennaCapacitor(
      (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) ? 0 : 1
    );
    
    // Configure based on current mode
    // SSB patch should only be loaded for LSB/USB modes
    if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
      // SSB mode - ensure patch is loaded
      if (!radioState.ssbPatchLoaded) {
        loadSSBPatch();
      }
      radio.setSSB(band.minimumFreq, band.maximumFreq, storedFreq, 
                   band.defaultStep, radioState.currentMode);
      radio.setSSBAutomaticVolumeControl(0);
      radio.setSSBBfo(radioState.currentBFO);
    }
    else {
      // AM mode - standard AM reception
      radioState.currentMode = MODE_AM;
      
      // If SSB was loaded, we need to reconfigure to standard AM
      if (radioState.ssbPatchLoaded) {
        // Reset the radio to unload SSB patch
        radio.reset();
        delay(RADIO_RESET_DELAY_MS);
        radio.setup(RESET_PIN, 0, 1, SI473X_ANALOG_AUDIO);
        delay(BAND_SWITCH_DELAY_MS);
        radioState.ssbPatchLoaded = false;
      }
      
      radio.setAM(band.minimumFreq, band.maximumFreq, storedFreq, band.defaultStep);
      radioState.bfoMode = false;  // Disable BFO mode in AM
    }
    
    // Soft mute configuration
    radio.setAmSoftMuteMaxAttenuation(0); 
    radio.setAMSoftMuteRate(64);
    radio.setAMSoftMuteSlop(1);
    radio.setFmSoftMuteMaxAttenuation(16);
    
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
  
  //delay(100);  // original
  radioState.currentFrequency = storedFreq;
  radioState.currentStep = band.defaultStep;
  currentRSSI = 0;
  
  updateDisplay();
}

void cycleModeForward() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  if (band.type == BAND_TYPE_FM) {
    return;  // Can't change mode on FM
  }
  
  RadioMode oldMode = radioState.currentMode;
  
  // Cycle: AM → LSB → USB → AM
  switch(radioState.currentMode) {
    case MODE_AM:
      radioState.currentMode = MODE_LSB;
      // Restore saved BFO value
      radioState.currentBFO = radioState.savedBFO;
      break;
    case MODE_LSB:
      radioState.currentMode = MODE_USB;
      // Keep current BFO value
      break;
    case MODE_USB:
      radioState.currentMode = MODE_AM;
      // Save BFO value
      radioState.savedBFO = radioState.currentBFO;
      radioState.bfoMode = false;  // Exit BFO mode
      break;
    default:
      radioState.currentMode = MODE_AM;
      radioState.bfoMode = false;
      break;
  }
  
  // Load/unload SSB patch as needed
  if ((oldMode == MODE_AM) && 
      (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB)) {
    // Entering SSB mode - load patch
    if (!radioState.ssbPatchLoaded) {
      loadSSBPatch();
    }
    radio.setSSB(band.minimumFreq, band.maximumFreq, radioState.currentFrequency,
                 radioState.currentStep, radioState.currentMode);
    radio.setSSBBfo(radioState.currentBFO);
  }
  else if ((oldMode == MODE_LSB || oldMode == MODE_USB) && 
           (radioState.currentMode == MODE_AM)) {
    // Exiting SSB mode - unload patch and switch to AM
    if (radioState.ssbPatchLoaded) {
      // Reset radio to unload patch
      radio.reset();
      delay(RADIO_RESET_DELAY_MS);
      radio.setup(RESET_PIN, 0, 1, SI473X_ANALOG_AUDIO);
      delay(BAND_SWITCH_DELAY_MS);
      radioState.ssbPatchLoaded = false;
      
      // Reconfigure for AM
      radio.setTuneFrequencyAntennaCapacitor(
        (band.type == BAND_TYPE_MW || band.type == BAND_TYPE_LW) ? 0 : 1
      );
      radio.setAM(band.minimumFreq, band.maximumFreq, radioState.currentFrequency,
                  radioState.currentStep);
      radio.setAmSoftMuteMaxAttenuation(0);
      radio.setAutomaticGainControl(agcState.disabled, agcState.attenuation);
      radio.setVolume(volume);
    }
  }
  else if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    // Switching between LSB and USB - just update mode
    radio.setSSB(band.minimumFreq, band.maximumFreq, radioState.currentFrequency,
                 radioState.currentStep, radioState.currentMode);
    radio.setSSBBfo(radioState.currentBFO);
  }
  
  // Set display update flags
  displayNeedsUpdate.mode = true;
  displayNeedsUpdate.frequency = true;
  displayNeedsUpdate.bandwidth = true;
}

void adjustBandwidth(int8_t direction) {
  if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    // SSB bandwidth
    ssbBandwidthIndex += direction;
    // Clamp instead of wrapping
    ssbBandwidthIndex = constrain(ssbBandwidthIndex, 0, SSB_BANDWIDTH_COUNT - 1);
    
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
    // Clamp instead of wrapping
    amBandwidthIndex = constrain(amBandwidthIndex, 0, AM_BANDWIDTH_COUNT - 1);
    
    radio.setBandwidth(AM_BANDWIDTHS[amBandwidthIndex].deviceIndex, 1);
  }
  
  displayNeedsUpdate.bandwidth = true;
  lastCommandTime = millis();
}

void adjustAGC(int8_t direction) {
  agcState.index += direction;
  
  // Clamp instead of wrapping for more intuitive behavior
  agcState.index = constrain(agcState.index, AGC_INDEX_MIN, AGC_INDEX_MAX);
  
  // AGC is disabled when index > 0 (manual attenuation mode)
  agcState.disabled = (agcState.index > 0);
  
  // Calculate attenuation value for manual mode
  if (agcState.index > 1) {
    agcState.attenuation = agcState.index - 1;  // Index 2=ATT:1, Index 3=ATT:2, etc.
  } else {
    agcState.attenuation = 0;  // Index 0=AGC:ON, Index 1=ATT:0
  }
  
  radio.setAutomaticGainControl(agcState.disabled, agcState.attenuation);
  displayNeedsUpdate.agc = true;
  lastCommandTime = millis();
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
  
  // Set flag to indicate step just changed - next frequency adjustment will round
  radioState.stepJustChanged = true;
  
  displayNeedsUpdate.stepIndicator = true;
  lastCommandTime = millis();
}

void performSeek() {
  // Callback for showing frequency during seek
  auto seekCallback = [](uint16_t freq) {
    radioState.currentFrequency = freq;
    showFrequency();
  };
  
  radio.seekStationProgress(seekCallback, radioState.seekDirection);
  radioState.currentFrequency = radio.getFrequency();
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
  // Check if we should use BFO tuning
  // BFO tuning is active when:
  // 1. We're in LSB or USB mode AND
  // 2. We're in BFO adjustment mode (not VFO mode)
  bool useBFOTuning = (radioState.currentMode == MODE_LSB || 
                       radioState.currentMode == MODE_USB) && 
                      radioState.bfoMode;
  
  if (useBFOTuning) {
    tuneBFO(direction);
  } else {
    // Check if we need to round to step boundary (after step change)
    if (radioState.stepJustChanged) {
      // Calculate the rounded frequency based on direction
      uint16_t currentFreq = radioState.currentFrequency;
      uint16_t step = radioState.currentStep;
      uint16_t newFreq;
      
      if (direction > 0) {
        // Round UP to next step boundary
        // Calculate how far we are into the current step
        uint16_t remainder = currentFreq % step;
        if (remainder == 0) {
          // Already on boundary, just move forward normally
          newFreq = currentFreq + step;
        } else {
          // Round up to next boundary
          newFreq = currentFreq + (step - remainder);
        }
      } else {
        // Round DOWN to previous step boundary
        // Calculate how far we are into the current step
        uint16_t remainder = currentFreq % step;
        if (remainder == 0) {
          // Already on boundary, just move backward normally
          newFreq = currentFreq - step;
        } else {
          // Round down to previous boundary
          newFreq = currentFreq - remainder;
        }
      }
      
      // Apply band limits
      const Band& band = BAND_TABLE[currentBandIndex];
      if (newFreq < band.minimumFreq) {
        newFreq = band.minimumFreq;
      } else if (newFreq > band.maximumFreq) {
        newFreq = band.maximumFreq;
      }
      
      // Set the new frequency
      radio.setFrequency(newFreq);
      radioState.currentFrequency = newFreq;
      radioState.seekDirection = (direction > 0) ? DIRECTION_UP : DIRECTION_DOWN;
      
      // Clear the flag - rounding is done, subsequent adjustments will be normal
      radioState.stepJustChanged = false;
    } else {
      // Normal VFO tuning (no rounding needed)
      if (direction > 0) {
        radio.frequencyUp();
        radioState.seekDirection = DIRECTION_UP;
      } else {
        radio.frequencyDown();
        radioState.seekDirection = DIRECTION_DOWN;
      }
      radioState.currentFrequency = radio.getFrequency();
    }
    
    // Update frequency memory
    bandFrequencyMemory[currentBandIndex] = radioState.currentFrequency;
  }
}

void tuneBFO(int8_t direction) {
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
  radioState.savedBFO = newBFO;  // Always keep savedBFO in sync
  radio.setSSBBfo(radioState.currentBFO);
}

long calculateDisplayFrequency() {
  // Display frequency is ALWAYS just the VFO frequency
  // BFO only affects the actual received frequency, not the display
  return (long)radioState.currentFrequency * 1000L;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void updateDisplay() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Reset display cache to force redraw of everything
  displayCache.bandIndex = 255;
  displayCache.frequency = 0xFFFF;
  displayCache.mode = (RadioMode)255;
  displayCache.rssi = 255;
  displayCache.bandwidth = -1;
  displayCache.agc = -1;
  
  // Set flags for what needs to be drawn based on band type
  displayNeedsUpdate.band = true;
  displayNeedsUpdate.mode = true;
  displayNeedsUpdate.frequency = true;
  displayNeedsUpdate.frequencyUnit = true;
  displayNeedsUpdate.rssi = true;
  
  // Non-FM bands show additional controls
  if (band.type != BAND_TYPE_FM) {
    displayNeedsUpdate.stepIndicator = true;
    displayNeedsUpdate.agc = true;
    displayNeedsUpdate.bandwidth = true;
  } else {
    // FM band - hide AGC and bandwidth sprites by filling with background colour
    agcSprite.fillSprite(COLOR_BACKGROUND);
    agcSprite.pushSprite(AGC_LABEL_X_POS, AGC_LABEL_Y_POS);
    
    bandwidthSprite.fillSprite(COLOR_BACKGROUND);
    bandwidthSprite.pushSprite(BANDWIDTH_LABEL_X_POS, BANDWIDTH_LABEL_Y_POS);
    
    // Step indicator IS still relevant on FM - update it
    displayNeedsUpdate.stepIndicator = true;
    
    // Invalidate cache so they redraw properly when leaving FM
    displayCache.bandwidth = -1;
    displayCache.agc = -1;
  }
  
  // Process all pending display updates
  processDisplayUpdates();
}

void processDisplayUpdates() {
  // Process display updates based on flags
  // This centralizes all display logic in one place
  
  if (displayNeedsUpdate.band) {
    showMode();  // Shows band name
    displayNeedsUpdate.band = false;
  }
  
  if (displayNeedsUpdate.mode) {
    showMode();  // Shows mode (LSB/USB/AM/Stereo/Mono)
    displayNeedsUpdate.mode = false;
  }
  
  if (displayNeedsUpdate.frequencyUnit) {
    showFrequencyUnit();
    displayNeedsUpdate.frequencyUnit = false;
  }
  
  if (displayNeedsUpdate.frequency) {
    showFrequency();
    displayNeedsUpdate.frequency = false;
  }
  
  if (displayNeedsUpdate.stepIndicator) {
    showStepIndicator();
    displayNeedsUpdate.stepIndicator = false;
  }
  
  if (displayNeedsUpdate.agc) {
    showAGC();
    displayNeedsUpdate.agc = false;
  }
  
  if (displayNeedsUpdate.bandwidth) {
    showBandwidth();
    displayNeedsUpdate.bandwidth = false;
  }
  
  if (displayNeedsUpdate.rssi) {
    showRSSI();
    displayNeedsUpdate.rssi = false;
  }
}

void showFrequencyUnit() {
  const Band& band = BAND_TABLE[currentBandIndex];
  const char* unit = (band.type == BAND_TYPE_FM || band.type == BAND_TYPE_SW) ? "MHz" : "kHz";
  
  freqUnitSprite.fillSprite(TFT_BLACK);
  freqUnitSprite.setFont(&fonts::Orbitron_Light_24);
  freqUnitSprite.setTextSize(1);
  freqUnitSprite.setTextColor(COLOR_WHITE);
  freqUnitSprite.drawString(unit, 0, 0);
  freqUnitSprite.pushSprite(FREQ_UNIT_X_POS, FREQ_UNIT_Y_POS);
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
    // SW: XX.XXX kHz
    if (actualFreqHz >= 1000) {
      long millions = actualFreqHz / 1000000;
      long thousands = (actualFreqHz / 1000) % 1000;
      snprintf(buffer, sizeof(buffer), "%ld.%03ld", millions, thousands);
    }
  }
  
  freqSprite.drawString(buffer, FREQ_SPRITE_WIDTH, 0);
  
  // Show BFO only when in LSB or USB mode on SW bands
  if (band.type == BAND_TYPE_SW && 
      (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB)) {
    // Choose background color based on whether BFO mode is active
    uint16_t bfoBackgroundColor;
    if (radioState.bfoMode) {
      // BFO mode active - use highlight color
      bfoBackgroundColor = COLOR_BFO_SELECTED;
    } else {
      // BFO mode not active - use normal color
      bfoBackgroundColor = COLOR_BFO;
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
    bfoSprite.setTextFont(&fonts::DejaVu40);
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
    
    bfoSprite.drawString(bfoBuffer, BFO_SPRITE_WIDTH - 10, 110); // Aligned right
  }
  
  // Push to display
  freqSprite.pushSprite(FREQ_X_POS, FREQ_Y_POS);
  bfoSprite.pushRotateZoom(BFO_X_POS, BFO_Y_POS, 0, BFO_ZOOM, BFO_ZOOM);
  
  showStepIndicator();
  
  displayCache.frequency = radioState.currentFrequency;
}

void showMode() {
  const Band& band = BAND_TABLE[currentBandIndex];
  
  // Band name
  if (displayCache.bandIndex != currentBandIndex) {
    drawLabel(bandSprite, BAND_LABEL_WIDTH, BAND_LABEL_HEIGHT, 
              band.name, BAND_LABEL_BACKGROUND, BAND_LABEL_BORDER);
    bandSprite.pushSprite(BAND_LABEL_X_POS, BAND_LABEL_Y_POS);
    displayCache.bandIndex = currentBandIndex;
  }
  
  // Mode/stereo indicator
  if (displayCache.mode != radioState.currentMode || 
      (band.type == BAND_TYPE_FM && displayCache.stereo != radio.getCurrentPilot())) {
    
    const char* modeText;
    if (band.type == BAND_TYPE_FM) {
      bool isStereo = radio.getCurrentPilot();
      modeText = isStereo ? "Stereo" : "Mono";
      displayCache.stereo = isStereo;
    } else {
      modeText = getModeDescription(radioState.currentMode);
    }
    
    drawLabel(modeSprite, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT,
              modeText, MODE_LABEL_BACKGROUND, MODE_LABEL_BORDER);
    modeSprite.pushSprite(MODE_LABEL_X_POS, MODE_LABEL_Y_POS);
    displayCache.mode = radioState.currentMode;
  }
}

void showBandwidth() {
  // Calculate current bandwidth index based on mode
  int8_t currentBwIdx;
  if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    currentBwIdx = ssbBandwidthIndex;
  } else if (radioState.currentMode == MODE_AM) {
    currentBwIdx = amBandwidthIndex + 100;  // Offset to distinguish from SSB
  } else {
    currentBwIdx = -1;  // FM or other (no bandwidth control)
  }
  
  // Check cache - skip redraw if unchanged
  if (displayCache.bandwidth == currentBwIdx) {
    return;  // No change, skip unnecessary redraw
  }
  displayCache.bandwidth = currentBwIdx;
  
  // Bandwidth has changed, update display
  const char* bwText;
  char buffer[12];
  
  if (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) {
    snprintf(buffer, sizeof(buffer), "BW: %s", 
             SSB_BANDWIDTHS[ssbBandwidthIndex].description);
    bwText = buffer;
  }
  else if (radioState.currentMode == MODE_AM) {
    snprintf(buffer, sizeof(buffer), "BW: %s",
             AM_BANDWIDTHS[amBandwidthIndex].description);
    bwText = buffer;
  }
  else {
    bwText = "";
  }
  
  drawLabel(bandwidthSprite, BANDWIDTH_LABEL_WIDTH, BANDWIDTH_LABEL_HEIGHT,
            bwText, BANDWIDTH_LABEL_BACKGROUND, BANDWIDTH_LABEL_BORDER);
  bandwidthSprite.pushSprite(BANDWIDTH_LABEL_X_POS, BANDWIDTH_LABEL_Y_POS);
}

void showAGC() {
  // Check cache - skip redraw if unchanged
  if (displayCache.agc == agcState.index) {
    return;  // No change, skip unnecessary redraw
  }
  displayCache.agc = agcState.index;
  
  // AGC has changed, update display
  char buffer[12];
  
  if (agcState.index == 0) {
    strcpy(buffer, "AGC: ON");
  } else {
    snprintf(buffer, sizeof(buffer), "ATT: %d", agcState.attenuation);
  }
  
  drawLabel(agcSprite, AGC_LABEL_WIDTH, AGC_LABEL_HEIGHT,
            buffer, AGC_LABEL_BACKGROUND, AGC_LABEL_BORDER);
  agcSprite.pushSprite(AGC_LABEL_X_POS, AGC_LABEL_Y_POS);
}

void showRSSI() {
  radio.getCurrentReceivedSignalQuality();
  currentRSSI = radio.getCurrentRSSI();
  currentSNR = radio.getCurrentSNR();
  
  uint8_t sMeter = calculateSMeterReading(currentRSSI);
  
  // Only update if changed
  if (displayCache.rssi != sMeter) {
    signalStrengthSprite.fillSprite(TFT_BLACK);
    signalStrengthSprite.setFont(&fonts::Orbitron_Light_24);
    signalStrengthSprite.setTextSize(1);
    signalStrengthSprite.setTextColor(SIGNAL_STRENGTH_COLOR);
    
    char buffer[12];
    // Format S-meter display based on value
    if (sMeter <= 9) {
      // S0 through S9
      snprintf(buffer, sizeof(buffer), "S%d", sMeter);
    } else {
      // S9+10dB through S9+60dB
      const char* plusDB[] = {"+10", "+20", "+40", "+60"};
      snprintf(buffer, sizeof(buffer), "S9%s", plusDB[sMeter - 10]);
    }
    signalStrengthSprite.drawString(buffer, 0, 0);
    
    // Draw signal strength bars (14 bars total: S0-S9 + 4 over-S9)
    // S0-S9 use green, over-S9 use red
    for (int i = 0; i < 14; i++) {
      int xPos = 100+ (i * 7); // Was 60 without pluses
      
      if (sMeter > i) {
        // Filled bar
        uint16_t color = (i < 9) ? TFT_GREEN : TFT_RED;  // Green for S0-S9, red for over-S9
        signalStrengthSprite.fillRect(xPos, 10, 5, 14, color);
      } else {
        // Empty bar outline
        signalStrengthSprite.drawRect(xPos, 10, 5, 14, TFT_LIGHTGREY);
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
  
  // If in BFO mode on SW band (and in SSB mode), don't draw triangle (BFO sprite background shows mode instead)
  if (band.type == BAND_TYPE_SW && 
      (radioState.currentMode == MODE_LSB || radioState.currentMode == MODE_USB) && 
      radioState.bfoMode) {
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
  sprite.fillSprite(bgColor);
  sprite.drawRect(0, 0, width, height, borderColor);
  
  sprite.setTextColor(COLOR_WHITE);
  sprite.setTextDatum(MC_DATUM);
  sprite.setFont(&fonts::Orbitron_Light_24);
  sprite.drawString(text, width / 2, height / 2);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void resetCommandMode(bool* flagToPreserve, bool preserveValue,
                     void (*displayCallback)()) {
  commandState.band = false;
  commandState.agc = false;
  commandState.bandwidth = false;
  commandState.step = false;
  
  if (flagToPreserve != nullptr) {
    *flagToPreserve = preserveValue;
  }
  
  if (displayCallback != nullptr) {
    displayCallback();
  }
  
  // Update active command
  if (commandState.band) commandState.active = CMD_BAND;
  else if (commandState.agc) commandState.active = CMD_AGC;
  else if (commandState.bandwidth) commandState.active = CMD_BANDWIDTH;
  else if (commandState.step) commandState.active = CMD_STEP;
  else commandState.active = CMD_NONE;
  
  lastRSSIUpdate = millis();
}

const char* getModeDescription(RadioMode mode) {
  if (mode < MODE_DESCRIPTION_COUNT) {
    return MODE_DESCRIPTIONS[mode];
  }
  return "";
}

bool shouldUpdateRSSI() {
  return (millis() - lastRSSIUpdate) > RSSI_UPDATE_INTERVAL_MS;
}

void updateRSSIIfNeeded() {
  if (shouldUpdateRSSI()) {
    showRSSI();
    lastRSSIUpdate = millis();
  }
}

void handleCommandTimeout() {
  if (commandState.active != CMD_NONE) {
    if ((millis() - lastCommandTime) > COMMAND_TIMEOUT_MS) {
      resetCommandMode(nullptr, false, nullptr);
    }
  }
}

uint8_t calculateSMeterReading(uint8_t rssi) {
  // Returns S-meter value: 0-9 for S0-S9, 10-13 for S9+10 through S9+60
  // Return values: 0=S0, 1=S1, ..., 9=S9, 10=S9+10, 11=S9+20, 12=S9+40, 13=S9+60
  
  if (rssi < RSSI_S1_THRESHOLD) return 0;      // S0
  else if (rssi < RSSI_S2_THRESHOLD) return 1; // S1
  else if (rssi < RSSI_S3_THRESHOLD) return 2; // S2
  else if (rssi < RSSI_S4_THRESHOLD) return 3; // S3
  else if (rssi < RSSI_S5_THRESHOLD) return 4; // S4
  else if (rssi < RSSI_S6_THRESHOLD) return 5; // S5
  else if (rssi < RSSI_S7_THRESHOLD) return 6; // S6
  else if (rssi < RSSI_S8_THRESHOLD) return 7; // S7
  else if (rssi < RSSI_S9_THRESHOLD) return 8; // S8
  else if (rssi < RSSI_S9_PLUS_10_THRESHOLD) return 9;  // S9
  else if (rssi < RSSI_S9_PLUS_20_THRESHOLD) return 10; // S9+10
  else if (rssi < RSSI_S9_PLUS_40_THRESHOLD) return 11; // S9+20
  else if (rssi < RSSI_S9_PLUS_60_THRESHOLD) return 12; // S9+40
  else return 13; // S9+60 or higher
}
