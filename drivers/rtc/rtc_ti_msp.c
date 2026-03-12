/*
 * Copyright (c) 2025 Linumiz GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DT_DRV_COMPAT ti_msp_rtc

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_macro.h>
#include "rtc_utils.h"
#include <ti/driverlib/dl_rtc_common.h>

/*
 * initCalendar() writes 6 registers (SEC, MIN, HOUR, DAY, MON, YEAR).
 * Each write takes up to 3 RTCCLK cycles to propagate through the
 * clock-domain crossing. RTCCLK = LFCLK = 32.768 kHz (~30.5 us/cycle).
 * 6 registers x 3 cycles x 30.5 us = ~549 us; round up to 600 us.
 */
#define RTC_TI_MSP_WRITE_SETTLE_US 600U

#define RTC_TI_MSP_TIME_MASK                                                                       \
	(RTC_ALARM_TIME_MASK_SECOND | RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_HOUR |      \
	 RTC_ALARM_TIME_MASK_MONTHDAY | RTC_ALARM_TIME_MASK_MONTH | RTC_ALARM_TIME_MASK_YEAR |     \
	 RTC_ALARM_TIME_MASK_WEEKDAY)

/*
 * The MSP RTC_Common peripheral provides exactly two calendar alarms (ALARM1
 * and ALARM2). This is a fixed hardware constraint shared across all MSPM0 and
 * MSPM33 variants that use the RTC_Common IP. Supporting more than 2 alarms
 * would require a RTC IP with additional alarm registers.
 */
#if defined(CONFIG_RTC_ALARM)
#define RTC_TI_ALARM_1   0
#define RTC_TI_ALARM_2   1
#define RTC_TI_MAX_ALARM DT_INST_PROP(0, alarms_count)

#define RTC_TI_MSP_ALARM_MASK                                                                      \
	(RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_HOUR | RTC_ALARM_TIME_MASK_WEEKDAY |     \
	 RTC_ALARM_TIME_MASK_MONTHDAY)

BUILD_ASSERT((RTC_TI_MAX_ALARM != 0),
	     "CONFIG_RTC_ALARM is enabled, without setting alarms-count property");
BUILD_ASSERT((RTC_TI_MAX_ALARM <= 2),
	     "This driver supports at most 2 alarms; RTC hardware only has ALARM1 and ALARM2 registers");
#endif

struct rtc_ti_msp_config {
	RTC_Regs *regs;
#if defined(CONFIG_RTC_ALARM) || defined(CONFIG_RTC_UPDATE)
	void (*irq_config_func)(void);
#endif
	bool rtc_x;
};

#if defined(CONFIG_RTC_ALARM)
struct rtc_ti_msp_alarm {
	rtc_alarm_callback callback;
	void *user_data;
	uint16_t mask;
	bool is_pending;
};
#endif

struct rtc_ti_msp_data {
	struct k_spinlock lock;
	bool time_set;
#if defined(CONFIG_RTC_UPDATE)
	rtc_update_callback update_cb;
	void *update_cb_user_data;
#endif
#if defined(CONFIG_RTC_ALARM)
	struct rtc_ti_msp_alarm rtc_alarm[RTC_TI_MAX_ALARM];
#endif
};

static int rtc_ti_msp_set_time(const struct device *dev, const struct rtc_time *timeptr)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	struct rtc_ti_msp_data *data = dev->data;
	DL_RTC_Common_Calendar cal;

	if ((timeptr == NULL) || !rtc_utils_validate_rtc_time(timeptr, RTC_TI_MSP_TIME_MASK)) {
		return -EINVAL;
	}

	cal.seconds = timeptr->tm_sec;
	cal.minutes = timeptr->tm_min;
	cal.hours = timeptr->tm_hour;
	cal.dayOfWeek = timeptr->tm_wday;
	cal.dayOfMonth = timeptr->tm_mday;
	cal.month = timeptr->tm_mon + 1;
	cal.year = timeptr->tm_year + 1900;

	K_SPINLOCK(&data->lock) {
		DL_RTC_Common_initCalendar(cfg->regs, cal, DL_RTC_COMMON_FORMAT_BINARY);
		data->time_set = true;
	}

	k_busy_wait(RTC_TI_MSP_WRITE_SETTLE_US);

	return 0;
}

