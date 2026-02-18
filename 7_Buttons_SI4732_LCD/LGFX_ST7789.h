// ST7789 240 x 320 Display Configuration for LovyanGFX
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      // Use SPI3_HOST for newer ESP32 variants (ESP32-S3, ESP32-C3, etc.)
      // For classic ESP32, both VSPI_HOST and SPI3_HOST work
      #ifdef VSPI_HOST
        cfg.spi_host = VSPI_HOST;
      #else
        cfg.spi_host = SPI3_HOST;
      #endif
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = 18;  // SCK
      cfg.pin_mosi = 23;  // MOSI (SDA)
      cfg.pin_miso = -1;  // Not used
      cfg.pin_dc = 27;    // DC (Data/Command)
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 26;       // CS
      cfg.pin_rst = 25;      // RESET
      cfg.pin_busy = -1;     // Not used
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;     // 2" screen = true, 2.8" screen = false
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

// Color picker https://rgbcolorpicker.com/565

// Display dimensions (landscape mode)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Color definitions
#define COLOR_BACKGROUND 0x0000  // Black
#define COLOR_BFO 0x108A
#define COLOR_BFO_SELECTED COLOR_DARKSLATEBLUE

// Color definitions
// From https://forum.4dsystems.com.au/node/257
#define COLOR_ALICEBLUE 0xF7DF
#define COLOR_ANTIQUEWHITE 0xFF5A
#define COLOR_AQUA 0x07FF
#define COLOR_AQUAMARINE 0x7FFA
#define COLOR_AZURE 0xF7FF
#define COLOR_BEIGE 0xF7BB
#define COLOR_BISQUE 0xFF38
#define COLOR_BLACK 0x0000
#define COLOR_BLANCHEDALMOND 0xFF59
#define COLOR_BLUE 0x001F
#define COLOR_BLUEVIOLET 0x895C
#define COLOR_BROWN 0xA145
#define COLOR_BURLYWOOD 0xDDD0
#define COLOR_CADETBLUE 0x5CF4
#define COLOR_CHARTREUSE 0x7FE0
#define COLOR_CHOCOLATE 0xD343
#define COLOR_CORAL 0xFBEA
#define COLOR_CORNFLOWERBLUE 0x64BD
#define COLOR_CORNSILK 0xFFDB
#define COLOR_CRIMSON 0xD8A7
#define COLOR_CYAN 0x07FF
#define COLOR_DARKBLUE 0x0011
#define COLOR_DARKCYAN 0x0451
#define COLOR_DARKGOLDENROD 0xBC21
#define COLOR_DARKGRAY 0xAD55
#define COLOR_DARKGREEN 0x0320
#define COLOR_DARKKHAKI 0xBDAD
#define COLOR_DARKMAGENTA 0x8811
#define COLOR_DARKOLIVEGREEN 0x5345
#define COLOR_DARKORANGE 0xFC60
#define COLOR_DARKORCHID 0x9999
#define COLOR_DARKRED 0x8800
#define COLOR_DARKSALMON 0xECAF
#define COLOR_DARKSEAGREEN 0x8DF1
#define COLOR_DARKSLATEBLUE 0x49F1
#define COLOR_DARKSLATEGRAY 0x2A69
#define COLOR_DARKTURQUOISE 0x067A
#define COLOR_DARKVIOLET 0x901A
#define COLOR_DEEPPINK 0xF8B2
#define COLOR_DEEPSKYBLUE 0x05FF
#define COLOR_DIMGRAY 0x6B4D
#define COLOR_DODGERBLUE 0x1C9F
#define COLOR_FIREBRICK 0xB104
#define COLOR_FLORALWHITE 0xFFDE
#define COLOR_FORESTGREEN 0x2444
#define COLOR_FUCHSIA 0xF81F
#define COLOR_GAINSBORO 0xDEFB
#define COLOR_GHOSTWHITE 0xFFDF
#define COLOR_GOLD 0xFEA0
#define COLOR_GOLDENROD 0xDD24
#define COLOR_GRAY 0x8410
#define COLOR_GREEN 0x0400
#define COLOR_GREENYELLOW 0xAFE5
#define COLOR_HONEYDEW 0xF7FE
#define COLOR_HOTPINK 0xFB56
#define COLOR_INDIANRED 0xCAEB
#define COLOR_INDIGO 0x4810
#define COLOR_IVORY 0xFFFE
#define COLOR_KHAKI 0xF731
#define COLOR_LAVENDER 0xE73F
#define COLOR_LAVENDERBLUSH 0xFF9E
#define COLOR_LAWNGREEN 0x7FE0
#define COLOR_LEMONCHIFFON 0xFFD9
#define COLOR_LIGHTBLUE 0xAEDC
#define COLOR_LIGHTCORAL 0xF410
#define COLOR_LIGHTCYAN 0xE7FF
#define COLOR_LIGHTGOLDENRODYELLOW 0xFFDA
#define COLOR_LIGHTGREEN 0x9772
#define COLOR_LIGHTGREY 0xD69A
#define COLOR_LIGHTPINK 0xFDB8
#define COLOR_LIGHTSALMON 0xFD0F
#define COLOR_LIGHTSEAGREEN 0x2595
#define COLOR_LIGHTSKYBLUE 0x867F
#define COLOR_LIGHTSLATEGRAY 0x7453
#define COLOR_LIGHTSTEELBLUE 0xB63B
#define COLOR_LIGHTYELLOW 0xFFFC
#define COLOR_LIME 0x07E0
#define COLOR_LIMEGREEN 0x3666
#define COLOR_LINEN 0xFF9C
#define COLOR_MAGENTA 0xF81F
#define COLOR_MAROON 0x8000
#define COLOR_MEDIUMAQUAMARINE 0x6675
#define COLOR_MEDIUMBLUE 0x0019
#define COLOR_MEDIUMORCHID 0xBABA
#define COLOR_MEDIUMPURPLE 0x939B
#define COLOR_MEDIUMSEAGREEN 0x3D8E
#define COLOR_MEDIUMSLATEBLUE 0x7B5D
#define COLOR_MEDIUMSPRINGGREEN 0x07D3
#define COLOR_MEDIUMTURQUOISE 0x4E99
#define COLOR_MEDIUMVIOLETRED 0xC0B0
#define COLOR_MIDNIGHTBLUE 0x18CE
#define COLOR_MINTCREAM 0xF7FF
#define COLOR_MISTYROSE 0xFF3C
#define COLOR_MOCCASIN 0xFF36
#define COLOR_NAVAJOWHITE 0xFEF5
#define COLOR_NAVY 0x0010
#define COLOR_OLDLACE 0xFFBC
#define COLOR_OLIVE 0x8400
#define COLOR_OLIVEDRAB 0x6C64
#define COLOR_ORANGE 0xFD20
#define COLOR_ORANGERED 0xFA20
#define COLOR_ORCHID 0xDB9A
#define COLOR_PALEGOLDENROD 0xEF55
#define COLOR_PALEGREEN 0x9FD3
#define COLOR_PALETURQUOISE 0xAF7D
#define COLOR_PALEVIOLETRED 0xDB92
#define COLOR_PAPAYAWHIP 0xFF7A
#define COLOR_PEACHPUFF 0xFED7
#define COLOR_PERU 0xCC27
#define COLOR_PINK 0xFE19
#define COLOR_PLUM 0xDD1B
#define COLOR_POWDERBLUE 0xB71C
#define COLOR_PURPLE 0x8010
#define COLOR_RED 0xF800
#define COLOR_ROSYBROWN 0xBC71
#define COLOR_ROYALBLUE 0x435C
#define COLOR_SADDLEBROWN 0x8A22
#define COLOR_SALMON 0xFC0E
#define COLOR_SANDYBROWN 0xF52C
#define COLOR_SEAGREEN 0x2C4A
#define COLOR_SEASHELL 0xFFBD
#define COLOR_SIENNA 0xA285
#define COLOR_SILVER 0xC618
#define COLOR_SKYBLUE 0x867D
#define COLOR_SLATEBLUE 0x6AD9
#define COLOR_SLATEGRAY 0x7412
#define COLOR_SNOW 0xFFDF
#define COLOR_SPRINGGREEN 0x07EF
#define COLOR_STEELBLUE 0x4416
#define COLOR_TAN 0xD5B1
#define COLOR_TEAL 0x0410
#define COLOR_THISTLE 0xDDFB
#define COLOR_TOMATO 0xFB08
#define COLOR_TURQUOISE 0x471A
#define COLOR_VIOLET 0xEC1D
#define COLOR_WHEAT 0xF6F6
#define COLOR_WHITE 0xFFFF
#define COLOR_WHITESMOKE 0xF7BE
#define COLOR_YELLOW 0xFFE0
#define COLOR_YELLOWGREEN 0x9E66
