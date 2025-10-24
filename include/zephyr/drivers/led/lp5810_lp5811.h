/*
 * Copyright 2025 Texas Instruments
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_LED_LP5810_LP5811_H_
#define ZEPHYR_INCLUDE_DRIVERS_LED_LP5810_LP5811_H_

#define LP5810_LP5811_NUM_LEDS 4

#if DT_HAS_COMPAT_STATUS_OKAY(ti_lp5811)
#define BOOST_EN 1
#endif

/* Chip Enable Register */
#define LP5810_LP5811_CHIP_EN_REG  0x00
#define LP5810_LP5811_CHIP_EN BIT(0)
#define LP5810_LP5811_CHIP_DISABLE 0x00

/* Dev Config 0 Register: Enable Max Current Drive */
#define LP5810_LP5811_DEV_CONFIG_0_REG 0x01
#define LP5810_LP5811_DEV_CONFIG_0_BOOST_VOUT_MASK GENMASK(1, 5)
#define LP5810_LP5811_DEV_CONFIG_0_EN_MAX_CURRENT BIT(0)

/* Dev Config 1 Register: PWM Frequency Control */
#define LP5810_LP5811_DEV_CONFIG_1_REG 0x02
#define LP5810_LP5811_DEV_CONFIG_1_PWM_FREQ_24KHZ 0x00
#define LP5810_LP5811_DEV_CONFIG_1_PWM_FREQ_12KHZ BIT(7)

/* Dev Config 3 Register: LED Mode Control */
#define LP5810_LP5811_DEV_CONFIG_3_REG 0x04

/* Dev Config 5 Register: LED PWM Curve Control */
#define LP5810_LP5811_DEV_CONFIG_5_REG 0x06

/* Dev Config 6 Register: PWM Phase Alignment Control */
#define LP5810_LP5811_DEV_CONFIG_7_REG 0x08

/* Dev Config 11 Register: Line Change and VSYNC Control */
#define LP5810_LP5811_DEV_CONFIG_11_REG 0x0C
#define LP5810_LP5811_DEV_CONFIG_11_VSYNC_BIT 0x2

/* Dev Config 12 Register: */
#define LP5810_LP5811_DEV_CONFIG_12_REG 0x0D
#define LP5810_LP5811_LSD_THRESHOLD_MASK GENMASK(1, 0)
#define LP5810_LP5811_LSD_ACTION_BIT 0x2
#define LP5810_LP5811_LOD_ACTION_BIT 0x3
#define LP5810_LP5811_CLAMP_MODE_BIT 0x4
#define LP5810_LP5811_CLAMP_BEHAVIOR_BIT 0x5
#define LP5810_LP5811_CLAMP_VOLTAGE_MASK GENMASK(7, 6)

/* Command Registers */
#define LP5810_LP5811_CMD_UPDATE_REG 0x10
#define LP5810_LP5811_CMD_UPDATE_EN 0x55
#define LP5810_LP5811_CMD_START_REG 0x11
#define LP5810_LP5811_CMD_START_EN 0xFF
#define LP5810_LP5811_CMD_STOP_REG 0x12
#define LP5810_LP5811_CMD_STOP_EN 0xAA
#define LP5810_LP5811_CMD_PAUSE_REG 0x13
#define LP5810_LP5811_CMD_PAUSE_EN 0x33
#define LP5810_LP5811_CMD_CONTINUE_REG 0x14
#define LP5810_LP5811_CMD_CONTINUE_EN 0xCC

/* LED Enable Registers */
#define LP5810_LP5811_LED_EN_REG 0x20
#define LP5810_LP5811_LED0 0
#define LP5810_LP5811_LED1 1
#define LP5810_LP5811_LED2 2
#define LP5810_LP5811_LED3 3

/* Fault Clear Register */
#define LP5810_LP5811_FAULT_CLEAR_REG 0x22
#define LP5810_LP5811_TSD_FAULT_CLEAR BIT(2)
#define LP5810_LP5811_LSD_FAULT_CLEAR BIT(1)
#define LP5810_LP5811_LOD_FAULT_CLEAR BIT(0)

