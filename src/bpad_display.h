#pragma once
// =============================================================================
//  bpad_display.h - Native esp_lcd Display Driver for bpad hardware
// =============================================================================
//  Provides native hardware-accelerated esp_lcd drivers:
//  - bpad 185+: ST77916 QSPI 360x360 via esp_lcd_st77916
// =============================================================================

#include <Arduino.h>
#include "bpad_config.h"

#if defined(ESP_PLATFORM)
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#endif

class bpadDisplay {
public:
  explicit bpadDisplay(const bpadDisplayConfig& config = bpadDisplayConfig())
    : _cfg(config) {}

  void select(const bpadDisplayConfig& config) {
    _cfg = config;
  }

  bool begin() {
#if defined(ESP_PLATFORM)
    if (_cfg.backend == bpadDisplayBackend::ST77916_QSPI) {
      // 1. Release JTAG multiplexing so GPIO 39, 40, 41, 42 work as GPIOs
      PIN_FUNC_SELECT(IO_MUX_GPIO39_REG, PIN_FUNC_GPIO);
      PIN_FUNC_SELECT(IO_MUX_GPIO40_REG, PIN_FUNC_GPIO);
      PIN_FUNC_SELECT(IO_MUX_GPIO41_REG, PIN_FUNC_GPIO);
      PIN_FUNC_SELECT(IO_MUX_GPIO42_REG, PIN_FUNC_GPIO);

      // 2. Hardware DC Mode pin (GPIO 40) - MUST BE LOW for QSPI mode
      if (_cfg.dc >= 0) {
        gpio_config_t pin_dc_conf = {
          .pin_bit_mask = (1ULL << _cfg.dc),
          .mode = GPIO_MODE_OUTPUT,
          .pull_up_en = GPIO_PULLUP_DISABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&pin_dc_conf);
        gpio_set_level((gpio_num_t)_cfg.dc, 0);
      }

      // 3. Reset & drive CS pin (GPIO 14) and Backlight pin (GPIO 39)
      if (_cfg.cs >= 0) {
        gpio_reset_pin((gpio_num_t)_cfg.cs);
        gpio_set_drive_capability((gpio_num_t)_cfg.cs, GPIO_DRIVE_CAP_1);
      }

      if (_cfg.bl >= 0) {
        gpio_reset_pin((gpio_num_t)_cfg.bl);
        gpio_set_drive_capability((gpio_num_t)_cfg.bl, GPIO_DRIVE_CAP_1);
        ledcAttach(_cfg.bl, _cfg.backlightFreq, _cfg.backlightResolution);
        ledcWrite(_cfg.bl, 0);
      }

      // 4. Initialize QSPI SPI2_HOST bus
      spi_bus_config_t buscfg = {};
      buscfg.data0_io_num = _cfg.d0;
      buscfg.data1_io_num = _cfg.d1;
      buscfg.sclk_io_num  = _cfg.sclk;
      buscfg.data2_io_num = _cfg.d2;
      buscfg.data3_io_num = _cfg.d3;
      buscfg.max_transfer_sz = _cfg.width * 90 * sizeof(uint16_t);
      buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
      esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[DISPLAY] spi_bus_initialize failed: %d\n", err);
        return false;
      }

      esp_lcd_panel_io_spi_config_t io_config = {};
      io_config.cs_gpio_num = (gpio_num_t)_cfg.cs;
      io_config.dc_gpio_num = GPIO_NUM_NC;
      io_config.spi_mode = 2;
      io_config.pclk_hz = 80 * 1000 * 1000;
      io_config.trans_queue_depth = 10;
      io_config.lcd_cmd_bits = 32;
      io_config.lcd_param_bits = 8;
      io_config.flags.quad_mode = true;

      st77916_vendor_config_t vendor_config = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .flags = { .use_qspi_interface = 1 }
      };

      if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &_io_handle) != ESP_OK) {
        Serial.println("[DISPLAY] new_panel_io_spi failed!");
        return false;
      }

      esp_lcd_panel_dev_config_t panel_config = {};
      panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
      panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
      panel_config.bits_per_pixel = 16;
      panel_config.reset_gpio_num = _cfg.rst;
      panel_config.vendor_config = &vendor_config;

      if (esp_lcd_new_panel_st77916(_io_handle, &panel_config, &_panel_handle) != ESP_OK) {
        Serial.println("[DISPLAY] new_panel_st77916 failed!");
        return false;
      }

      esp_lcd_panel_reset(_panel_handle);
      delay(50);
      esp_lcd_panel_init(_panel_handle);
      delay(100);
      esp_lcd_panel_disp_on_off(_panel_handle, true);
      esp_lcd_panel_invert_color(_panel_handle, true);

      _ready = true;
      setBrightness(220);
      Serial.println("[DISPLAY] ST77916 panel initialized successfully!");
      return true;
    }
#endif
    return false;
  }

  void setBrightness(uint8_t brightness) {
    if (_cfg.bl >= 0) {
      ledcWrite(_cfg.bl, brightness);
    }
  }

  void drawBitmap(int x_start, int y_start, int x_end, int y_end, const void* color_data) {
#if defined(ESP_PLATFORM)
    if (_panel_handle && _ready) {
      esp_lcd_panel_draw_bitmap(_panel_handle, x_start, y_start, x_end, y_end, color_data);
    }
#endif
  }

  void drawFrame(const void* color_data) {
#if defined(ESP_PLATFORM)
    if (_panel_handle && _ready) {
      const int chunkHeight = 90;
      const uint16_t* ptr = (const uint16_t*)color_data;
      for (int y = 0; y < _cfg.height; y += chunkHeight) {
        int h = (y + chunkHeight > _cfg.height) ? (_cfg.height - y) : chunkHeight;
        esp_lcd_panel_draw_bitmap(_panel_handle, 0, y, _cfg.width, y + h, ptr + (y * _cfg.width));
      }
    }
#endif
  }

  uint16_t width() const { return _cfg.width; }
  uint16_t height() const { return _cfg.height; }
  bool isReady() const { return _ready; }

#if defined(ESP_PLATFORM)
  esp_lcd_panel_handle_t getPanelHandle() const { return _panel_handle; }
  esp_lcd_panel_io_handle_t getIOHandle() const { return _io_handle; }
#endif

private:
  bpadDisplayConfig _cfg;
  bool _ready = false;
#if defined(ESP_PLATFORM)
  esp_lcd_panel_handle_t _panel_handle = NULL;
  esp_lcd_panel_io_handle_t _io_handle = NULL;
#endif
};
