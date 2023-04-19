#include "axp192.h"

typedef enum axp192_voltage_source {
    AXP192_DCDC1 = 0,
    AXP192_DCDC2,
    AXP192_DCDC3,
    AXP192_LDO2,
    AXP192_LDO3,
} axp192_voltage_source;

axp192_err_t axp192_read_byte(const axp192_t *axp, uint8_t reg, uint8_t *buffer);
axp192_err_t axp192_get_battery_voltage_unscaled(const axp192_t* axp, uint16_t *buffer);
axp192_err_t axp192_get_battery_voltage(const axp192_t* axp, float *buffer);

axp192_err_t axp192_get_voltage_enabled(const axp192_t* axp, axp192_voltage_source source, uint8_t* enabled);

axp192_err_t axp192_voltage_enable(axp192_t* axp, axp192_voltage_source source);

axp192_err_t axp192_voltage_disable(axp192_t* axp, axp192_voltage_source source);

axp192_err_t axp192_get_voltage_unscaled(axp192_t* axp, axp192_voltage_source source, uint8_t* voltage);

axp192_err_t axp192_get_voltage(axp192_t* axp, axp192_voltage_source source, uint16_t* voltage);