/* Software Reset Register */
#define LP5810_LP5811_CMD_SW_RESET 0x23
#define LP5810_LP5811_SW_RESET_EN 0x66

/* Manual DC base register */
#define LP5810_LP5811_LED0_MANUAL_DC 0x30

/* Manual PWM base register */
#define LP5810_LP5811_LED0_MANUAL_PWM 0x40

/* Auto DC base register */
#define LP5810_LP5811_LED0_AUTO_DC 0x50

/* Autonomous Animiation register base (total 4) */
#define LP5810_LP5811_LED_AUTO_ANIMATION_REG_BASE 0x80
#define LP5810_LP5811_LED_AUTO_ANIMATION_REG_SIZE 0x1A
#define LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_OFFSET 0x00
#define LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_END_MASK GENMASK(3, 0)
#define LP5810_LP5811_LED_AUTO_ANIMATION_PAUSE_TIME_START_MASK GENMASK(7, 4)
#define LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_SETTINGS_OFFSET 0x01
#define LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_TIMES_MASK GENMASK(3, 0)
#define LP5810_LP5811_LED_AUTO_ANIMATION_PLAYBACK_ACTIVE_AEUS_MASK GENMASK(5, 4)

/* Animiation engine size (including PWM, T12, T23, Playback registers)*/
#define LP5810_LP5811_ANIMATION_ENGINE_SIZE 0x08
#define LP5810_LP5811_ANIMATION_ENGINE_BASE 0x02
#define LP5810_LP5811_ANIMATION_T12_OFFSET 0x05
#define LP5810_LP5811_ANIMATION_T1_MASK GENMASK(3, 0)
#define LP5810_LP5811_ANIMATION_T2_MASK GENMASK(7, 4)
#define LP5810_LP5811_ANIMATION_T34_OFFSET 0x06
#define LP5810_LP5811_ANIMATION_T3_MASK GENMASK(3, 0)
#define LP5810_LP5811_ANIMATION_T4_MASK GENMASK(7, 4)
#define LP5810_LP5811_ANIMATION_PLAYBACK_TIME_OFFSET 0x07

/* Status Registers */
#define LP5810_LP5811_TSD_CONFIG_STATUS 0x300
#define LP5810_LP5811_LOD_STATUS 0x301
#define LP5810_LP5811_LSD_STATUS 0x303
#define LP5810_LP5811_AUTO_PWM_0 0x305
#define LP5810_LP5811_AEP_STATUS_0 0x315
#define LP5810_LP5811_AEP_STATUS_1 0x316
#define LP5810_LP5811_AEP_STATUS_LOW_MASK GENMASK(2, 0)
#define LP5810_LP5811_AEP_STATUS_HIGH_MASK GENMASK(5, 3)

/**
 * @brief LED control mode options
 *
 * Defines whether LEDs operate in manual or autonomous mode
 */
enum lp5810_lp5811_led_mode {
    /** LED operates in manual mode (default) */
    LP5810_LP5811_LED_MODE_MANUAL = 0x0, 
    /** LED operates in autonomous mode */
    LP5810_LP5811_LED_MODE_AUTONOMOUS = 0x1,
};

/**
 * @brief PWM dimming curve type options
 *
 * Defines the PWM dimming curve characteristics
 */
enum lp5810_lp5811_pwm_curve_type {
    /** Linear PWM dimming curve (default) */
    LP5810_LP5811_PWM_CURVE_LINEAR = 0x0,
    /** Exponential PWM dimming curve */
    LP5810_LP5811_PWM_CURVE_EXPONENTIAL = 0x1,
};

/**
 * @brief PWM phase alignment options
 *
 * Defines how the PWM pulses are aligned within the PWM period
 */
