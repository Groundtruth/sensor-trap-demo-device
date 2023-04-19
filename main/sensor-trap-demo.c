#include <inttypes.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "lvgl.h"
#include "display.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "ttn.h"
#include "i2c_helper.h"
#include "axp192.h"
#include "axp192_custom.h"
#include "state.h"

static const char* TAG = "sensor_trap_demo"; 

#define SLEEP_INTERVAL 10
#define HEARTBEAT_INTERVAL 600

#define TTN_SPI_HOST      SPI2_HOST
#define TTN_SPI_DMA_CHAN  SPI_DMA_DISABLED
#define TTN_PIN_SPI_SCLK  5
#define TTN_PIN_SPI_MOSI  27
#define TTN_PIN_SPI_MISO  19
#define TTN_PIN_NSS       18
#define TTN_PIN_RXTX      TTN_NOT_CONNECTED
#define TTN_PIN_RST       23
#define TTN_PIN_DIO0      26
#define TTN_PIN_DIO1      33

void fill_message(event_t event, status_t status, uint16_t battery, uint8_t* message) {
  message[0] = (event & 0x03) << 6 | (status & 0x03) << 4 | (battery & 0x0f00) >> 8;
  message[1] = (battery & 0x00ff);
}

esp_err_t get_status(status_t* result) {
  gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
  gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);
  int level = gpio_get_level(GPIO_NUM_13);

  *result = level == 1 ? STATUS_SPRUNG : STATUS_SET;

  gpio_set_pull_mode(GPIO_NUM_13, GPIO_FLOATING);

  return ESP_OK;
}

/* Set up AXP192 */
void configure_axp192() {
  ESP_LOGI(TAG, "Configuring AXP192");

  // DCDC1 enabled at 2.5V, for the OLED
  // Don't turn this off, apparently it might mess with the connection to the AXP192 chip since they share an I2C bus
  axp192_voltage_enable(&axp, AXP192_DCDC1);  
  axp192_ioctl(&axp, AXP192_DCDC1_SET_VOLTAGE, 2500);

  // DCDC2 disabled, not connected
  axp192_voltage_disable(&axp, AXP192_DCDC2);  

  // DCDC3 enabled at 3.3V, for the ESP32
  axp192_voltage_enable(&axp, AXP192_DCDC3);  
  axp192_ioctl(&axp, AXP192_DCDC3_SET_VOLTAGE, 3300);

  // LDO2 enabled at 3.3V, for the LoRa chip
  axp192_voltage_enable(&axp, AXP192_LDO2);  
  axp192_ioctl(&axp, AXP192_LDO2_SET_VOLTAGE, 3300);

  // LDO3 enabled at 3.3V, for the GPS chip
  axp192_voltage_enable(&axp, AXP192_LDO3);  
  axp192_ioctl(&axp, AXP192_LDO3_SET_VOLTAGE, 3300);
}

void app_main() {
  i2c_init(I2C_NUM_0);

  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

  if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    ESP_LOGI(TAG, "Woke due to reset");
    configure_axp192();
  }

  if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
    ESP_LOGI(TAG, "Woke due to button press, starting display");
    gui_start();
  }

  status_t new_status = STATUS_UNKNOWN;
  get_status(&new_status);
  ESP_LOGI(TAG, "New status: %s", status_name[new_status]);

  event_t event = event_from_status_transition(status, new_status);

  time_t current_time = time(NULL);

  if (event == EVENT_NONE && current_time - previous_message_time + SLEEP_INTERVAL > HEARTBEAT_INTERVAL) {
    event = EVENT_HEARTBEAT;
  }

  status = new_status;

  if (event != EVENT_NONE) {
    ESP_LOGI(TAG, "Sending message");
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(nvs_flash_init());
    
    spi_bus_config_t spi_bus_config = {
      .miso_io_num = TTN_PIN_SPI_MISO,
      .mosi_io_num = TTN_PIN_SPI_MOSI,
      .sclk_io_num = TTN_PIN_SPI_SCLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
    }; 
    ESP_ERROR_CHECK(spi_bus_initialize(TTN_SPI_HOST, &spi_bus_config, TTN_SPI_DMA_CHAN));

    ttn_init();
    ttn_configure_pins(TTN_SPI_HOST, TTN_PIN_NSS, TTN_PIN_RXTX, TTN_PIN_RST, TTN_PIN_DIO0, TTN_PIN_DIO1);
    
    ttn_start_provisioning_task();
    ttn_wait_for_provisioning();
    
    ESP_LOGI(TAG, "Turning on LoRa");
    if (ttn_resume_after_deep_sleep()) {
      ESP_LOGI(TAG, "LoRa resumed from sleep");
    } else if (ttn_join()) {
      ESP_LOGI(TAG, "LoRa newly joined");
    } else {
      ESP_LOGE(TAG, "LoRa failed to join");
    }

    uint16_t battery_voltage_unscaled;
    axp192_get_battery_voltage_unscaled(&axp, &battery_voltage_unscaled);
    ESP_LOGI(TAG, "Unscaled voltage: %d", battery_voltage_unscaled);

    uint8_t message[2];
    fill_message(event, status, battery_voltage_unscaled, message);

    ESP_LOGI(TAG, "Message: %x %x", *((uint8_t *)&message), *((uint8_t *)(&message + 1)));
    ttn_transmit_message((void *)&message, 2, 1, false);

    previous_message_time = current_time;

    ESP_LOGI(TAG, "Turning off LoRa");

    ttn_wait_for_idle();
    ttn_prepare_for_deep_sleep();
  }

  // Power off GPS and LoRa
  ttn_shutdown();
  ESP_LOGI(TAG, "Turning off GPS");
  axp192_voltage_disable(&axp, AXP192_LDO3);

  // Turn off red LED on IO4
  gpio_set_pull_mode(GPIO_NUM_4, GPIO_FLOATING);

  gui_wait();
  
  ESP_LOGI(TAG, "Setting up wakeup from timer");
  ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL * /* microseconds */ 1000 * 1000));

  ESP_LOGI(TAG, "Setting up wakeup from button");
  ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(1ull << 38, ESP_EXT1_WAKEUP_ALL_LOW));

  ESP_LOGI(TAG, "Deep sleeping...");
  esp_deep_sleep_start();
}