static int rtc_ti_msp_get_time(const struct device *dev, struct rtc_time *timeptr)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	struct rtc_ti_msp_data *data = dev->data;
	DL_RTC_Common_Calendar cal;
	int ret = 0;

	if (timeptr == NULL) {
		return -EINVAL;
	}

	K_SPINLOCK(&data->lock) {
		if (!data->time_set) {
			ret = -ENODATA;
			K_SPINLOCK_BREAK;
		}

		/*
		 * The RTC has a ~3.9 ms keep-out window before each second
		 * boundary where calendar registers are in transition. Poll
		 * the RTCRDY status bit to avoid reading during that window.
		 */
		while (!DL_RTC_Common_isSafeToRead(cfg->regs)) {
			k_busy_wait(100);
		}

		cal = DL_RTC_Common_getCalendarTime(cfg->regs);
	}

	if (ret) {
		return ret;
	}

	timeptr->tm_sec = cal.seconds;
	timeptr->tm_min = cal.minutes;
	timeptr->tm_hour = cal.hours;
	timeptr->tm_mday = cal.dayOfMonth;
	timeptr->tm_mon = cal.month - 1;
	timeptr->tm_year = cal.year - 1900;
	timeptr->tm_wday = cal.dayOfWeek;
	timeptr->tm_yday = -1;
	timeptr->tm_nsec = 0;
	timeptr->tm_isdst = -1;

	return 0;
}

#if defined(CONFIG_RTC_ALARM)
static int rtc_ti_msp_alarm_get_supported_fields(const struct device *dev, uint16_t id,
						 uint16_t *mask)
{
	ARG_UNUSED(dev);

	if (id >= RTC_TI_MAX_ALARM) {
		return -EINVAL;
	}

	if (mask == NULL) {
		return -EINVAL;
	}

	*mask = RTC_TI_MSP_ALARM_MASK;

	return 0;
}

static inline void rtc_ti_msp_set_alarm(const struct device *dev, uint16_t id, uint16_t mask,
					const struct rtc_time *timeptr)
{
	const struct rtc_ti_msp_config *cfg = dev->config;

	switch (id) {
	case RTC_TI_ALARM_1:
		DL_RTC_Common_disableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM1);
		if (mask & RTC_ALARM_TIME_MASK_MINUTE) {
			DL_RTC_Common_setAlarm1MinutesBinary(cfg->regs, timeptr->tm_min);
			DL_RTC_Common_enableAlarm1MinutesBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_HOUR) {
			DL_RTC_Common_setAlarm1HoursBinary(cfg->regs, timeptr->tm_hour);
			DL_RTC_Common_enableAlarm1HoursBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_WEEKDAY) {
			DL_RTC_Common_setAlarm1DayOfWeekBinary(cfg->regs, timeptr->tm_wday);
			DL_RTC_Common_enableAlarm1DayOfWeekBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_MONTHDAY) {
			DL_RTC_Common_setAlarm1DayOfMonthBinary(cfg->regs, timeptr->tm_mday);
			DL_RTC_Common_enableAlarm1DayOfMonthBinary(cfg->regs);
		}
		DL_RTC_Common_enableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM1);
		break;
	case RTC_TI_ALARM_2:
		DL_RTC_Common_disableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM2);
		if (mask & RTC_ALARM_TIME_MASK_MINUTE) {
			DL_RTC_Common_setAlarm2MinutesBinary(cfg->regs, timeptr->tm_min);
			DL_RTC_Common_enableAlarm2MinutesBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_HOUR) {
			DL_RTC_Common_setAlarm2HoursBinary(cfg->regs, timeptr->tm_hour);
			DL_RTC_Common_enableAlarm2HoursBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_WEEKDAY) {
			DL_RTC_Common_setAlarm2DayOfWeekBinary(cfg->regs, timeptr->tm_wday);
			DL_RTC_Common_enableAlarm2DayOfWeekBinary(cfg->regs);
		}
		if (mask & RTC_ALARM_TIME_MASK_MONTHDAY) {
			DL_RTC_Common_setAlarm2DayOfMonthBinary(cfg->regs, timeptr->tm_mday);
			DL_RTC_Common_enableAlarm2DayOfMonthBinary(cfg->regs);
		}
		DL_RTC_Common_enableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM2);
		break;
	default:
		break;
	}
}