enum lp5810_lp5811_pwm_align {
    /** Forward alignment (pulse at beginning of period) */
    LP5810_LP5811_PWM_ALIGN_FORWARD = 0x0,
    /** Middle alignment (pulse centered in period) */
    LP5810_LP5811_PWM_ALIGN_MIDDLE = 0x2,
    /** Backward alignment (pulse at end of period) */
    LP5810_LP5811_PWM_ALIGN_BACKWARD = 0x3,
};

/**
 * @brief VSYNC pin configuration options
 *
 * Defines whether VSYNC pin is used as input or output
 */
enum lp5810_lp5811_vsync_mode {
    /** VSYNC pin configured as input */
    LP5810_LP5811_VSYNC_INPUT = 0x0,
    /** VSYNC pin configured as output (exports internal oscillator clock) */
    LP5810_LP5811_VSYNC_OUTPUT = 0x1,
};

/**
 * @brief Line change time options
 *
 * Defines the time taken to change lines during LED operation
 */
enum lp5810_lp5811_line_change_time {
	/** Line change time of 1.0 microseconds */
	LP5810_LP5811_LINE_CHANGE_1US = 0x0,
	/** Line change time of 1.3 microseconds */
    LP5810_LP5811_LINE_CHANGE_1_3US = 0x1,
    /** Line change time of 1.7 microseconds */
    LP5810_LP5811_LINE_CHANGE_1_7US = 0x2,
    /** Line change time of 2.0 microseconds */
    LP5810_LP5811_LINE_CHANGE_2US = 0x3,
};

/**
 * @brief LED Short Detection (LSD) threshold options
 * Defines the voltage threshold for detecting LED shorts
 */
enum lp5810_lp5811_lsd_threshold {
    /** LSD threshold at 0.35 * VOUT */
    LP5810_LP5811_LSD_THRESHOLD_0_35_VOUT = 0x0,
    /** LSD threshold at 0.45 * VOUT */
    LP5810_LP5811_LSD_THRESHOLD_0_45_VOUT = 0x1,
    /** LSD threshold at 0.55 * VOUT */
    LP5810_LP5811_LSD_THRESHOLD_0_55_VOUT = 0x2,
    /** LSD threshold at 0.65 * VOUT */
    LP5810_LP5811_LSD_THRESHOLD_0_65_VOUT = 0x3,
};

/**
 * @brief LED Short Detection (LSD) action options
 * Defines the action to take when an LED short is detected
 */
enum lp5810_lp5811_lsd_action {
    /** No action taken when LED short is detected */
    LP5810_LP5811_LSD_ACTION_NONE = 0x0,
    /** All outputs shut down when LED short is detected */
    LP5810_LP5811_LSD_ACTION_SHUTDOWN_ALL = 0x1,
};

/**
 * @brief LED Open Detection (LOD) action options
 * Defines the action to take when an LED open fault is detected
 */
enum lp5810_lp5811_lod_action {
    /** No action taken when LED open fault is detected */
    LP5810_LP5811_LOD_ACTION_NONE = 0x0,
    /** Current sink shut down when LED open fault is detected */
    LP5810_LP5811_LOD_ACTION_SHUTDOWN_SINK = 0x1,
};

/**
 * @brief Clamp behavior options
 * Defines whether the clamp feature is enabled or disabled
 */
enum lp5810_lp5811_clamp_mode {
    /** Enable clamp behavior */
    LP5810_LP5811_CLAMP_ENABLE = 0x0,
    /** Disable clamp behavior */
    LP5810_LP5811_CLAMP_DISABLE = 0x1,
};

/**
 * @brief Clamp behavior selection options
 * Defines when the clamp feature is applied
 */
enum lp5810_lp5811_clamp_behavior {
    /** Clamp outputs only during line change time */
    LP5810_LP5811_CLAMP_DURING_LINE_CHANGE = 0x0,
    /** Clamp outputs when current sink turns off */
    LP5810_LP5811_CLAMP_WHEN_CURRENT_SINK_OFF = 0x1,
};

/**
 * @brief Clamp voltage selection options
 * Defines the voltage level for the clamp feature
 */
