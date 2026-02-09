# SI4732 Multi-Band Radio Controller

A software-defined radio receiver for **FM, AM, Shortwave (SSB/LSB/USB), Medium Wave, and Long Wave** bands, built with ESP32 and the Si4732 radio IC.

This version was based on ATmega328 with Nokia 5110 version by PU2CLR, Ricardo Feb 2022 https://github.com/pu2clr/SI4735/tree/master/examples/. It has been heavily modified with the assistance of Claude AI. It is provided under the MIT License

⚠️ Experimental BFO in AM Mode: On shortwave bands, the SSB patch remains loaded even when in AM mode, keeping the BFO circuit active. While the Si4732 BFO is designed specifically for SSB demodulation, having it enabled in AM mode allows Hz-level tuning precision—though this may introduce minor audio artifacts or phase shifts that wouldn't occur in standard AM reception.

---

## 📻 Features

### Radio Capabilities
- **FM Radio**: 64-108 MHz with stereo/mono detection
- **Shortwave**: 9 ham radio bands (160m, 80m, 40m, 30m, 20m, 17m, 15m, 12m, 10m)
- **CB Radio**: 11m Citizens Band (Australian, 26.9-27.5 MHz)
- **Medium Wave (MW)**: 150-1720 kHz AM broadcast band
- **Long Wave (LW)**: 150-810 kHz including 2200m ham band
- **Multi-Mode Demodulation**: AM, LSB (Lower Sideband), USB (Upper Sideband)
- **Signal Strength**: S-meter with visual bar graph (S4-S9+)
- **Adjustable Bandwidth**: 
- **AGC Control**: Automatic Gain Control with manual attenuation
- **Variable Tuning Steps**: Multiple step sizes per band
- AM coverage is 150 kHz to 30 MHz. Additional bands in this range can be easily added

### Controls:
- **Rotary Encoder**: Tune frequency (or adjust BFO when BFO mode is active)
- **MODE Button**: Cycle through AM → LSB → USB → AM
- **BANDWIDTH Button**: Adjust filter bandwidth
- **BAND Button**: Cycle through bands (FM, MW, SW1-SW8)
- **SEEK Button**: Auto-scan for stations
- **AGC Button**: Toggle AGC and adjust attenuation
- **STEP Button**: Change frequency step size
- **BFO Button**: Toggle BFO control mode (SSB only)

---

## Hardware Requirements

### Core Components

- **ESP32 Development Module** - Main microcontroller
- **SI4735 Radio IC** - Radio receiver chip and support components
- **ST7789 TFT Display** (240x320 pixels) - Visual interface
- **Rotary Encoder** - Main tuning control
- **7 Push Buttons** - Function controls

### Pin Connections

#### ESP32 to SI4735
- SDIO: GPIO 21
- SCLK: GPIO 22
- SEN: GND
- Reset: GPIO 33

#### ESP32 to ST7789 Display (SPI)
- SCK (Clock): GPIO 18
- MOSI (SDA): GPIO 23
- CS (Chip Select): GPIO 26
- DC (Data/Command): GPIO 27
- RESET: GPIO 25
- Backlight: Connect to 3.3V

#### Rotary Encoder
- Pin A: GPIO 17 (TX2)
- Pin B: GPIO 16 (RX2)
- Common: GND (with internal pull-ups enabled)

#### Control Buttons
- Mode Button: GPIO 4
- Bandwidth Button: GPIO 5
- Band Button: GPIO 32
- Seek Button: GPIO 15
- AGC Button: GPIO 13
- Step Button: GPIO 14
- BFO Toggle Button: GPIO 19

> **Note**: All buttons connect between the GPIO pin and GND. Internal pullups are enabled in software.

### Power Supply
- 3.3V or 5V 
- Ensure adequate current capacity for ESP32 and display (500mA+ recommended)

## Features

### Band Coverage

**FM Band (VHF)**
- Range: 64.0 - 108.0 MHz
- Default: 103.9 MHz
- Mono/Stereo reception with indicator

**Shortwave Bands (Amateur Radio Focused)**
- SW1 (160M): 1.843 MHz - 160m amateur band
- SW2 (80M): 3.776 MHz - 80m amateur band
- SW3 (40M): 7.060 MHz - 40m amateur band
- SW4 (30M): 10.120 MHz - 30m amateur band
- SW5 (20M): 14.112 MHz - 20m amateur band
- SW6 (17M): 18.110 MHz - 17m amateur band
- SW7 (15M): 21.150 MHz - 15m amateur band
- SW8 (12M): 24.930 MHz - 12m amateur band
- SW9 (10M): 28.300 MHz - 10m amateur band

**Citizens Band (CB)**
- CB (11M): 26.9 - 27.5 MHz
- Default: 27.085 MHz (Australian CB)

**Medium Wave (MW)**
- Range: 150 - 1720 kHz
- Default: 810 kHz
- AM broadcast band

**Long Wave (LW)**
- Range: 150 - 810 kHz
- Default: 401 kHz
- LW broadcast band