static inline void rtc_ti_msp_clear_alarm(const struct device *dev, uint16_t id)
{
	const struct rtc_ti_msp_config *cfg = dev->config;

	switch (id) {
	case RTC_TI_ALARM_1:
		DL_RTC_Common_disableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM1);
		cfg->regs->A1MIN = 0x00;
		cfg->regs->A1HOUR = 0x00;
		cfg->regs->A1DAY = 0x00;
		break;
	case RTC_TI_ALARM_2:
		DL_RTC_Common_disableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM2);
		cfg->regs->A2MIN = 0x00;
		cfg->regs->A2HOUR = 0x00;
		cfg->regs->A2DAY = 0x00;
		break;
	default:
		break;
	}
}

static int rtc_ti_msp_alarm_set_time(const struct device *dev, uint16_t id, uint16_t mask,
				     const struct rtc_time *timeptr)
{
	struct rtc_ti_msp_data *data = dev->data;

	if (id >= RTC_TI_MAX_ALARM) {
		return -EINVAL;
	}

	if (mask & ~RTC_TI_MSP_ALARM_MASK) {
		return -EINVAL;
	}

	if (mask == 0) {
		K_SPINLOCK(&data->lock) {
			rtc_ti_msp_clear_alarm(dev, id);
			data->rtc_alarm[id].mask = 0;
			data->rtc_alarm[id].is_pending = false;
		}
		return 0;
	}

	if (timeptr == NULL) {
		return -EINVAL;
	}

	if (!rtc_utils_validate_rtc_time(timeptr, mask)) {
		return -EINVAL;
	}

	K_SPINLOCK(&data->lock) {
		rtc_ti_msp_clear_alarm(dev, id);
		rtc_ti_msp_set_alarm(dev, id, mask, timeptr);

		data->rtc_alarm[id].mask = mask;
		data->rtc_alarm[id].is_pending = false;
	}

	return 0;
}

static uint16_t rtc_ti_msp_get_alarm(const struct device *dev, uint16_t id,
				     struct rtc_time *timeptr)
{
	uint16_t return_mask = 0;
	uint16_t alarm_mask;
	const struct rtc_ti_msp_config *cfg = dev->config;
	struct rtc_ti_msp_data *data = dev->data;

	alarm_mask = data->rtc_alarm[id].mask;

	switch (id) {
	case RTC_TI_ALARM_1:
		if (alarm_mask & RTC_ALARM_TIME_MASK_MINUTE) {
			timeptr->tm_min = DL_RTC_Common_getAlarm1MinutesBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_MINUTE;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_HOUR) {
			timeptr->tm_hour = DL_RTC_Common_getAlarm1HoursBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_HOUR;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_WEEKDAY) {
			timeptr->tm_wday = DL_RTC_Common_getAlarm1DayOfWeekBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_WEEKDAY;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_MONTHDAY) {
			timeptr->tm_mday = DL_RTC_Common_getAlarm1DayOfMonthBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_MONTHDAY;
		}
		break;
	case RTC_TI_ALARM_2:
		if (alarm_mask & RTC_ALARM_TIME_MASK_MINUTE) {
			timeptr->tm_min = DL_RTC_Common_getAlarm2MinutesBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_MINUTE;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_HOUR) {
			timeptr->tm_hour = DL_RTC_Common_getAlarm2HoursBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_HOUR;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_WEEKDAY) {
			timeptr->tm_wday = DL_RTC_Common_getAlarm2DayOfWeekBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_WEEKDAY;
		}
		if (alarm_mask & RTC_ALARM_TIME_MASK_MONTHDAY) {
			timeptr->tm_mday = DL_RTC_Common_getAlarm2DayOfMonthBinary(cfg->regs);
			return_mask |= RTC_ALARM_TIME_MASK_MONTHDAY;
		}
		break;
	default:
		break;
	}