enum lp5810_lp5811_clamp_voltage {
    /** Clamp voltage at VOUT minus 1.1V */
    LP5810_LP5811_CLAMP_VOLTAGE_VOUT_MINUS_1_1V = 0x0,
    /** Clamp voltage at VOUT minus 1.3V */
    LP5810_LP5811_CLAMP_VOLTAGE_VOUT_MINUS_1_3V = 0x1,
    /** Clamp voltage at VOUT minus 1.5V */
    LP5810_LP5811_CLAMP_VOLTAGE_VOUT_MINUS_1_5V = 0x2,
    /** Clamp voltage at VOUT minus 1.7V */
    LP5810_LP5811_CLAMP_VOLTAGE_VOUT_MINUS_1_7V = 0x3,
};

enum lp5810_lp5811_animation_engine {
	LP5810_LP5811_ANIMATION_ENGINE_1 = 0x1,
	LP5810_LP5811_ANIMATION_ENGINE_2 = 0x2,
	LP5810_LP5811_ANIMATION_ENGINE_3 = 0x3,
};

/**
 * @brief Animation pattern playback times
 * Defines how many times the pattern is played back
 */
enum lp5810_lp5811_auto_pattern_playback_times {
    /** Play pattern 0 times (disabled) */
    LP5810_LP5811_AUTO_PLAYBACK_0_TIMES = 0x0,
    /** Play pattern 1 time */
    LP5810_LP5811_AUTO_PLAYBACK_1_TIME = 0x1,
    /** Play pattern 2 times */
    LP5810_LP5811_AUTO_PLAYBACK_2_TIMES = 0x2,
    /** Play pattern 3 times */
    LP5810_LP5811_AUTO_PLAYBACK_3_TIMES = 0x3,
    /** Play pattern 4 times */
    LP5810_LP5811_AUTO_PLAYBACK_4_TIMES = 0x4,
    /** Play pattern 5 times */
    LP5810_LP5811_AUTO_PLAYBACK_5_TIMES = 0x5,
    /** Play pattern 6 times */
    LP5810_LP5811_AUTO_PLAYBACK_6_TIMES = 0x6,
    /** Play pattern 7 times */
    LP5810_LP5811_AUTO_PLAYBACK_7_TIMES = 0x7,
    /** Play pattern 8 times */
    LP5810_LP5811_AUTO_PLAYBACK_8_TIMES = 0x8,
    /** Play pattern 9 times */
    LP5810_LP5811_AUTO_PLAYBACK_9_TIMES = 0x9,
    /** Play pattern 10 times */
    LP5810_LP5811_AUTO_PLAYBACK_10_TIMES = 0xA,
    /** Play pattern 11 times */
    LP5810_LP5811_AUTO_PLAYBACK_11_TIMES = 0xB,
    /** Play pattern 12 times */
    LP5810_LP5811_AUTO_PLAYBACK_12_TIMES = 0xC,
    /** Play pattern 13 times */
    LP5810_LP5811_AUTO_PLAYBACK_13_TIMES = 0xD,
    /** Play pattern 14 times */
    LP5810_LP5811_AUTO_PLAYBACK_14_TIMES = 0xE,
    /** Play pattern infinite times (continuous loop) */
    LP5810_LP5811_AUTO_PLAYBACK_INFINITE = 0xF,
};

/**
 * @brief Active Animation Engine Unit (AEU) number selection
 * Defines which animation engines are active for a LED
 */
enum lp5810_lp5811_led_active_aeus {
    /** Only use Animation Engine Unit 1 (AEU1) */
    LP5810_LP5811_USE_AEU1_ONLY = 0x0,
    /** Use Animation Engine Units 1 and 2 (AEU1 and AEU2) */
    LP5810_LP5811_LED2_USE_AEU1_AEU2 = 0x1,
    /** Use Animation Engine Units 1, 2, and 3 (AEU1, AEU2, and AEU3) */
    LP5810_LP5811_LED2_USE_AEU1_AEU2_AEU3 = 0x2,
};