**2200M Amateur Band**
- Range: 150 - 500 kHz
- Default: 135 kHz

### Demodulation Modes

- **FM** - Frequency Modulation (FM broadcast)
- **AM** - Amplitude Modulation (MW/SW)
- **LSB** - Lower Sideband (SSB for SW amateur bands)
- **USB** - Upper Sideband (SSB for SW amateur bands)

### Advanced Tuning System

#### VFO (Variable Frequency Oscillator)
- Primary frequency tuning
- Multiple step sizes for precise control
- Frequency memory for all bands (remembers last frequency when switching)

#### BFO (Beat Frequency Oscillator) - Separate Control
- **Independent ±8000 Hz adjustment range**
- Enables Hz-level precision tuning on SSB/AM modes
- Two operational modes:
  - **VFO Mode**: Encoder adjusts main frequency (background: black)
  - **BFO Mode**: Encoder adjusts BFO offset (background: slate blue)
- Visual indicator shows current offset with +/- sign
- Press BFO Toggle button to switch between VFO/BFO adjustment modes
- Double-press BFO Toggle to enable/disable BFO

#### Step Sizes
- **FM**: 1 MHz → 100 kHz → 10 kHz
- **MW/LW**: 100 kHz → 10 kHz → 1 kHz
- **SW**: 10 MHz → 1 MHz → 100 kHz → 10 kHz → 1 kHz
- **Plus Hz tuning** via BFO when enabled

### Bandwidth Control

**SSB Modes (LSB/USB) - 6 Settings:**
- 0.5 kHz - Extremely narrow (CW/weak signal)
- 1.0 kHz - Narrow CW/data
- 1.2 kHz - Narrow voice
- 2.2 kHz - Standard SSB
- **3.0 kHz** - Default (good voice quality)
- 4.0 kHz - Wide (broadcast quality)

**AM Mode - 7 Settings:**
- 1.0 kHz - Very narrow
- 1.8 kHz - Narrow
- 2.0 kHz - Standard narrow
- 2.5 kHz - Medium
- **3.0 kHz** - Default (standard AM)
- 4.0 kHz - Wide
- 6.0 kHz - Maximum (broadcast quality)

### AGC (Automatic Gain Control)

- **AGC ON** (Index 0) - Automatic signal level management
- **Manual Attenuation** (Index 1-37) - Manual RF gain reduction
  - Adjustable from 0 to maximum attenuation
  - Useful for strong signal handling and noise reduction
  - Index > 1 indicates manual mode is active

### Signal Quality Display

- **S-Meter**: Visual signal strength indicator (S4 to S9+)
- **RSSI Monitoring**: Updates every 900ms
- **Bar Graph**: Real-time signal strength visualization
- **FM Stereo Indicator**: Shows Mono/Stereo reception status

## User Interface

### Display Layout

The 320x240 landscape display shows:

**Top Section:**
- Signal Strength (S-meter with bar graph) - Top left
- Frequency Unit (MHz/kHz) - Top right

**Center Section:**
- Large frequency display
- BFO offset display (when enabled)
  - Shows +/- sign and offset value
  - Background color indicates active mode (VFO/BFO)
- Step size indicator (red triangle below frequency)

**Bottom Section (Status Labels):**
- Band Name (e.g., "SW3 40M") - Blue background
- Mode (FM Stereo/Mono, LSB, USB, AM) - Green background
- AGC Status (AGC: ON or ATT: xx) - Maroon background
- Bandwidth (BW: x.x kHz) - Violet background

### Button Functions

#### 1. Mode Button (GPIO 4)
- Press: Cycle through demodulation modes
  - FM → LSB → USB → AM → FM
- SSB modes (LSB/USB) automatically load SSB patch on first use

#### 2. Bandwidth Button (GPIO 5)
- Press: Enter bandwidth adjustment mode
- Rotate encoder: Cycle through available bandwidths
- Auto-exits after inactivity
- Available for AM and SSB modes only

#### 3. Band Button (GPIO 32)
- Press: Enter band selection mode
- Rotate encoder: Navigate through all available bands
- Auto-exits after inactivity

#### 4. Seek Button (GPIO 15)
- Press: Initiate automatic station seeking
- Direction: Based on last encoder rotation direction
- Stops when signal detected or band limit reached

#### 5. AGC Button (GPIO 13)
- Press: Enter AGC adjustment mode
- Rotate encoder: Adjust AGC/attenuation
  - Counter-clockwise: Enable AGC (automatic)
  - Clockwise: Increase manual attenuation
- Auto-exits after inactivity

#### 6. Step Button (GPIO 14)
- Press: Enter step size selection mode
- Rotate encoder: Cycle through available step sizes
- Red triangle indicator shows current step position
- Auto-exits after inactivity

#### BFO Toggle Modes
- **Single Press**: Toggle between **VFO mode** (normal tuning) and **BFO mode** (Hz tuning)
  - If BFO was disabled, single press re-enables it with the saved offset value
  - If BFO is enabled, single press switches between VFO and BFO tuning modes
  - BFO offset is preserved when switching modes
