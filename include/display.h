#pragma once
#include <LovyanGFX.hpp>

// Configuration verified in this model's display example and by the LCD output
// observed on the connected unit:
// https://github.com/pay191/DIY_Malls-JC2432W328C_Tests/blob/main/BigYellowDisplay_2_ScreenTest/BigYellowDisplay_2_ScreenTest.ino
class ClockDisplay : public lgfx::LGFX_Device {
  lgfx::Bus_SPI bus;
  lgfx::Panel_ST7789 panel;
public:
  static constexpr int Backlight = 27;
  ClockDisplay() {
    auto spi = bus.config();
    spi.spi_host = VSPI_HOST;
    spi.spi_mode = 0;
    spi.freq_write = 27000000;
    spi.freq_read = 16000000;
    spi.spi_3wire = false;
    spi.use_lock = true;
    spi.dma_channel = 1;
    spi.pin_sclk = 14;
    spi.pin_mosi = 13;
    spi.pin_miso = -1;
    spi.pin_dc = 2;
    bus.config(spi);
    panel.setBus(&bus);
    auto lcd = panel.config();
    lcd.pin_cs = 15;
    lcd.pin_rst = -1;
    lcd.pin_busy = -1;
    lcd.panel_width = 240;
    lcd.panel_height = 320;
    lcd.readable = false;
    lcd.invert = false;
    lcd.rgb_order = false;
    lcd.bus_shared = true;
    panel.config(lcd);
    setPanel(&panel);
  }
};