	return return_mask;
}

static int rtc_ti_msp_alarm_get_time(const struct device *dev, uint16_t id, uint16_t *mask,
				     struct rtc_time *timeptr)
{
	struct rtc_ti_msp_data *data = dev->data;

	if (timeptr == NULL || mask == NULL) {
		return -EINVAL;
	}

	if (id >= RTC_TI_MAX_ALARM) {
		return -EINVAL;
	}

	memset(timeptr, 0, sizeof(*timeptr));
	timeptr->tm_isdst = -1;

	K_SPINLOCK(&data->lock) {
		*mask = rtc_ti_msp_get_alarm(dev, id, timeptr);
	}

	return 0;
}

static int rtc_ti_msp_alarm_set_callback(const struct device *dev, uint16_t id,
					 rtc_alarm_callback callback, void *user_data)
{
	struct rtc_ti_msp_data *data = dev->data;

	if (id >= RTC_TI_MAX_ALARM) {
		return -EINVAL;
	}

	K_SPINLOCK(&data->lock) {
		data->rtc_alarm[id].callback = callback;
		data->rtc_alarm[id].user_data = user_data;
	}

	return 0;
}

static int rtc_ti_msp_alarm_is_pending(const struct device *dev, uint16_t id)
{
	int ret;
	struct rtc_ti_msp_data *data = dev->data;

	if (id >= RTC_TI_MAX_ALARM) {
		return -EINVAL;
	}

	K_SPINLOCK(&data->lock) {
		ret = data->rtc_alarm[id].is_pending ? 1 : 0;
		data->rtc_alarm[id].is_pending = false;
	}

	return ret;
}
#endif /* CONFIG_RTC_ALARM */

#if defined(CONFIG_RTC_UPDATE)
static int rtc_ti_msp_update_set_callback(const struct device *dev, rtc_update_callback callback,
					  void *user_data)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	struct rtc_ti_msp_data *data = dev->data;

	K_SPINLOCK(&data->lock) {
		data->update_cb = callback;
		data->update_cb_user_data = user_data;

		if (callback) {
			DL_RTC_Common_enableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_READY);
		} else {
			DL_RTC_Common_disableInterrupt(cfg->regs, DL_RTC_COMMON_INTERRUPT_READY);
		}
	}

	return 0;
}
#endif /* CONFIG_RTC_UPDATE */

#if defined(CONFIG_RTC_ALARM) || defined(CONFIG_RTC_UPDATE)
static void rtc_ti_msp_isr(const struct device *dev)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	struct rtc_ti_msp_data *data = dev->data;
	DL_RTC_COMMON_IIDX iidx;

#if defined(CONFIG_RTC_ALARM)
	rtc_alarm_callback alarm_cb = NULL;
	void *alarm_cb_data = NULL;
	uint8_t alarm_id = 0;
#endif /* CONFIG_RTC_ALARM */

#if defined(CONFIG_RTC_UPDATE)
	rtc_update_callback update_cb = NULL;
	void *update_cb_data = NULL;
