#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "state.h"
#include "axp192_custom.h"
#include <time.h>

#define LV_TICK_PERIOD_MS 1

static const char* TAG = "sensor_trap_demo_display"; 

static void lv_tick_task(void *arg) {
  (void) arg;

  lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void flush(struct _lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  esp_lcd_panel_draw_bitmap(*(esp_lcd_panel_handle_t *)disp_drv->user_data, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
  lv_disp_flush_ready(disp_drv);
}

static void set_px(lv_disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa) {
  uint8_t *byte = &buf[x + ((y >> 3) * buf_w)];

  if ((color.full == 0) && (LV_OPA_TRANSP != opa)) {
    *byte |= 1 << (y & 0x07);
  } else {
    *byte &= ~(1 << (y & 0x07));
  }
}

static void rounder(lv_disp_drv_t * disp_drv, lv_area_t * area) {
  area->y1 = area->y1 & ~0x07;
  area->y2 = (area->y2 & ~0x07) + 7;
}

SemaphoreHandle_t semaphore = NULL;
StaticSemaphore_t semaphore_buffer;

void gui_task(void *pvParameters) {
  (void) pvParameters;
 
  ESP_LOGI(TAG, "Configure panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = 0b00111100,
    .control_phase_bytes = 1,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
    .dc_bit_offset = 6,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(I2C_NUM_0, &io_config, &io_handle));
  
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
    .bits_per_pixel = 1,
    .reset_gpio_num = -1,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
  
  ESP_LOGI(TAG, "Configure panel");
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
  
  ESP_LOGI(TAG, "Init LVGL");
  lv_init();

  lv_color_t* buf = heap_caps_malloc(128 * 64 * 8, MALLOC_CAP_DMA);
  assert(buf != NULL);  
  
  static lv_disp_draw_buf_t disp_buf;
  
  lv_disp_draw_buf_init(&disp_buf, buf, NULL, 128 * 64);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 128;
  disp_drv.ver_res = 64;
  
  disp_drv.flush_cb = flush;
  disp_drv.rounder_cb = rounder;
  disp_drv.set_px_cb = set_px;  

  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = &panel_handle;

  lv_disp_drv_register(&disp_drv);

  const esp_timer_create_args_t periodic_timer_args = {
      .callback = &lv_tick_task,
      .name = "periodic_gui"
  };
  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
  
  lv_obj_t* scr, * label1;

  scr = lv_disp_get_scr_act(NULL);

  label1 = lv_label_create(scr);
  
  time_t last_message_ago = time(NULL) - previous_message_time;
  float battery_voltage;
  axp192_get_battery_voltage(&axp, &battery_voltage);
  lv_label_set_text_fmt(label1, "State: %s\nBattery: %.3fV\nLast message:\n  %lldm %llds ago", status_name[status], battery_voltage / 1000, last_message_ago / 60, last_message_ago % 60);

  lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 0, 0);  
  
  time_t stop_display_time = time(NULL) + 5;

  while (time(NULL) <= stop_display_time) {
    vTaskDelay(pdMS_TO_TICKS(10));
    lv_task_handler();
  }
  
  ESP_LOGI(TAG, "Turn things off");
  
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, false));
  free(buf);

  xSemaphoreGive(semaphore);
  vTaskDelete(NULL);
}

void gui_start() {
  semaphore = xSemaphoreCreateBinaryStatic(&semaphore_buffer);
  xTaskCreatePinnedToCore(gui_task, "gui", 4096, NULL, 1, NULL, 1);
}

void gui_wait() {
  if (semaphore != NULL) {
    xSemaphoreTake(semaphore, portMAX_DELAY);
  }
}

