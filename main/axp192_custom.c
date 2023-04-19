#include "axp192_custom.h"

uint8_t axp192_voltage_control_register[] = {
  /* AXP192_DCDC1  */ AXP192_DCDC13_LDO23_CONTROL,
  /* AXP192_DCDC2  */ AXP192_DCDC13_LDO23_CONTROL,
  /* AXP192_DCDC3  */ AXP192_DCDC13_LDO23_CONTROL,
  /* AXP192_LDO2   */ AXP192_DCDC13_LDO23_CONTROL,
  /* AXP192_LDO3   */ AXP192_DCDC13_LDO23_CONTROL,
};

uint8_t axp192_voltage_control_register_mask[] = {
  /* AXP192_DCDC1  */ 1 << 0,
  /* AXP192_DCDC2  */ 1 << 4,
  /* AXP192_DCDC3  */ 1 << 1,
  /* AXP192_LDO2   */ 1 << 2,
  /* AXP192_LDO3   */ 1 << 3,
};

axp192_err_t axp192_read_byte(const axp192_t *axp, uint8_t reg, uint8_t *buffer) {
    return axp->read(axp->handle, AXP192_ADDRESS, reg, buffer, 1);
}

axp192_err_t axp192_get_battery_voltage_unscaled(const axp192_t* axp, uint16_t *buffer) {
    axp192_err_t ret;
    uint8_t tmp[2];
    ret = axp192_read_byte(axp, AXP192_BATTERY_VOLTAGE, tmp);
    if (ret != AXP192_OK) {
        return ret;
    }
    ret = axp192_read_byte(axp, AXP192_BATTERY_VOLTAGE + 1, tmp + 1);
    if (ret != AXP192_OK) {
        return ret;
    }

    *buffer = (tmp[0] << 4) + (tmp[1] & 0x000f);
    return ret;
}

axp192_err_t axp192_get_battery_voltage(const axp192_t* axp, float *buffer) {
    uint16_t voltage_unscaled;
    axp192_err_t ret;
    ret = axp192_get_battery_voltage_unscaled(axp, &voltage_unscaled);
    if (ret != AXP192_OK) {
        return ret;
    }

    *buffer = 1.1 * voltage_unscaled;
    return ret;
}

axp192_err_t axp192_get_voltage_enabled(const axp192_t* axp, axp192_voltage_source source, uint8_t* enabled) {
    uint8_t tmp;
    axp192_err_t ret;

    ret = axp192_read_byte(axp, axp192_voltage_control_register[source], &tmp);
    if (ret != AXP192_OK) {
        return ret;
    }

    *enabled = tmp & axp192_voltage_control_register_mask[source] ? 1 : 0;

    return ret;
}

axp192_err_t axp192_voltage_enable(axp192_t* axp, axp192_voltage_source source) {
    uint8_t tmp;
    axp192_err_t ret;

    ret = axp192_read_byte(axp, axp192_voltage_control_register[source], &tmp);
    if (ret != AXP192_OK) {
        return ret;
    }

    tmp |= axp192_voltage_control_register_mask[source];

    return axp192_write(axp, axp192_voltage_control_register[source], &tmp);
}

axp192_err_t axp192_voltage_disable(axp192_t* axp, axp192_voltage_source source) {
    uint8_t tmp;
    axp192_err_t ret;

    ret = axp192_read_byte(axp, axp192_voltage_control_register[source], &tmp);
    if (ret != AXP192_OK) {
        return ret;
    }

    tmp &= ~axp192_voltage_control_register_mask[source];

    return axp192_write(axp, axp192_voltage_control_register[source], &tmp);
}

axp192_err_t axp192_get_voltage_unscaled(axp192_t* axp, axp192_voltage_source source, uint8_t* voltage) {
    uint8_t tmp;
    axp192_err_t ret;

    uint8_t reg;

    switch (source) {
        case AXP192_DCDC1:
            reg = AXP192_DCDC1_VOLTAGE;
            break;
        case AXP192_DCDC2:
            reg = AXP192_DCDC2_VOLTAGE;
            break;
        case AXP192_DCDC3:
            reg = AXP192_DCDC3_VOLTAGE;
            break;
        case AXP192_LDO2:
        case AXP192_LDO3:
            reg = AXP192_LDO23_VOLTAGE;
            break;
        default:
            return AXP192_ERROR_NOTTY;
            break;
    }

    ret = axp192_read_byte(axp, reg, &tmp);
    if (ret != AXP192_OK) {
        return ret;
    }

    switch (source) {
        case AXP192_DCDC1:
        case AXP192_DCDC2:
        case AXP192_DCDC3:
            *voltage = tmp;
            break;
        case AXP192_LDO2:
            *voltage = (tmp & 0xf0) >> 4;
            break;
        case AXP192_LDO3:
            *voltage = tmp & 0x0f;
            break;
    }

    return ret;
}

axp192_err_t axp192_get_voltage(axp192_t* axp, axp192_voltage_source source, uint16_t* voltage) {
    uint8_t voltage_unscaled, ret;

    ret = axp192_get_voltage_unscaled(axp, source, &voltage_unscaled);
    if (ret != AXP192_OK) {
        return ret;
    }

    switch (source) {
        case AXP192_DCDC1:
        case AXP192_DCDC2:
        case AXP192_DCDC3:
            *voltage = voltage_unscaled * 25 + 700;
            break;
        case AXP192_LDO2:
        case AXP192_LDO3:
            *voltage = voltage_unscaled * 100 + 1800;
            break;
        default:
            return AXP192_ERROR_NOTTY;
            break;
    }

    return ret;
}