#endif /* CONFIG_RTC_UPDATE */
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	iidx = DL_RTC_Common_getPendingInterrupt(cfg->regs);

	switch (iidx) {
#if defined(CONFIG_RTC_ALARM)
	case DL_RTC_COMMON_IIDX_ALARM1:
	case DL_RTC_COMMON_IIDX_ALARM2: {
		uint8_t id = (iidx == DL_RTC_COMMON_IIDX_ALARM1) ? RTC_TI_ALARM_1 : RTC_TI_ALARM_2;
		struct rtc_ti_msp_alarm *alarm = &data->rtc_alarm[id];

		if (alarm->callback) {
			alarm_cb = alarm->callback;
			alarm_cb_data = alarm->user_data;
			alarm_id = id;
		} else {
			alarm->is_pending = true;
		}
		break;
	}
#endif /* CONFIG_RTC_ALARM */

#if defined(CONFIG_RTC_UPDATE)
	case DL_RTC_COMMON_IIDX_READY:
		update_cb = data->update_cb;
		update_cb_data = data->update_cb_user_data;
		break;
#endif /* CONFIG_RTC_UPDATE */
	default:
		break;
	}

	k_spin_unlock(&data->lock, key);

#if defined(CONFIG_RTC_ALARM)
	if (alarm_cb != NULL) {
		alarm_cb(dev, alarm_id, alarm_cb_data);
	}
#endif /* CONFIG_RTC_ALARM */

#if defined(CONFIG_RTC_UPDATE)
	if (update_cb != NULL) {
		update_cb(dev, update_cb_data);
	}
#endif /* CONFIG_RTC_UPDATE */
}
#endif /* CONFIG_RTC_ALARM || CONFIG_RTC_UPDATE */

#if defined(CONFIG_RTC_CALIBRATION)
#define RTC_TI_MSP_MAX_CAL_PPM 240
#define RTC_TI_MSP_PPB_PER_PPM 1000
#define RTC_TI_MSP_MAX_CAL_PPB (RTC_TI_MSP_MAX_CAL_PPM * RTC_TI_MSP_PPB_PER_PPM)

static int rtc_ti_msp_set_calibration(const struct device *dev, int32_t calibration)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	DL_RTC_COMMON_OFFSET_CALIBRATION_SIGN sign;
	int32_t ppm;

	if (calibration > RTC_TI_MSP_MAX_CAL_PPB || calibration < -RTC_TI_MSP_MAX_CAL_PPB) {
		return -EINVAL;
	}

	ppm = calibration / RTC_TI_MSP_PPB_PER_PPM;

	if (ppm >= 0) {
		sign = DL_RTC_COMMON_OFFSET_CALIBRATION_SIGN_UP;
	} else {
		sign = DL_RTC_COMMON_OFFSET_CALIBRATION_SIGN_DOWN;
		ppm = -ppm;
	}

	while (!DL_RTC_Common_isReadyToCalibrate(cfg->regs)) {
		k_busy_wait(100);
	}

	DL_RTC_Common_setOffsetCalibrationAdjValue(cfg->regs, sign, (uint8_t)ppm);

	/*
	 * Verify the write was accepted. If missed the window,
	 * wait for the next one and retry once before returning an error.
	 */
	if (!DL_RTC_Common_isCalibrationWriteResultOK(cfg->regs)) {
		while (!DL_RTC_Common_isReadyToCalibrate(cfg->regs)) {
			k_busy_wait(100);
		}
		DL_RTC_Common_setOffsetCalibrationAdjValue(cfg->regs, sign, (uint8_t)ppm);
		if (!DL_RTC_Common_isCalibrationWriteResultOK(cfg->regs)) {
			return -EIO;
		}
	}

	return 0;
}

static int rtc_ti_msp_get_calibration(const struct device *dev, int32_t *calibration)
{
	const struct rtc_ti_msp_config *cfg = dev->config;
	int32_t ppm;

	if (calibration == NULL) {
		return -EINVAL;
	}

	ppm = cfg->regs->CAL & RTC_CAL_RTCOCALX_MASK;

	if (DL_RTC_Common_getOffsetCalibrationSign(cfg->regs) ==
	    DL_RTC_COMMON_OFFSET_CALIBRATION_SIGN_DOWN) {
		ppm = -ppm;
	}

	*calibration = ppm * RTC_TI_MSP_PPB_PER_PPM;

	return 0;
}
#endif /* CONFIG_RTC_CALIBRATION */

