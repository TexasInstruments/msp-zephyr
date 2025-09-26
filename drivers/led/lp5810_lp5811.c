/*
 * Copyright (c) 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief LP5810_LP5811 LED controller
 *
 * The LP5810 and LP5811 are 4-channel LED drivers that communicate over I2C.
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/led.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/led/lp5810_lp5811.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lp5810_lp5811, CONFIG_LED_LOG_LEVEL);

struct lp5810_lp5811_config {
	struct i2c_dt_spec bus;
};

int lp5810_lp5811_i2c_write(const struct device *dev, uint16_t reg_addr, uint8_t val) {
	const struct lp5810_lp5811_config *config = dev->config;

	const struct i2c_dt_spec bus = {
		.bus = (config->bus.bus),
		.addr = config->bus.addr | ((reg_addr >> 8) & 0x3),
	};

	return i2c_reg_write_byte_dt(&bus, reg_addr & 0xFF, val);
}

int lp5810_lp5811_i2c_read(const struct device *dev, uint16_t reg_addr, uint8_t *val) {
	const struct lp5810_lp5811_config *config = dev->config;

	const struct i2c_dt_spec bus = {
		.bus = (config->bus.bus),
		.addr = config->bus.addr | ((reg_addr >> 8) & 0x3),
	};

	return i2c_reg_read_byte_dt(&bus, reg_addr & 0xFF, val);
}

int lp5810_lp5811_update_cmd(const struct device *dev)
{	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_UPDATE_REG, LP5810_LP5811_CMD_UPDATE_EN);
	if (ret < 0) {
		LOG_ERR("LED update command failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_start_cmd(const struct device *dev)
{	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_START_REG, LP5810_LP5811_CMD_START_EN);
	if (ret < 0) {
		LOG_ERR("LED start command failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_stop_cmd(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_STOP_REG, LP5810_LP5811_CMD_STOP_EN);
	if (ret < 0) {
		LOG_ERR("LED stop command failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_pause_cmd(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_PAUSE_REG, LP5810_LP5811_CMD_PAUSE_EN);
	if (ret < 0) {
		LOG_ERR("LED pause command failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_continue_cmd(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_CONTINUE_REG, LP5810_LP5811_CMD_CONTINUE_EN);
	if (ret < 0) {
		LOG_ERR("LED continue command failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_sw_reset(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CMD_SW_RESET, LP5810_LP5811_SW_RESET_EN);
	if (ret < 0) {
		LOG_ERR("Software reset failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_chip_enable(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CHIP_EN_REG, LP5810_LP5811_CHIP_EN);
	if (ret < 0) {
		LOG_ERR("Chip enable failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_chip_disable(const struct device *dev)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_CHIP_EN_REG, LP5810_LP5811_CHIP_DISABLE);
	if (ret < 0) {
		LOG_ERR("Chip enable failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_clear_faults(const struct device *dev, uint8_t faults)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_FAULT_CLEAR_REG, faults);
	if (ret < 0) {
		LOG_ERR("Clearing faults failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_enable_max_current(const struct device *dev)
{
	int ret;
	uint8_t dev0_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_0_REG, &dev0_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 0 config");
		return ret;
	}
	
	dev0_config |= LP5810_LP5811_DEV_CONFIG_0_EN_MAX_CURRENT;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_0_REG, dev0_config);
	if (ret < 0) {
		LOG_ERR("Enabling max current failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_disable_max_current(const struct device *dev)
{
	int ret;
	uint8_t dev0_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_0_REG, &dev0_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 0 config");
		return ret;
	}
	
	dev0_config &= ~LP5810_LP5811_DEV_CONFIG_0_EN_MAX_CURRENT;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_0_REG, dev0_config);
	if (ret < 0) {
		LOG_ERR("Disabling max current failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

#ifdef BOOST_EN	
int lp5810_lp5811_set_boost_output_voltage(const struct device *dev, uint8_t boost_output_voltage)
{
	int ret;
	uint8_t dev0_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_0_REG, &dev0_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 0 config");
		return ret;
	}

	dev0_config &= ~LP5810_LP5811_DEV_CONFIG_0_BOOST_VOUT_MASK;
	dev0_config |= FIELD_PREP(LP5810_LP5811_DEV_CONFIG_0_BOOST_VOUT_MASK, boost_output_voltage);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_0_REG, dev0_config);
	if (ret < 0) {
		LOG_ERR("Setting boost output voltage failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}
#endif

int lp5810_lp5811_enable_leds(const struct device *dev, uint8_t leds)
{
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_LED_EN_REG, leds);
	if(ret < 0) {
		LOG_ERR("Enabling LEDs failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_set_pwm_freq_24khz(const struct device *dev)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_1_REG, LP5810_LP5811_DEV_CONFIG_1_PWM_FREQ_24KHZ);
	if (ret < 0) {
		LOG_ERR("LED PWM frequency update failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_pwm_freq_12khz(const struct device *dev)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_1_REG, LP5810_LP5811_DEV_CONFIG_1_PWM_FREQ_12KHZ);
	if (ret < 0) {
		LOG_ERR("LED PWM frequency update failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_led_set_mode(const struct device *dev, uint8_t led, enum lp5810_lp5811_led_mode mode)
{
	
	int ret;
	uint8_t dev3_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_3_REG, &dev3_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 3 config");
		return ret;
	}

	if((mode == LP5810_LP5811_LED_MODE_AUTONOMOUS) && !IS_BIT_SET(dev3_config, led)) {
		WRITE_BIT(dev3_config, led, 1);
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_3_REG, dev3_config);
		ret = lp5810_lp5811_update_cmd(dev);
	}
	else if((mode == LP5810_LP5811_LED_MODE_MANUAL) && (IS_BIT_SET(dev3_config, led))) {
		WRITE_BIT(dev3_config, led, 0);
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_3_REG, dev3_config);
		ret = lp5810_lp5811_update_cmd(dev);
	}

	if (ret < 0) {
		LOG_ERR("LED control mode update failed");
	}

	return ret;
}

int lp5810_lp5811_led_set_pwm_curve(const struct device *dev, uint8_t led, enum lp5810_lp5811_pwm_curve_type curve)
{
	
	int ret;
	uint8_t dev5_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_5_REG, &dev5_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 5 config");
		return ret;
	}

	if((curve == LP5810_LP5811_PWM_CURVE_EXPONENTIAL) && !IS_BIT_SET(dev5_config, led)) {
		WRITE_BIT(dev5_config, led, 1);
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_5_REG, dev5_config);
		ret = lp5810_lp5811_update_cmd(dev);
	}
	else if((curve == LP5810_LP5811_PWM_CURVE_LINEAR) && IS_BIT_SET(dev5_config, led)) {
		WRITE_BIT(dev5_config, led, 0);
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_5_REG, dev5_config);
		ret = lp5810_lp5811_update_cmd(dev);
	}

	if (ret < 0) {
		LOG_ERR("LED PWM curve update failed");
	}

	return ret;
}

int lp5810_lp5811_led_set_pwm_align(const struct device *dev, uint8_t led, enum lp5810_lp5811_pwm_align alignment)
{
	
	int ret;
	uint8_t dev7_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_7_REG, &dev7_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 7 config");
		return ret;
	}

	if(alignment != FIELD_GET(dev7_config, GENMASK(1, 0) << led)) {
		dev7_config &= ~(GENMASK(1, 0) << led);
		dev7_config |= alignment << led;
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_7_REG, dev7_config);
	}

	if (ret < 0) {
		LOG_ERR("LED PWM curve update failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_line_change_time(const struct device *dev, enum lp5810_lp5811_line_change_time time)
{
	
	int ret;
	uint8_t dev11_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_11_REG, &dev11_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 11 config");
		return ret;
	}

	if(time != FIELD_GET(dev11_config, GENMASK(1,0))) {
		dev11_config &= ~GENMASK(1,0);
		dev11_config |= time;
		ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_11_REG, dev11_config);
	}

	if (ret < 0) {
		LOG_ERR("Setting line change failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_vsync_input(const struct device *dev)
{
	
	int ret;
	uint8_t dev11_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_11_REG, &dev11_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 11 config");
		return ret;
	}

	WRITE_BIT(dev11_config, LP5810_LP5811_DEV_CONFIG_11_VSYNC_BIT, LP5810_LP5811_VSYNC_INPUT);
	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_11_REG, dev11_config);

	if (ret < 0) {
		LOG_ERR("Configuring VSYNC as input failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_vsync_output(const struct device *dev)
{
	
	int ret;
	uint8_t dev11_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_11_REG, &dev11_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 11 config");
		return ret;
	}

	WRITE_BIT(dev11_config, LP5810_LP5811_DEV_CONFIG_11_VSYNC_BIT, LP5810_LP5811_VSYNC_OUTPUT);
	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_11_REG, dev11_config);

	if (ret < 0) {
		LOG_ERR("Configuring VSYNC as output failed");
		return ret;
	}
	
	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_lsd_threshold(const struct device *dev, enum lp5810_lp5811_lsd_threshold threshold)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~LP5810_LP5811_LSD_THRESHOLD_MASK;
	dev12_config |= threshold;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set LSD threshold");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_lsd_action(const struct device *dev, enum lp5810_lp5811_lsd_action action)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~(LP5810_LP5811_LSD_ACTION_BIT);
	WRITE_BIT(dev12_config, LP5810_LP5811_LSD_ACTION_BIT, action);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set action on LED short fault");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_lod_action(const struct device *dev, enum lp5810_lp5811_lod_action action)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~LP5810_LP5811_LOD_ACTION_BIT;
	WRITE_BIT(dev12_config, LP5810_LP5811_LOD_ACTION_BIT, action);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set action on LED open fault");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_clamp_mode(const struct device *dev, enum lp5810_lp5811_clamp_mode mode)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~LP5810_LP5811_CLAMP_MODE_BIT;
	WRITE_BIT(dev12_config, LP5810_LP5811_CLAMP_MODE_BIT, mode);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set clamp mode");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_clamp_behavior(const struct device *dev, enum lp5810_lp5811_clamp_behavior behavior)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~LP5810_LP5811_CLAMP_BEHAVIOR_BIT;
	WRITE_BIT(dev12_config, LP5810_LP5811_CLAMP_BEHAVIOR_BIT, behavior);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set clamp behavior");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_clamp_voltage(const struct device *dev, enum lp5810_lp5811_clamp_voltage clamp_voltage)
{
	
	int ret;
	uint8_t dev12_config;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_DEV_CONFIG_12_REG, &dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to read current dev 12 config");
		return ret;
	}

	dev12_config &= ~LP5810_LP5811_CLAMP_VOLTAGE_MASK;
	dev12_config |= FIELD_PREP(LP5810_LP5811_CLAMP_VOLTAGE_MASK, clamp_voltage);

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_DEV_CONFIG_12_REG, dev12_config);
	if (ret < 0) {
		LOG_ERR("Failed to set clamp voltage selection");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

int lp5810_lp5811_set_analog_dimming(const struct device *dev, uint32_t led, uint8_t brightness)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_LED0_MANUAL_DC + led, brightness);
	if (ret < 0) {
		LOG_ERR("LED DC update failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_set_pwm_dimming(const struct device *dev, uint32_t led, uint8_t brightness)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_LED0_MANUAL_PWM + led, brightness);
	if (ret < 0) {
		LOG_ERR("LED PWM update failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_auto_dc(const struct device *dev, uint8_t led, uint8_t auto_dc)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_LED0_AUTO_DC + led, auto_dc);
	if (ret < 0) {
		LOG_ERR("Auto DC update failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_auto_pause_times(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_aeu_slope_time pt_end, enum lp5810_lp5811_aeu_slope_time pt_start)
{
	
	int ret;
	uint8_t reg;
	uint8_t pause_time;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) + LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_OFFSET;
	pause_time = FIELD_PREP(LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_END_MASK, pt_end) | FIELD_PREP(LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_START_MASK, pt_start);

	ret = lp5810_lp5811_i2c_write(dev, reg, pause_time);
		if (ret < 0) {
		LOG_ERR("Animation pause time update failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_auto_playback_settings(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_auto_pattern_playback_times pt, enum lp5810_lp5811_led_active_aeus aeus)
{
	
	int ret;
	uint8_t reg;
	uint8_t playback_settings;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) + LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_SETTINGS_OFFSET;
	playback_settings = FIELD_PREP(LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_TIMES_MASK, pt) | FIELD_PREP(LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_ACTIVE_AEUS_MASK, aeus);

	ret = lp5810_lp5811_i2c_write(dev, reg, playback_settings);
		if (ret < 0) {
		LOG_ERR("Auto playback settings update failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_aeu_pwm(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_animation_engine engine, enum lp5810_lp5811_animation_engine_pwm pwm, uint8_t setting)
{
	
	int ret;
	uint8_t reg;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) +
		(LP5810_LP5811_ANIMATION_ENGINE_BASE * engine) + pwm;

	ret = lp5810_lp5811_i2c_write(dev, reg, setting);
		if (ret < 0) {
		LOG_ERR("PWM update of animation engine failed");
		return ret;
	}
	
	return 0;
}

int lp5810_lp5811_led_set_aeu_t12(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_animation_engine engine, enum lp5810_lp5811_aeu_slope_time t1,
		enum lp5810_lp5811_aeu_slope_time t2)
{
	
	int ret;
	uint8_t reg;
	uint8_t t12;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) + 
		(LP5810_LP5811_ANIMATION_ENGINE_BASE + engine) + LP5810_LP5811_ANIMATION_T12_OFFSET;
	t12 = FIELD_PREP(LP5810_LP5811_ANIMATION_T2_MASK, t2) | FIELD_PREP(LP5810_LP5811_ANIMATION_T1_MASK, t1);
	
	ret = lp5810_lp5811_i2c_write(dev, reg, t12);
		if (ret < 0) {
		LOG_ERR("Update of AEU T12 slope time failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_aeu_t34(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_animation_engine engine, enum lp5810_lp5811_aeu_slope_time t3,
		enum lp5810_lp5811_aeu_slope_time t4)
{
	
	int ret;
	uint8_t reg;
	uint8_t t34;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) +
		(LP5810_LP5811_ANIMATION_ENGINE_BASE + engine) + LP5810_LP5811_ANIMATION_T34_OFFSET;
	t34 = FIELD_PREP(LP5810_LP5811_ANIMATION_T4_MASK, t4) | FIELD_PREP(LP5810_LP5811_ANIMATION_T3_MASK, t3);
	
	ret = lp5810_lp5811_i2c_write(dev, reg, t34);
		if (ret < 0) {
		LOG_ERR("Update of AEU T34 slope time failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_led_set_aeu_playback(const struct device *dev, uint8_t led,
		enum lp5810_lp5811_animation_engine engine, enum lp5810_lp5811_aeu_pattern_playback_times pt)
{
	
	int ret;
	uint8_t reg;

	reg = LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE + (led * LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE) +
		(LP5810_LP5811_ANIMATION_ENGINE_BASE + engine) + LP5810_LP5811_ANIMATION_PLAYBACK_TIME_OFFSET;
	ret = lp5810_lp5811_i2c_write(dev, reg, pt);
		if (ret < 0) {
		LOG_ERR("Update of AEU playback time failed");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_get_tsd_config_status(const struct device *dev, struct tsd_config_status *tsd_config)
{
	
	int ret;
	uint8_t tsd_status;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_TSD_CONFIG_STATUS, &tsd_status);
	if (ret < 0) {
		LOG_ERR("Failed to read TSD status");
		return ret;
	}

	tsd_config->tsd_status = IS_BIT_SET(tsd_status, 1);
	tsd_config->config_err_status = IS_BIT_SET(tsd_status, 0);

	return 0;
}

int lp5810_lp5811_get_lod_status(const struct device *dev, struct lod_status *lod_status)
{
	
	int ret;
	uint8_t lod_byte;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_LOD_STATUS, &lod_byte);
	if (ret < 0) {
		LOG_ERR("Failed to read LOD status");
		return ret;
	}

	lod_status->led_3_lod_status = IS_BIT_SET(lod_byte, 3);
	lod_status->led_2_lod_status = IS_BIT_SET(lod_byte, 2);
	lod_status->led_1_lod_status = IS_BIT_SET(lod_byte, 1);
	lod_status->led_0_lod_status = IS_BIT_SET(lod_byte, 0);

	return 0;
}

int lp5810_lp5811_get_lsd_status(const struct device *dev, struct lsd_status *lsd_status)
{
	
	int ret;
	uint8_t lsd_byte;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_LSD_STATUS, &lsd_byte);
	if (ret < 0) {
		LOG_ERR("Failed to read LSD status");
		return ret;
	}

	lsd_status->led_3_lsd_status = IS_BIT_SET(lsd_byte, 3);
	lsd_status->led_2_lsd_status = IS_BIT_SET(lsd_byte, 2);
	lsd_status->led_1_lsd_status = IS_BIT_SET(lsd_byte, 1);
	lsd_status->led_0_lsd_status = IS_BIT_SET(lsd_byte, 0);

	return 0;
}

int lp5810_lp5811_get_auto_pwm_value(const struct device *dev, uint8_t pwm, uint8_t *value)
{
	
	int ret;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_AUTO_PWM_0 + pwm, value);
	if (ret < 0) {
		LOG_ERR("Failed to read auto PWM status");
		return ret;
	}

	return 0;
}

int lp5810_lp5811_get_aep_status(const struct device *dev, struct aep_status *aep_status)
{
	
	int ret;
	uint8_t aep_byte_0;
	uint8_t aep_byte_1;

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_AEP_STATUS_0, &aep_byte_0);
	if (ret < 0) {
		LOG_ERR("Failed to read AEP status 0");
		return ret;
	}

	ret = lp5810_lp5811_i2c_read(dev, LP5810_LP5811_AEP_STATUS_1, &aep_byte_1);
	if (ret < 0) {
		LOG_ERR("Failed to read AEP status 0");
		return ret;
	}

	aep_status->aep_status_3 = FIELD_GET(LP5810_LP5811_AEP_STATUS_HIGH_MASK, aep_byte_1);
	aep_status->aep_status_2 = FIELD_GET(LP5810_LP5811_AEP_STATUS_LOW_MASK, aep_byte_1);
	aep_status->aep_status_1 = FIELD_GET(LP5810_LP5811_AEP_STATUS_HIGH_MASK, aep_byte_0);
	aep_status->aep_status_0 = FIELD_GET(LP5810_LP5811_AEP_STATUS_LOW_MASK, aep_byte_0);

	return 0;
}


static int lp5810_lp5811_init(const struct device *dev)
{
	const struct lp5810_lp5811_config *config = dev->config;
	int ret;

	if (!i2c_is_ready_dt(&config->bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	ret = lp5810_lp5811_chip_enable(dev);
	if (ret < 0) {
		LOG_ERR("Chip enable failed");
		return ret;
	}

	ret = lp5810_lp5811_i2c_write(dev, LP5810_LP5811_LED_EN_REG, BIT_MASK(LP5810_LP5811_NUM_LEDS));
	if (ret < 0) {
		LOG_ERR("Enabling LEDs failed");
		return ret;
	}

	return lp5810_lp5811_update_cmd(dev);
}

static DEVICE_API(led, lp5810_lp5811_led_api) = {
};

#define LP5810_LP5811_DEFINE(n, id)                                              	       \
	static const struct lp5810_lp5811_config lp5810_lp5811_config_##id_##n = {             \
		.bus = I2C_DT_SPEC_INST_GET(n),                                                    \
	};                                                                                     \
                                                                                           \
                                                                                           \
	DEVICE_DT_INST_DEFINE(n, &lp5810_lp5811_init, NULL, NULL,                              \
			      &lp5810_lp5811_config_##id_##n, POST_KERNEL, CONFIG_LED_INIT_PRIORITY,   \
			      &lp5810_lp5811_led_api);

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT ti_lp5810
DT_INST_FOREACH_STATUS_OKAY_VARGS(LP5810_LP5811_DEFINE, 5810)

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT ti_lp5811
DT_INST_FOREACH_STATUS_OKAY_VARGS(LP5810_LP5811_DEFINE, 5811)
