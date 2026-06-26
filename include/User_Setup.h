// TFT_eSPI pin config for ST7735 128x160 V1.1 on ESP32
// SCK=18, MOSI=23, CS=5, DC=2, RST=4

#define USER_SETUP_INFO "User_Setup"

#define ST7735_DRIVER
#define ST7735_GREENTAB

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

#define SPI_FREQUENCY  27000000