enum lp5810_lp5811_animation_engine_pwm {
	LP5810_LP5811_ANIMATION_ENGINE_PWM1 = 0x0,
	LP5810_LP5811_ANIMATION_ENGINE_PWM2 = 0x1,
	LP5810_LP5811_ANIMATION_ENGINE_PWM3 = 0x2,
	LP5810_LP5811_ANIMATION_ENGINE_PWM4 = 0x3,
	LP5810_LP5811_ANIMATION_ENGINE_PWM5 = 0x4,
};

/**
 * @brief Animation Engine Unit 1 (AEU1) T1 slope time options
 *
 * Defines the slope time for LED_0 in Animation Engine Unit 1
 */
enum lp5810_lp5811_aeu_slope_time {
    /** No pause time */
    LP5810_LP5811_AEU_NO_PAUSE = 0x0,
    /** Slope time of 0.09 seconds */
    LP5810_LP5811_AEU_0_09S = 0x1,
    /** Slope time of 0.18 seconds */
    LP5810_LP5811_AEU_0_18S = 0x2,
    /** Slope time of 0.36 seconds */
    LP5810_LP5811_AEU_0_36S = 0x3,
    /** Slope time of 0.54 seconds */
    LP5810_LP5811_AEU_0_54S = 0x4,
    /** Slope time of 0.80 seconds */
    LP5810_LP5811_AEU_0_80S = 0x5,
    /** Slope time of 1.07 seconds */
    LP5810_LP5811_AEU_1_07S = 0x6,
    /** Slope time of 1.52 seconds */
    LP5810_LP5811_AEU_1_52S = 0x7,
    /** Slope time of 2.06 seconds */
    LP5810_LP5811_AEU_2_06S = 0x8,
    /** Slope time of 2.50 seconds */
    LP5810_LP5811_AEU_2_50S = 0x9,
    /** Slope time of 3.04 seconds */
    LP5810_LP5811_AEU_3_04S = 0xA,
    /** Slope time of 4.02 seconds */
    LP5810_LP5811_AEU_4_02S = 0xB,
    /** Slope time of 5.01 seconds */
    LP5810_LP5811_AEU_5_01S = 0xC,
    /** Slope time of 5.99 seconds */
    LP5810_LP5811_AEU_5_99S = 0xD,
    /** Slope time of 7.06 seconds */
    LP5810_LP5811_AEU_7_06S = 0xE,
    /** Slope time of 8.05 seconds */
    LP5810_LP5811_AEU_8_05S = 0xF,
};

struct tsd_config_status {
	uint8_t tsd_status : 1;
	uint8_t config_err_status : 1;
};

struct lod_status {
	uint8_t led_3_lod_status : 1;
	uint8_t led_2_lod_status : 1;
	uint8_t led_1_lod_status : 1;
	uint8_t led_0_lod_status : 1;
};

struct lsd_status {
	uint8_t led_3_lsd_status : 1;
	uint8_t led_2_lsd_status : 1;
	uint8_t led_1_lsd_status : 1;
	uint8_t led_0_lsd_status : 1;
};

struct aep_status {
	uint8_t aep_status_3 : 3;
	uint8_t aep_status_2 : 3;
	uint8_t aep_status_1 : 3;
	uint8_t aep_status_0 : 3;
};

/**
 * @brief Animation Engine (AEU) pattern playback times
 * Defines how many times the pattern is played back
 */
enum lp5810_lp5811_aeu_pattern_playback_times {
    /** Play pattern 0 times (disabled) */
    LP5810_LP5811_AEU_PLAYBACK_0_TIMES = 0x0,
    /** Play pattern 1 time */
    LP5810_LP5811_AEU_PLAYBACK_1_TIME = 0x1,
    /** Play pattern 2 times */
    LP5810_LP5811_AEU_PLAYBACK_2_TIMES = 0x2,
    /** Play pattern infinite times (continuous loop) */
    LP5810_LP5811_AEU_PLAYBACK_INFINITE = 0x3,
};

