#pragma once
// Display driver setup for the 3.5" 480x320 SPI screen on an ESP32-S3 (LovyanGFX).
// Only 8 wires are needed: VCC, GND, CS=10, RESET=8, DC=9, SDI(MOSI)=11, SCK=12, LED=7 (or LED to 3V3).
// SDO and the T_* touch pins are not used.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define PANEL_ILI9488  // the chip printed on the back of the screen; comment this out for ST7796S

class LGFX : public lgfx::LGFX_Device {
#ifdef PANEL_ILI9488
  lgfx::Panel_ILI9488 panel;
#else
  lgfx::Panel_ST7796 panel;
#endif
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

 public:
  LGFX() {
    {
      auto c = bus.config();
      c.spi_host = SPI2_HOST;
      c.spi_mode = 0;
      c.freq_write = 40000000;  // drop to 27000000 if the image shows noise
      c.freq_read = 16000000;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 12;
      c.pin_mosi = 11;
      c.pin_miso = -1;
      c.pin_dc = 9;
      bus.config(c);
      panel.setBus(&bus);
    }
    {
      auto c = panel.config();
      c.pin_cs = 10;
      c.pin_rst = 8;
      c.pin_busy = -1;
      c.panel_width = 320;
      c.panel_height = 480;
      c.readable = false;
      c.invert = false;     // set true if colors look inverted (some ST7796S boards)
      c.rgb_order = false;  // set true if red and blue are swapped
      c.bus_shared = false;
      panel.config(c);
    }
    {
      auto c = light.config();
      c.pin_bl = 7;
      c.invert = false;
      c.freq = 44100;
      c.pwm_channel = 7;
      light.config(c);
      panel.setLight(&light);
    }
    setPanel(&panel);
  }
};