- **Double Press**: Disable BFO and reset offset to 0 Hz
  - Saves current BFO value for potential re-enabling
  - Returns to normal VFO tuning

### Rotary Encoder Operation

**In Normal Mode:**
- Adjusts frequency based on current step size
- Clockwise: Increase frequency
- Counter-clockwise: Decrease frequency
- Sets direction for seek function

**In BFO Mode:**
- Adjusts BFO offset in ±10 Hz steps
- Provides Hz-level tuning precision
- Visual feedback via BFO display background color

**In Command Modes:**
- Used to adjust the active parameter:
  - Band selection
  - Step size
  - Bandwidth
  - AGC/Attenuation
- Rate-limited to 150ms between adjustments

## Technical Details

### Timing Parameters

- Button debounce: 50ms
- Double-press window: 400ms
- Command mode timeout: 1.5 seconds
- RSSI update interval: 900ms
- Encoder rate limit: 150ms (in command modes)

### Display Technology

- LovyanGFX library for high-performance graphics
- Sprite-based rendering for smooth updates
- Custom fonts for optimal readability
- Minimal redraw strategy to reduce flicker

### Radio Configuration

- Default volume: 55
- BFO step size: 10 Hz
- BFO threshold: ±1000 Hz (triggers mode switching behavior)
- SSB patch: Compressed patch loaded on-demand

## Compilation Requirements

### Required Libraries

1. **SI4735** - Silicon Labs radio chip driver by pu2clr https://github.com/pu2clr/SI4735
2. **LovyanGFX** - Graphics library for ESP32 by lovyan03 https://github.com/lovyan03/LovyanGFX
3. **Rotary** - Rotary encoder library (included with sketch)
4. **SPI** - ESP32 SPI library (built-in)
5. **patch_ssb_compressed** - SSB demodulation patch

### Arduino IDE Settings

- Board: ESP32 Dev Module
- Upload Speed: 115200
- Flash Frequency: 80 MHz
- Flash Mode: QIO
- Partition Scheme: Default 4MB with spiffs

## 🐛 Troubleshooting

### Display Issues
- **Blank screen**: Check SPI connections and power
- **Inverted colors**: Adjust `cfg.invert` in LGFX_ST7789.h (line 61)
- **Wrong orientation**: Display is configured for landscape mode

### Radio Won't Initialize
- **Check Si4735 reset pin**: (GPIO 33)
- **Verify I2C pullup resistors**: (4.7kΩ recommended)
- **Monitor serial output**: (115200 baud) for error messages

### Encoder Issues
- **Reversed direction**: Swap ENCODER_PIN_A and ENCODER_PIN_B
- **Erratic behavior**: Check for loose connections or add hardware debouncing

### Button Issues
- **Verify buttons**: connect GPIO to GND (active LOW)
- **Not responding**: Check pull-up configuration (active LOW)
- **Multiple triggers**: Increase DEBOUNCE_DELAY_MS in config.h

## 📝 Configuration

### Customizing Default Settings

Edit these values in **config.h**:

```cpp
#define DEFAULT_VOLUME 55              // Initial volume (0-63)
#define DEFAULT_BAND_INDEX 3           // Startup band (3 = SW3 40m)
#define BFO_STEP_SIZE 10               // BFO step in Hz
#define COMMAND_TIMEOUT_MS 1500        // Auto-exit time for modes
```

Edit these values in **constants.h**:

```cpp
DEFAULT_SSB_BANDWIDTH_INDEX = 4        // 3.0 kHz for SSB
DEFAULT_AM_BANDWIDTH_INDEX = 4         // 3.0 kHz for AM
```

### Adding/Modifying Bands

Edit `constants.h`, BAND_TABLE array:
```cpp
{"Name", BAND_TYPE_XX, minFreq, maxFreq, defaultFreq, defaultStep}
```

### Display Customization

Edit `config.h`:
- Display positions and sizes
- Color schemes

---

## 📚 Additional Resources

- **Si4735 Library**: [github.com/pu2clr/SI4735](https://github.com/pu2clr/SI4735)
- **Si4732 Datasheet**: [Skyworks](https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/data-shorts/Si4732-A10-short.pdf)
- **Si4735 Datasheet**: [Skyworks](https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/data-sheets/Si4730-31-34-35-D60.pdf)
- **LovyanGFX Documentation**: [github.com/lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- **ESP32 Pinout Reference**: [randomnerdtutorials.com/esp32-pinout-reference-gpios/](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)

## 📄 License and 🙏 Credits

Licenced under the MIT License

Based on ATmega328 with Nokia 5110 example version by PU2CLR, Ricardo  Feb  2022 [Github](https://github.com/pu2clr/SI4735/tree/master/examples/SI47XX_09_NOKIA_5110)

- **Author**: This version was modified by Claude AI and [garrysblog.com](https://garrysblog.com/)  
- **Display Library**: LovyanGFX  
- **Radio Library**: SI4735 by Ricardo Lima Caratti (PU2CLR)
- **Encoder Library**: Ben Buxton bb
