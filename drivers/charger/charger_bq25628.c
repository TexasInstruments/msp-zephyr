/*
 * Copyright 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BQ25628 Datasheet: https://www.ti.com/lit/ds/symlink/bq25628.pdf
 */

#define DT_DRV_COMPAT ti_bq25628

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ti_bq25628, CONFIG_CHARGER_LOG_LEVEL);

/* Charge Current Limit */
#define BQ25628_REG_CC_LOW                    0x02
#define BQ25628_REG_CC_CHARGE_CURRENT_MASK    GENMASK(10, 5)
#define BQ25628_REG_CC_CHARGE_CURRENT_STEP_UA 40000
#define BQ25628_REG_CC_CHARGE_CURRENT_MIN_UA  0
#define BQ25628_REG_CC_CHARGE_CURRENT_MAX_UA  2000000

/* Max Charge Voltage */
#define BQ25628_REG_CV_LOW                    0x04
#define BQ25628_REG_CV_CHARGE_VOLTAGE_MASK    GENMASK(11, 3)
#define BQ25628_REG_CV_CHARGE_VOLTAGE_STEP_UV 10000
#define BQ25628_REG_CV_CHARGE_VOLTAGE_MIN_UV  3500000
#define BQ25628_REG_CV_CHARGE_VOLTAGE_MAX_UV  4800000

/*  Input Current DPM */
#define BQ25628_REG_IIN_DPM_LOW     0x06
#define BQ25628_REG_IIN_DPM_MASK    GENMASK(11, 4)
#define BQ25628_REG_IIN_DPM_STEP_UA 20000
#define BQ25628_REG_IIN_DPM_MIN_UA  20000
#define BQ25628_REG_IIN_DPM_MAX_UA  3200000

/*  Input Voltage DPM */
#define BQ25628_REG_VIN_DPM_LOW     0x08
#define BQ25628_REG_VIN_DPM_MASK    GENMASK(13, 5)
#define BQ25628_REG_VIN_DPM_STEP_UV 40000
#define BQ25628_REG_VIN_DPM_MIN_UV  3800000
#define BQ25628_REG_VIN_DPM_MAX_UV  16800000

/* Charge Control 0 */
#define BQ25628_REG_CC0              0x16
#define BQ25628_REG_CC0_INHIBIT      0x01
#define BQ25628_REG_CC0_INHIBIT_MASK BIT(5)

/* Charger Status 1 */
#define BQ25628_REG_CS1                              0x1E
#define BQ25628_REG_CS1_VBUS_STAT_MASK               GENMASK(2, 0)
#define BQ25628_REG_CS1_UNKNOWN_ADAPTER              BIT(2)
#define BQ25628_REG_CS1_CHG_STAT_MASK                GENMASK(4, 3)
#define BQ25628_REG_CS1_TRICKLE_PRE_FAST_CHARGING    BIT(3)
#define BQ25628_REG_CS1_TAPER_CHARGE                 BIT(4)
#define BQ25628_REG_CS1_TOP_OFF_TIMER_CHARGING       BIT(4) | BIT(3)

/* Mininum system voltage */
#define BQ25628_REG_MIN_SYS_VOLTAGE_HI      0x0E
#define BQ25628_REG_MIN_SYS_VOLTAGE_MASK    GENMASK(11, 6)
#define BQ25628_REG_MIN_SYS_VOLTAGE_STEP_UV 80000
#define BQ25628_REG_MIN_SYS_VOLTAGE_MIN_UV  2560000
#define BQ25628_REG_MIN_SYS_VOLTAGE_MAX_UV  3840000

/* Manufacture ID */
#define BQ25628_REG_ID_LOW       0x38
#define BQ25628_REG_ID_PN_25628  0x12

#define BQ25628_FACTOR_U_TO_M 1000

struct BQ25628_config {
	struct i2c_dt_spec i2c;
	uint32_t vsys_min_uv;
	uint32_t ichg_ua;
	uint32_t vreg_uv;
};

static inline int BQ25628_write8(const struct device *dev, uint8_t reg, uint8_t value)
{
	const struct BQ25628_config *const config = dev->config;

	return i2c_reg_write_byte_dt(&config->i2c, reg, value);
}

static inline int BQ25628_read8(const struct device *dev, uint8_t reg, uint8_t *value)
{
	const struct BQ25628_config *const config = dev->config;
	int ret;

	ret = i2c_reg_read_byte_dt(&config->i2c, reg, value);
	if (ret < 0) {
		LOG_ERR("Unable to read register");
	}
	return ret;
}