static int rtc_ti_msp_init(const struct device *dev)
{
	const struct rtc_ti_msp_config *cfg = dev->config;

	if (!cfg->rtc_x) {
		/* Enable power to RTC module (not needed for LFSS-resident RTC) */
		if (!DL_RTC_Common_isPowerEnabled(cfg->regs)) {
			DL_RTC_Common_enablePower(cfg->regs);
			k_busy_wait(100);
		}
	}

	DL_RTC_Common_enableClockControl(cfg->regs);
	DL_RTC_Common_setClockFormat(cfg->regs, DL_RTC_COMMON_FORMAT_BINARY);

#if defined(CONFIG_RTC_ALARM)
	{
		struct rtc_ti_msp_data *data = dev->data;

		for (int i = 0; i < RTC_TI_MAX_ALARM; i++) {
			data->rtc_alarm[i].callback = NULL;
			data->rtc_alarm[i].user_data = NULL;
			data->rtc_alarm[i].mask = 0;
			data->rtc_alarm[i].is_pending = false;
		}

		DL_RTC_Common_clearInterruptStatus(cfg->regs,
						   DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM1 |
							   DL_RTC_COMMON_INTERRUPT_CALENDAR_ALARM2);
	}
#endif

#if defined(CONFIG_RTC_ALARM) || defined(CONFIG_RTC_UPDATE)
	cfg->irq_config_func();
#endif

	return 0;
}

static DEVICE_API(rtc, rtc_ti_msp_driver_api) = {
	.set_time = rtc_ti_msp_set_time,
	.get_time = rtc_ti_msp_get_time,
#if defined(CONFIG_RTC_ALARM)
	.alarm_set_time = rtc_ti_msp_alarm_set_time,
	.alarm_get_time = rtc_ti_msp_alarm_get_time,
	.alarm_is_pending = rtc_ti_msp_alarm_is_pending,
	.alarm_set_callback = rtc_ti_msp_alarm_set_callback,
	.alarm_get_supported_fields = rtc_ti_msp_alarm_get_supported_fields,
#endif /* CONFIG_RTC_ALARM */
#if defined(CONFIG_RTC_UPDATE)
	.update_set_callback = rtc_ti_msp_update_set_callback,
#endif /* CONFIG_RTC_UPDATE */
#if defined(CONFIG_RTC_CALIBRATION)
	.set_calibration = rtc_ti_msp_set_calibration,
	.get_calibration = rtc_ti_msp_get_calibration,
#endif /* CONFIG_RTC_CALIBRATION */
};

#if defined(CONFIG_RTC_ALARM) || defined(CONFIG_RTC_UPDATE)
#define RTC_TI_MSP_IRQ_FUNC(n)                                                                     \
	static void ti_msp_config_irq_##n(void)                                                    \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), rtc_ti_msp_isr,             \
			    DEVICE_DT_INST_GET(n), 0);                                             \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}
#define RTC_TI_MSP_IRQ_CFG(n) .irq_config_func = ti_msp_config_irq_##n,
#else
#define RTC_TI_MSP_IRQ_FUNC(n)
#define RTC_TI_MSP_IRQ_CFG(n)
#endif

#define RTC_TI_MSP_DEVICE_INIT(n)                                                                  \
	RTC_TI_MSP_IRQ_FUNC(n)                                                                     \
                                                                                                   \
	static struct rtc_ti_msp_data rtc_data_##n;                                                \
                                                                                                   \
	static const struct rtc_ti_msp_config rtc_config_##n = {                                   \
		.regs = (RTC_Regs *)DT_INST_REG_ADDR(n),                                           \
		.rtc_x = DT_INST_PROP(n, ti_rtc_x),                                                \
		RTC_TI_MSP_IRQ_CFG(n)};                                                            \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &rtc_ti_msp_init, NULL, &rtc_data_##n, &rtc_config_##n,           \
			      PRE_KERNEL_1, CONFIG_RTC_INIT_PRIORITY, &rtc_ti_msp_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RTC_TI_MSP_DEVICE_INIT);
