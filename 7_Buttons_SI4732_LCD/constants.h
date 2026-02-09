/**
 * @file constants.h
 * @brief Constant data tables (band configurations, bandwidth settings)
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "types.h"

// ============================================================================
// SSB BANDWIDTH CONFIGURATIONS
// ============================================================================

const Bandwidth SSB_BANDWIDTHS[] = {
  {4, "0.5"},   // 0
  {5, "1.0"},   // 1
  {0, "1.2"},   // 2
  {1, "2.2"},   // 3
  {2, "3.0"},   // 4
  {3, "4.0"}    // 5
};
const uint8_t SSB_BANDWIDTH_COUNT = 6;
const uint8_t DEFAULT_SSB_BANDWIDTH_INDEX = 4;  // 3.0 kHz

// ============================================================================
// AM BANDWIDTH CONFIGURATIONS
// ============================================================================

const Bandwidth AM_BANDWIDTHS[] = {
  {4, "1.0"},   // 0
  {5, "1.8"},   // 1
  {3, "2.0"},   // 2
  {6, "2.5"},   // 3
  {2, "3.0"},   // 4
  {1, "4.0"},   // 5
  {0, "6.0"}    // 6
};
const uint8_t AM_BANDWIDTH_COUNT = 7;
const uint8_t DEFAULT_AM_BANDWIDTH_INDEX = 4;  // 3.0 kHz

// ============================================================================
// BAND CONFIGURATIONS
// ============================================================================

/**
 * Band configuration table
 * 
 * Format: {Name, Type, Min Freq, Max Freq, Default Freq, Default Step}
 * 
 * Frequencies:
 * - FM: in 10 kHz units (e.g., 10390 = 103.90 MHz)
 * - AM/SW/MW/LW: in kHz (e.g., 7100 = 7100 kHz = 7.1 MHz)
 */
const Band BAND_TABLE[] = {
  // FM Band (VHF)
  {"FM",  BAND_TYPE_FM, 6400, 10800, 10390, 1},
  
  /*// Shortwave bands (150 kHz to 30 MHz spectrum)
  {"SW1 40M", BAND_TYPE_SW, 150, 30000, 7100, 1},   // 7.1 MHz (40m band)
  {"SW2 31M", BAND_TYPE_SW, 150, 30000, 9600, 1},   // 9.6 MHz (31m band)
  {"SW3 25M", BAND_TYPE_SW, 150, 30000, 11940, 1},  // 11.94 MHz (25m band)
  {"SW4 22M", BAND_TYPE_SW, 150, 30000, 13600, 1},  // 13.6 MHz (22m band)
  {"SW5 20M", BAND_TYPE_SW, 150, 30000, 14200, 1},  // 14.2 MHz (20m ham band)
  {"SW6 19M", BAND_TYPE_SW, 150, 30000, 15300, 1},  // 15.3 MHz (19m band)
  {"SW7 16M", BAND_TYPE_SW, 150, 30000, 17600, 1},  // 17.6 MHz (16m band)
  {"SW8 15M", BAND_TYPE_SW, 150, 30000, 21100, 1},  // 21.1 MHz (15m ham band)
  {"SW9 10M", BAND_TYPE_SW, 150, 30000, 28400, 1},  // 28.4 MHz (10m ham band)*/

  // Shortwave bands (150 kHz to 30 MHz spectrum)
  {"SW1 160M", BAND_TYPE_SW, 150, 30000, 1843, 1},  // 1.843 MHz (160m band)
  {"SW2 80M", BAND_TYPE_SW, 150, 30000, 3776, 1},   // 3.776 MHz (80m band)
  {"SW3 40M", BAND_TYPE_SW, 150, 30000, 7060, 1},   // 7.060 MHz (40m band)
  {"SW4 30M", BAND_TYPE_SW, 150, 30000, 10120, 1},  // 10.120 MHz (30m band)
  {"SW5 20M", BAND_TYPE_SW, 150, 30000, 14112, 1},  // 14.112 MHz (20m ham band)
  {"SW6 17M", BAND_TYPE_SW, 150, 30000, 18110, 1},  // 18.11 MHz (17m band)
  {"SW7 15M", BAND_TYPE_SW, 150, 30000, 21150, 1},  // 21.150 MHz (15m ham band)
  {"SW8 12M", BAND_TYPE_SW, 150, 30000, 24930, 1},  // 24.930 MHz (12m ham band)
  {"SW9 10M", BAND_TYPE_SW, 150, 30000, 28300, 1},  // 28.3 MHz (10m ham band)

  // Australian CB Radio band
  {"CB 11M", BAND_TYPE_SW, 26900, 27500, 27085, 1}, // 28.4 MHz (11M CB band)

  // Medium Wave (AM broadcast)
  {"MW",  BAND_TYPE_MW, 150, 1720, 810, 1},

  // Long Wave band
  {"LW",  BAND_TYPE_MW, 150, 810, 401, 1},
  {"2200M", BAND_TYPE_LW, 150, 500, 135, 1}   // 135.7 kHz (2200m ham band)
};

const uint8_t BAND_COUNT = sizeof(BAND_TABLE) / sizeof(Band);
const uint8_t DEFAULT_BAND_INDEX = 3; //1;  // Start on 40M ham band

// ============================================================================
// AGC CONFIGURATION
// ============================================================================

const int8_t AGC_INDEX_MIN = 0;
const int8_t AGC_INDEX_MAX = 37;
const int8_t AGC_DISABLED_THRESHOLD = 1;  // Values > 1 indicate manual attenuation

#endif // CONSTANTS_H
