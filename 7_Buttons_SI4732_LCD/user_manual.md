

  Connections
  ===========
  | Device name               | Device Pin / Description      |  ESP32 Pin    |
  | ----------------          | ----------------------------- | ------------  |
  |     ST7789 Display        |                               |               |
  |                           | VCC                           |     3.3V      |
  |                           | GND                           |      GND      |
  |                           | SCK (SCL)                     |      18       |
  |                           | SDA (MOSI)                    |      23       |
  |                           | DC                            |      27       |
  |                           | CS                            |      26       |
  |                           | RST (RESET)                   |      25       |
  |                           | BLK (Backlight)               |     3.3V      |
  |     Si4732                |                               |               |
  |                           | (*3) RESET                    |      33       |
  |                           | (*3) SDIO                     |      21       |
  |                           | (*3) SCLK                     |      22       |
  |                           | (*4) SEN                      |     GND       |
  |     Buttons               |                               |               |
  |                           | (*1)Switch MODE (AM/LSB/AM)   |       4       |
  |                           | (*1)Banddwith                 |       5       |
  |                           | (*1)BAND                      |      32       |
  |                           | (*2)SEEK                      |      15       |
  |                           | (*1)AGC/Attenuation           |      13       |
  |                           | (*1)STEP                      |      14       |
  |                           | VFO/BFO Switch (Encoder)      |      19       |
  |    Encoder                |                               |               |
  |                           | A                             |      17       |
  |                           | B                             |      16       |

Notes
=====  
  Prototype documentation: https://pu2clr.github.io/SI4735/
  PU2CLR Si47XX API documentation: https://pu2clr.github.io/SI4735/extras/apidoc/html/

  By PU2CLR, Ricardo  Feb  2022.
  Modified for ESP32 Dec 2024.

  Modified from Arduino Pro Mini version to work with ESP32.
    - https://github.com/pu2clr/SI4735/tree/master/examples/SI47XX_09_NOKIA_5110

  Claude conversation: https://claude.ai/chat/e8c6ba09-57f1-4a5c-810a-96dfda048d8f

  Si4732 Antenna design: https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN383.pdf

  ESP32 Pinout Reference: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

  Discussion about BFO https://groups.io/g/si47xx/topic/minimum_tuning_step/82343241 
    The main VFO tunes in 1khz min steps. In SSB mode however, there's a second IF at 45khz and the conversion oscillator for that (which people call a BFO) is tunable about +/_ 16 kHz in 1 hz steps.