/**
 * @brief Wrapper function for I2C writes to LP5810/LP5811
 *
 * The LP5810/LP5811 packs upper register bits into the first
 * address byte of a transmission, requiring a wrapper function
 * to update the i2c_dt_spec.
 *
 * @param dev LED device structure
 * @param reg_addr Address to write to 
 * @param val Value to write to specified address
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_i2c_write(const struct device *dev, uint16_t reg_addr, uint8_t val);

/**
 * @brief Wrapper function for I2C reads to LP5810/LP5811
 *
 * The LP5810/LP5811 packs upper register bits into the first
 * address byte of a transmission, requiring a wrapper function
 * to update the i2c_dt_spec.
 *
 * @param dev LED device structure
 * @param reg_addr Address to read from 
 * @param val Storage where read value will be populated
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_i2c_read(const struct device *dev, uint16_t reg_addr, uint8_t *val);

/**
 * @brief Update the LP5810_LP5811 command register
 *
 * Updates the command register to apply configuration changes
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_update_cmd(const struct device *dev);

/**
 * @brief Start animation on the LP5810_LP5811
 *
 * Sends the start command to begin LED animation
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_start_cmd(const struct device *dev);

/**
 * @brief Stop animation on the LP5810_LP5811
 *
 * Sends the stop command to halt LED animation
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_stop_cmd(const struct device *dev);

/**
 * @brief Pause animation on the LP5810_LP5811
 *
 * Sends the pause command to pause LED animation
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_pause_cmd(const struct device *dev);

/**
 * @brief Continue animation on the LP5810_LP5811
 *
 * Sends the continue command to resume paused LED animation
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_continue_cmd(const struct device *dev);

/**
 * @brief Clear fault conditions on the LP5810_LP5811
 *
 * Clears specified fault conditions (TSD, LSD, LOD)
 *
 * @param dev LED device structure
 * @param faults Bitmask of faults to clear
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_clear_faults(const struct device *dev, uint8_t faults);

/**
 * @brief Software reset of the LP5810_LP5811
 *
 * Performs a software reset of the device
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_sw_reset(const struct device *dev);

/**
 * @brief Chip enable of the LP5810_LP5811
 *
 * Enables the internal circuits of the LP5810_LP5811.
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_chip_enable(const struct device *dev);

/**
 * @brief Chip disable of the LP5810_LP5811
 *
 * Disables the internal circuits of the LP5810_LP5811.
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_chip_disable(const struct device *dev);

/**
 * @brief Enable maximum current drive
 *
 * Enables maximum current drive capability
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_enable_max_current(const struct device *dev);

/**
 * @brief Disable maximum current drive
 *
 * Disables maximum current drive capability
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_disable_max_current(const struct device *dev);


#if BOOST_EN
/**
 * @brief Set the boost output voltage
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_boost_output_voltage(const struct device *dev, uint8_t boost_output_voltage);
#endif //BOOST_EN

/**
 * @brief Enable a LED on LP5810_LP5811
 *
 * @param dev LED device structure
 * @param led LED number (0-3) to enable
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_enable_led(const struct device *dev, uint8_t led);

/**
 * @brief Disable a LED on LP5810_LP5811
 *
 * @param dev LED device structure
 * @param led LED number (0-3) to disable
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_disable_led(const struct device *dev, uint8_t led);

/**
 * @brief Set PWM frequency to 24KHz
 *
 * Configure the PWM frequency to 24KHz for LED brightness control
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_pwm_freq_24khz(const struct device *dev);

/**
 * @brief Set PWM frequency to 12KHz
 *
 * Configure the PWM frequency to 12KHz for LED brightness control
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_pwm_freq_12khz(const struct device *dev);

/**
 * @brief Set PWM curve type
 *
 * Configure the PWM dimming curve characteristics
 *
 * @param dev LED device structure
 * @param curve_type Curve type (linear or exponential)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_pwm_curve(const struct device *dev, uint8_t led, enum lp5810_lp5811_pwm_curve_type curve_type);

/**
 * @brief Set PWM phase alignment
 *
 * Configure how PWM pulses are aligned within the PWM period
 *
 * @param dev LED device structure
 * @param align Alignment type (forward, middle, or backward)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_pwm_align(const struct device *dev, uint8_t led, enum lp5810_lp5811_pwm_align align);

/**
 * @brief Set VSYNC mode as input
 *
 * Configure the VSYNC pin as input
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_vsync_input(const struct device *dev);

/**
 * @brief Set VSYNC mode as output
 *
 * Configure the VSYNC pin as output
 *
 * @param dev LED device structure
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_vsync_output(const struct device *dev);

/**
 * @brief Set line change time
 *
 * Configure the time taken to change lines during LED operation
 *
 * @param dev LED device structure
 * @param time Line change time
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_line_change_time(const struct device *dev, enum lp5810_lp5811_line_change_time time);

/**
 * @brief Set LED operating mode
 *
 * Configure a specific LED to operate in manual or autonomous mode
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param mode LED mode (manual or autonomous)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_mode(const struct device *dev, uint8_t led, enum lp5810_lp5811_led_mode mode);

/**
 * @brief Set LED short detection threshold
 *
 * Configure the voltage threshold for detecting LED shorts
 *
 * @param dev LED device structure
 * @param threshold LSD threshold value
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_lsd_threshold(const struct device *dev, enum lp5810_lp5811_lsd_threshold threshold);

/**
 * @brief Set LED short detection action
 *
 * Configure the action to take when an LED short is detected
 *
 * @param dev LED device structure
 * @param action LSD action (none or shutdown all)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_lsd_action(const struct device *dev, enum lp5810_lp5811_lsd_action action);

/**
 * @brief Set LED open detection action
 *
 * Configure the action to take when an LED open fault is detected
 *
 * @param dev LED device structure
 * @param action LOD action (none or shutdown sink)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_lod_action(const struct device *dev, enum lp5810_lp5811_lod_action action);

/**
 * @brief Set clamp mode
 *
 * Enable or disable the clamp feature
 *
 * @param dev LED device structure
 * @param mode Clamp mode (enable or disable)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_clamp_mode(const struct device *dev, enum lp5810_lp5811_clamp_mode mode);

/**
 * @brief Set clamp behavior
 *
 * Configure when the clamp feature is applied
 *
 * @param dev LED device structure
 * @param behavior Clamp behavior selection
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_clamp_behavior(const struct device *dev, enum lp5810_lp5811_clamp_behavior behavior);

/**
 * @brief Set clamp voltage
 *
 * Configure the voltage level for the clamp feature
 *
 * @param dev LED device structure
 * @param clamp_voltage Clamp voltage selection
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_clamp_voltage(const struct device *dev, enum lp5810_lp5811_clamp_voltage clamp_voltage);

/**
 * @brief Set analog dimming brightness
 *
 * Set LED brightness using analog dimming
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param brightness Brightness value (0-255)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_analog_dimming(const struct device *dev, uint32_t led, uint8_t brightness);

/**
 * @brief Set PWM dimming brightness
 *
 * Set LED brightness using PWM dimming
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param brightness Brightness value (0-255)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_set_pwm_dimming(const struct device *dev, uint32_t led, uint8_t brightness);

/**
 * @brief Set autonomous mode DC brightness
 *
 * Set LED brightness in autonomous mode using DC control
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param auto_dc Autonomous DC brightness value (0-255)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_auto_dc(const struct device *dev, uint8_t led, uint8_t auto_dc);

/**
 * @brief Set autonomous animation pause times
 *
 * Configure pause times for start and end of autonomous animation
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param pt_end End pause time
 * @param pt_start Start pause time
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_auto_pause_times(const struct device *dev, uint8_t led,
                                   enum lp5810_lp5811_aeu_slope_time pt_end, 
                                   enum lp5810_lp5811_aeu_slope_time pt_start);

/**
 * @brief Set animation engine playback settings
 *
 * Configure animation playback settings for a specific LED
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param playback_times Number of times to play the pattern
 * @param active_aeus Which animation engine units are active
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_auto_playback_settings(const struct device *dev, uint8_t led,
                                         enum lp5810_lp5811_auto_pattern_playback_times playback_times,
                                         enum lp5810_lp5811_led_active_aeus active_aeus);

/**
 * @brief Set animation engine PWM value
 *
 * Configure PWM value for a specific animation engine
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param engine Animation engine (1-3)
 * @param pwm PWM setting (1-5)
 * @param setting PWM value (0-255)
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_aeu_pwm(const struct device *dev, uint8_t led, 
                          enum lp5810_lp5811_animation_engine engine, 
                          enum lp5810_lp5811_animation_engine_pwm pwm, 
                          uint8_t setting);

/**
 * @brief Set animation engine T1/T2 slope times
 *
 * Configure T1 and T2 slope times for animation transitions
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param engine Animation engine (1-3)
 * @param t1 T1 slope time setting
 * @param t2 T2 slope time setting
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_aeu_t12(const struct device *dev, uint8_t led,
                          enum lp5810_lp5811_animation_engine engine, 
                          enum lp5810_lp5811_aeu_slope_time t1,
                          enum lp5810_lp5811_aeu_slope_time t2);

/**
 * @brief Set animation engine T3/T4 slope times
 *
 * Configure T3 and T4 slope times for animation transitions
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param engine Animation engine (1-3)
 * @param t3 T3 slope time setting
 * @param t4 T4 slope time setting
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_aeu_t34(const struct device *dev, uint8_t led,
                          enum lp5810_lp5811_animation_engine engine, 
                          enum lp5810_lp5811_aeu_slope_time t3,
                          enum lp5810_lp5811_aeu_slope_time t4);

/**
 * @brief Set animation engine playback time
 *
 * Configure playback time for a specific animation engine
 *
 * @param dev LED device structure
 * @param led LED channel (0-3)
 * @param engine Animation engine (1-3)
 * @param playback_time Playback time value
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_led_set_aeu_playback_time(const struct device *dev, uint8_t led,
                                    enum lp5810_lp5811_animation_engine engine,
                                    uint8_t playback_time);

/**
 * @brief Get thermal shutdown and configuration error status
 *
 * Read the thermal shutdown and configuration error status
 *
 * @param dev LED device structure
 * @param tsd_config_status Status structure to populate
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_get_tsd_config_status(const struct device *dev, struct tsd_config_status *tsd_config_status);

/**
 * @brief Get LED open detection status
 *
 * Read the LED open detection status for all LEDs
 *
 * @param dev LED device structure
 * @param lod_status Status structure to populate
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_get_lod_status(const struct device *dev, struct lod_status *lod_status);

/**
 * @brief Get LED short detection status
 *
 * Read the LED short detection status for all LEDs
 *
 * @param dev LED device structure
 * @param lsd_status Status structure to populate
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_get_lsd_status(const struct device *dev, struct lsd_status *lsd_status);

/**
 * @brief Get autonomous PWM values
 *
 * Read the autonomous PWM values for LEDs
 *
 * @param dev LED device structure
 * @param pwm PWM setting (1-4)
 * @param value Return value of PWM to populate
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_get_auto_pwm_value(const struct device *dev, uint8_t pwm, uint8_t *auto_pwm);

/**
 * @brief Get animation engine playback status
 *
 * Read the animation engine playback status
 *
 * @param dev LED device structure
 * @param aep_status Status structure to populate
 * @return 0 on success, or negative error code
 */
int lp5810_lp5811_get_aep_status(const struct device *dev, struct aep_status *aep_status);

#endif /* ZEPHYR_INCLUDE_DRIVERS_LED_LP5810_LP5811_H_ */