static inline int BQ25628_update8(const struct device *dev, uint8_t reg, uint8_t mask,
				  uint8_t value)
{
	const struct BQ25628_config *const config = dev->config;
	int ret;

	ret = i2c_reg_update_byte_dt(&config->i2c, reg, mask, value);
	if (ret < 0) {
		LOG_ERR("Unable to update register");
	}
	return ret;
}

static inline int BQ25628_write16(const struct device *dev, uint8_t reg, uint16_t value)
{
	const struct BQ25628_config *const config = dev->config;
	uint8_t buf[3];

	buf[0] = reg;
	sys_put_le16(value, &buf[1]);

	return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

static inline int BQ25628_read16(const struct device *dev, uint8_t reg, uint16_t *value)
{
	const struct BQ25628_config *config = dev->config;
	uint8_t i2c_data[2];
	int ret;

	ret = i2c_burst_read_dt(&config->i2c, reg, i2c_data, sizeof(i2c_data));
	if (ret < 0) {
		LOG_ERR("Unable to read register");
		return ret;
	}

	*value = sys_get_le16(i2c_data);

	return 0;
}

static int BQ25628_set_minimum_system_voltage(const struct device *dev, uint32_t voltage_uv)
{
	if (!IN_RANGE(voltage_uv, BQ25628_REG_MIN_SYS_VOLTAGE_MIN_UV,
		      BQ25628_REG_MIN_SYS_VOLTAGE_MAX_UV)) {
		LOG_WRN("minimum system voltage out of range: %umV, "
			"clamping to the nearest limit",
			voltage_uv / BQ25628_FACTOR_U_TO_M);
	}

	uint32_t v;

	voltage_uv = CLAMP(voltage_uv, BQ25628_REG_MIN_SYS_VOLTAGE_MIN_UV,
			   BQ25628_REG_MIN_SYS_VOLTAGE_MAX_UV);
	v = voltage_uv / BQ25628_REG_MIN_SYS_VOLTAGE_STEP_UV;
	v = FIELD_PREP(BQ25628_REG_MIN_SYS_VOLTAGE_MASK, v);
	return BQ25628_write8(dev, BQ25628_REG_MIN_SYS_VOLTAGE_HI, v);
}

static int BQ25628_set_constant_charge_current(const struct device *dev, uint32_t current_ua)
{
	if (current_ua > BQ25628_REG_CC_CHARGE_CURRENT_MAX_UA) {
		LOG_WRN("charging current out of range: %umA, "
			"clamping to the nearest limit",
			current_ua / BQ25628_FACTOR_U_TO_M);
	}
	current_ua = CLAMP(current_ua, BQ25628_REG_CC_CHARGE_CURRENT_MIN_UA,
			   BQ25628_REG_CC_CHARGE_CURRENT_MAX_UA);
	uint32_t c;

	c = current_ua / BQ25628_REG_CC_CHARGE_CURRENT_STEP_UA;
	c = FIELD_PREP(BQ25628_REG_CC_CHARGE_CURRENT_MASK, c);

	return BQ25628_write16(dev, BQ25628_REG_CC_LOW, c);
}

static int BQ25628_set_constant_charge_voltage(const struct device *dev, uint32_t voltage_uv)
{
	if (!IN_RANGE(voltage_uv, BQ25628_REG_CV_CHARGE_VOLTAGE_MIN_UV,
		      BQ25628_REG_CV_CHARGE_VOLTAGE_MAX_UV)) {
		LOG_WRN("charging voltage out of range: %umV, "
			"clamping to the nearest limit",
			voltage_uv / BQ25628_FACTOR_U_TO_M);
	}

	uint32_t v;

	voltage_uv = CLAMP(voltage_uv, BQ25628_REG_CV_CHARGE_VOLTAGE_MIN_UV,
			   BQ25628_REG_CV_CHARGE_VOLTAGE_MAX_UV);
	v = voltage_uv / BQ25628_REG_CV_CHARGE_VOLTAGE_STEP_UV;
	v = FIELD_PREP(BQ25628_REG_CV_CHARGE_VOLTAGE_MASK, v);
	return BQ25628_write16(dev, BQ25628_REG_CV_LOW, v);
}

static int BQ25628_set_iindpm(const struct device *dev, uint32_t current_ua)
{
	if (!IN_RANGE(current_ua, BQ25628_REG_IIN_DPM_MIN_UA, BQ25628_REG_IIN_DPM_MAX_UA)) {
		LOG_WRN("input current regulation out of range: %umA, "
			"clamping to the nearest limit",
			current_ua / BQ25628_FACTOR_U_TO_M);
	}

	uint32_t a;

	current_ua = CLAMP(current_ua, BQ25628_REG_IIN_DPM_MIN_UA, BQ25628_REG_IIN_DPM_MAX_UA);
	a = current_ua / BQ25628_REG_IIN_DPM_STEP_UA;
	a = FIELD_PREP(BQ25628_REG_IIN_DPM_MASK, a);
	return BQ25628_write16(dev, BQ25628_REG_IIN_DPM_LOW, a);
}

static int BQ25628_set_vindpm(const struct device *dev, uint32_t voltage_uv)
{
	if (!IN_RANGE(voltage_uv, BQ25628_REG_VIN_DPM_MIN_UV,
		      BQ25628_REG_VIN_DPM_MAX_UV)) {
		LOG_WRN("input voltage regulation of range: %umV, "
			"clamping to the nearest limit",
			voltage_uv / BQ25628_FACTOR_U_TO_M);
	}

	uint32_t v;

	v = CLAMP(voltage_uv, BQ25628_REG_VIN_DPM_MIN_UV,
			   BQ25628_REG_VIN_DPM_MAX_UV);
	v = FIELD_PREP(BQ25628_REG_VIN_DPM_MASK, v);
	return BQ25628_write16(dev, BQ25628_REG_VIN_DPM_LOW, v);
}

static int BQ25628_get_constant_charge_current(const struct device *dev, uint32_t *current_ua)
{
	uint16_t v;
	int ret;

	ret = BQ25628_read16(dev, BQ25628_REG_CC_LOW, &v);
	if (ret < 0) {
		return ret;
	}

	v = FIELD_GET(BQ25628_REG_CC_CHARGE_CURRENT_MASK, v);

	*current_ua = v * BQ25628_REG_CC_CHARGE_CURRENT_STEP_UA;

	return 0;
}

static int BQ25628_get_constant_charge_voltage(const struct device *dev, uint32_t *voltage_uv)
{
	uint16_t value;
	int ret;

	ret = BQ25628_read16(dev, BQ25628_REG_CV_LOW, &value);
	if (ret < 0) {
		return ret;
	}
	value = FIELD_GET(BQ25628_REG_CV_CHARGE_VOLTAGE_MASK, value);

	*voltage_uv = value * BQ25628_REG_CV_CHARGE_VOLTAGE_STEP_UV;

	return 0;
}

static int BQ25628_get_iindpm(const struct device *dev, uint32_t *current_ua)
{
	uint16_t value;
	int ret;

	ret = BQ25628_read16(dev, BQ25628_REG_IIN_DPM_LOW, &value);
	if (ret < 0) {
		return ret;
	}
	value = FIELD_GET(BQ25628_REG_IIN_DPM_MASK, value);

	*current_ua = value * BQ25628_REG_IIN_DPM_STEP_UA;

	return 0;
}

static int BQ25628_get_vindpm(const struct device *dev, uint32_t *voltage_uv)
{
	uint16_t value;
	int ret;

	ret = BQ25628_read16(dev, BQ25628_REG_VIN_DPM_LOW, &value);
	if (ret < 0) {
		return ret;
	}
	value = FIELD_GET(BQ25628_REG_VIN_DPM_MASK, value);

	*voltage_uv = (value * BQ25628_REG_VIN_DPM_STEP_UV);

	return 0;
}

static int BQ25628_get_status(const struct device *dev, enum charger_status *status)
{
	uint8_t charge_status;
	int ret;

	ret = BQ25628_read8(dev, BQ25628_REG_CS1, &charge_status);
	if (ret < 0) {
		return ret;
	}

	switch (FIELD_GET(BQ25628_REG_CS1_CHG_STAT_MASK, charge_status)) {
	case BQ25628_REG_CS1_TRICKLE_PRE_FAST_CHARGING:
		__fallthrough;
	case BQ25628_REG_CS1_TAPER_CHARGE:
		__fallthrough;
	case BQ25628_REG_CS1_TOP_OFF_TIMER_CHARGING:
		*status = CHARGER_STATUS_CHARGING;
		break;
	default:
		*status = CHARGER_STATUS_NOT_CHARGING;
		break;
	}

	return 0;
}

static int BQ25628_get_online(const struct device *dev, enum charger_online *online)
{
	uint8_t status;
	int ret;

	ret = BQ25628_read8(dev, BQ25628_REG_CS1, &status);
	if (ret < 0) {
		return ret;
	}

	status = FIELD_GET(BQ25628_REG_CS1_VBUS_STAT_MASK, status);
	if (status == BQ25628_REG_CS1_UNKNOWN_ADAPTER) {
		*online = CHARGER_ONLINE_FIXED;
	} else {
		*online = CHARGER_ONLINE_OFFLINE;
	}

	return 0;
}

static int BQ25628_charger_get_charge_type(const struct device *dev,
					   enum charger_charge_type *charge_type)
{
	uint8_t ct;
	int ret;

	ret = BQ25628_read8(dev, BQ25628_REG_CS1, &ct);
	if (ret < 0) {
		return ret;
	}

	if(FIELD_GET(BQ25628_REG_CS1_CHG_STAT_MASK, ct) == 0) {
		*charge_type = CHARGER_CHARGE_TYPE_NONE;
	}
	else {
		*charge_type = CHARGER_CHARGE_TYPE_UNKNOWN;
	}

	return 0;
}

static int BQ25628_get_prop(const struct device *dev, charger_prop_t prop,
			    union charger_propval *value)
{
	switch (prop) {
	case CHARGER_PROP_ONLINE:
		return BQ25628_get_online(dev, &value->online);
	case CHARGER_PROP_CHARGE_TYPE:
		return BQ25628_charger_get_charge_type(dev, &value->charge_type);
	case CHARGER_PROP_STATUS:
		return BQ25628_get_status(dev, &value->status);
	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		return BQ25628_get_constant_charge_current(dev, &value->const_charge_current_ua);
	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		return BQ25628_get_constant_charge_voltage(dev, &value->const_charge_voltage_uv);
	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		return BQ25628_get_iindpm(dev, &value->input_current_regulation_current_ua);
	case CHARGER_PROP_INPUT_REGULATION_VOLTAGE_UV:
		return BQ25628_get_vindpm(dev, &value->input_voltage_regulation_voltage_uv);
	default:
		return -ENOTSUP;
	}
}

static int BQ25628_set_prop(const struct device *dev, charger_prop_t prop,
			    const union charger_propval *value)
{
	switch (prop) {
	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		return BQ25628_set_constant_charge_current(dev, value->const_charge_current_ua);
	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		return BQ25628_set_constant_charge_voltage(dev, value->const_charge_voltage_uv);
	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		return BQ25628_set_iindpm(dev, value->input_current_regulation_current_ua);
	case CHARGER_PROP_INPUT_REGULATION_VOLTAGE_UV:
		return BQ25628_set_vindpm(dev, value->input_voltage_regulation_voltage_uv);
	default:
		return -ENOTSUP;
	}
}

static int BQ25628_charge_enable(const struct device *dev, const bool enable)
{
	uint8_t value = enable ? 0 : BQ25628_REG_CC0_INHIBIT;

	return BQ25628_update8(dev, BQ25628_REG_CC_LOW, BQ25628_REG_CC0_INHIBIT_MASK, value);
}

static int BQ25628_set_config(const struct device *dev)
{
	const struct BQ25628_config *const config = dev->config;
	union charger_propval value;
	int ret;

	value.const_charge_current_ua = config->ichg_ua;

	ret = BQ25628_set_constant_charge_current(dev, value.const_charge_current_ua);
	if (ret < 0) {
		return ret;
	}

	value.const_charge_voltage_uv = config->vreg_uv;

	ret = BQ25628_set_constant_charge_voltage(dev, value.const_charge_voltage_uv);
	if (ret < 0) {
		return ret;
	}

	return BQ25628_set_minimum_system_voltage(dev, config->vsys_min_uv);
}

static int BQ25628_init(const struct device *dev)
{
	uint8_t value;
	int ret;

	ret = BQ25628_read8(dev, BQ25628_REG_ID_LOW, &value);
	if (ret < 0) {
		return ret;
	}

	switch (value) {
	case BQ25628_REG_ID_PN_25628:
		break;
	default:
		LOG_ERR("Error unknown model: 0x%04x\n", value);
		return -ENODEV;
	}

	return BQ25628_set_config(dev);
}

static DEVICE_API(charger, bq25628_driver_api) = {
	.get_property = BQ25628_get_prop,
	.set_property = BQ25628_set_prop,
	.charge_enable = BQ25628_charge_enable,
};

#define BQ25628_INIT(inst)                                                                         \
                                                                                                   \
	static const struct BQ25628_config BQ25628_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.ichg_ua = DT_INST_PROP(inst, constant_charge_current_max_microamp),               \
		.vreg_uv = DT_INST_PROP(inst, constant_charge_voltage_max_microvolt),              \
		.vsys_min_uv = DT_INST_PROP(inst, system_voltage_min_threshold_microvolt),         \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, BQ25628_init, NULL, NULL, &BQ25628_config_##inst, POST_KERNEL, \
			      CONFIG_CHARGER_INIT_PRIORITY, &bq25628_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BQ25628_INIT)
