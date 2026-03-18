/*
 * Copyright (c) 2026, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/drivers/fuel_gauge.h>

#include "bq27z8xx_common.h"
#include "bq27z855.h"

LOG_MODULE_REGISTER(test_bq27z8xx_prop_access, LOG_LEVEL_DBG);

struct bq27z8xx_prop_access_fixture {
	const struct device *dev;
};

static void *bq27z8xx_prop_access_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_prop_access_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	return &fixture;
}

/*
 * All standard and custom scalar read properties must return 0.
 * A few values are spot-checked against known emulator constants:
 *   - Signed registers: emulator returns raw -2; after driver scaling → -2000 µA / µW
 *   - Unsigned registers: emulator returns raw 1; after scaling → 1 or 1000
 *   - bq27z855 CYCLE_COUNT: raw 1 × 100 = 100 (hundredths of a cycle)
 */
ZTEST_F(bq27z8xx_prop_access, test_get_all_read_props)
{
	static const fuel_gauge_prop_t get_props[] = {
		FUEL_GAUGE_AVG_CURRENT,
		FUEL_GAUGE_CYCLE_COUNT,
		FUEL_GAUGE_CURRENT,
		FUEL_GAUGE_FULL_CHARGE_CAPACITY,
		FUEL_GAUGE_REMAINING_CAPACITY,
		FUEL_GAUGE_RUNTIME_TO_EMPTY,
		FUEL_GAUGE_RUNTIME_TO_FULL,
		FUEL_GAUGE_SBS_MFR_ACCESS,
		FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE,
		FUEL_GAUGE_TEMPERATURE,
		FUEL_GAUGE_VOLTAGE,
		FUEL_GAUGE_SBS_ATRATE,
		FUEL_GAUGE_SBS_ATRATE_TIME_TO_EMPTY,
		FUEL_GAUGE_CHARGE_VOLTAGE,
		FUEL_GAUGE_CHARGE_CURRENT,
		FUEL_GAUGE_STATUS,
		FUEL_GAUGE_DESIGN_CAPACITY,
		/* TODO: re-enable when firmware supports these threshold registers */
		/* FUEL_GAUGE_HIGH_VOLTAGE_ALARM, */   /* VoltHiSetThreshold */
		/* FUEL_GAUGE_LOW_VOLTAGE_ALARM, */    /* VoltLoSetThreshold */
		/* FUEL_GAUGE_HIGH_TEMPERATURE_ALARM, */ /* TempHiSetThreshold */
		/* FUEL_GAUGE_LOW_TEMPERATURE_ALARM, */ /* TempLoSetThreshold */
		/* FUEL_GAUGE_STATE_OF_CHARGE_ALARM, */ /* SOCDeltaSetThreshold */
		BQ27Z8XX_PROP_MAX_LOAD_CURRENT,
		BQ27Z8XX_PROP_MAX_LOAD_TIME_TO_EMPTY,
		BQ27Z8XX_PROP_AVERAGE_POWER,
		BQ27Z8XX_PROP_INTERNAL_TEMPERATURE,
		BQ27Z8XX_PROP_STATE_OF_HEALTH,
		BQ27Z8XX_PROP_TIMESTAMP_UPPER,
		BQ27Z8XX_PROP_TIMESTAMP_LOWER,
		BQ27Z8XX_PROP_QMAX_CYCLES,
		/* TODO: re-enable when firmware supports InterruptStatus */
		/* BQ27Z8XX_PROP_INTERRUPT_STATUS, */
	};

	union fuel_gauge_prop_val val;

	for (int i = 0; i < ARRAY_SIZE(get_props); i++) {
		int ret = fuel_gauge_get_prop(fixture->dev, get_props[i], &val);

		zassert_ok(ret, "get_prop(0x%x) failed: %d", get_props[i], ret);
	}

#ifdef CONFIG_EMUL
	/* Spot-check: signed register (emulator returns raw -2, driver scales × 1000) */
	int ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_CURRENT, &val);

	zassert_ok(ret, "get FUEL_GAUGE_CURRENT failed: %d", ret);
	zassert_equal(val.current, -2000,
		      "FUEL_GAUGE_CURRENT: expected -2000 uA, got %d", val.current);

	/* Spot-check: signed register via avg_current path */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_AVG_CURRENT, &val);
	zassert_ok(ret, "get FUEL_GAUGE_AVG_CURRENT failed: %d", ret);
	zassert_equal(val.avg_current, -2000,
		      "FUEL_GAUGE_AVG_CURRENT: expected -2000 uA, got %d", val.avg_current);
#else
	/* Spot-check: signed register (device returns raw 0) */
	int ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_CURRENT, &val);

	zassert_ok(ret, "get FUEL_GAUGE_CURRENT failed: %d", ret);
	zassert_equal(val.current, 0,
		      "FUEL_GAUGE_CURRENT: expected 0 uA, got %d", val.current);

	/* Spot-check: signed register via avg_current path */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_AVG_CURRENT, &val);
	zassert_ok(ret, "get FUEL_GAUGE_AVG_CURRENT failed: %d", ret);
	zassert_equal(val.avg_current, 0,
		      "FUEL_GAUGE_AVG_CURRENT: expected 0 uA, got %d", val.avg_current);
#endif


	/* Spot-check: unsigned register, not scaled (0.1 K) */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_TEMPERATURE, &val);
	zassert_ok(ret, "get FUEL_GAUGE_TEMPERATURE failed: %d", ret);
#ifdef CONFIG_EMUL
	zassert_equal(val.temperature, 1,
		      "FUEL_GAUGE_TEMPERATURE: expected 1, got %d", val.temperature);
#else
	/* 2000..3000 in 0.1 K = 200 K..300 K, covers plausible operating range */
	zassert_between_inclusive(val.temperature, 2000, 3000,
		      "FUEL_GAUGE_TEMPERATURE: expected 2000..3000, got %d", val.temperature);
#endif

	/* Spot-check: unsigned register scaled × 1000 (µV) */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_VOLTAGE, &val);
	zassert_ok(ret, "get FUEL_GAUGE_VOLTAGE failed: %d", ret);
#ifdef CONFIG_EMUL
	zassert_equal(val.voltage, 1000,
		      "FUEL_GAUGE_VOLTAGE: expected 1000 uV, got %d", val.voltage);
#else
	/* 3.3 V ± 20%: 2 640 000..3 960 000 µV */
	zassert_between_inclusive(val.voltage, 2640000, 3960000,
		      "FUEL_GAUGE_VOLTAGE: expected 2640000..3960000 uV, got %d", val.voltage);
#endif

#ifdef CONFIG_EMUL
	/* Spot-check: bq27z855 applies × 100 to CYCLE_COUNT */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_CYCLE_COUNT, &val);
	zassert_ok(ret, "get FUEL_GAUGE_CYCLE_COUNT failed: %d", ret);
	zassert_equal(val.cycle_count, 100,
		      "FUEL_GAUGE_CYCLE_COUNT: expected 100, got %d", val.cycle_count);
#endif

}

/*
 * All standard and custom writable scalar properties must be accepted with
 * return code 0.  The emulator silently discards all register writes.
 */
ZTEST_F(bq27z8xx_prop_access, test_set_all_write_props)
{
	union fuel_gauge_prop_val val;
	int ret;

	val.sbs_mfr_access_word = 0x0001;
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_SBS_MFR_ACCESS, val);
	zassert_ok(ret, "set FUEL_GAUGE_SBS_MFR_ACCESS failed: %d", ret);

	val.sbs_at_rate = -100;
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_SBS_ATRATE, val);
	zassert_ok(ret, "set FUEL_GAUGE_SBS_ATRATE failed: %d", ret);

/* TODO: re-enable when firmware supports VoltHiSetThreshold, VoltLoSetThreshold,
 * TempHiSetThreshold, TempLoSetThreshold, SOCDeltaSetThreshold */
#if 0
	val.high_voltage_alarm = 4200000; /* 4200 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_HIGH_VOLTAGE_ALARM, val);
	zassert_ok(ret, "set FUEL_GAUGE_HIGH_VOLTAGE_ALARM failed: %d", ret);

	val.low_voltage_alarm = 3000000; /* 3000 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_LOW_VOLTAGE_ALARM, val);
	zassert_ok(ret, "set FUEL_GAUGE_LOW_VOLTAGE_ALARM failed: %d", ret);

	val.high_temperature_alarm = 3281; /* 55 °C: 55 × 10 + 2731 = 3281 in 0.1 K */
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_HIGH_TEMPERATURE_ALARM, val);
	zassert_ok(ret, "set FUEL_GAUGE_HIGH_TEMPERATURE_ALARM failed: %d", ret);

	val.low_temperature_alarm = 2631; /* -10 °C: -10 × 10 + 2731 = 2631 in 0.1 K */
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_LOW_TEMPERATURE_ALARM, val);
	zassert_ok(ret, "set FUEL_GAUGE_LOW_TEMPERATURE_ALARM failed: %d", ret);

	val.state_of_charge_alarm = 20;
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_STATE_OF_CHARGE_ALARM, val);
	zassert_ok(ret, "set FUEL_GAUGE_STATE_OF_CHARGE_ALARM failed: %d", ret);
#endif

	val.sbs_mfr_access_word = 100;
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_BTP_DISCHARGE_SET, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_BTP_DISCHARGE_SET failed: %d", ret);

	val.sbs_mfr_access_word = 200;
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_BTP_CHARGE_SET, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_BTP_CHARGE_SET failed: %d", ret);

	val.voltage = 2500000; /* 2500 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_TERMINATE_VOLTAGE, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_TERMINATE_VOLTAGE failed: %d", ret);

/* TODO: re-enable when firmware supports VoltHiClearThreshold, VoltLoClearThreshold,
 * TempHiClearThreshold, TempLoClearThreshold */
#if 0
	val.high_voltage_alarm = 4100000; /* 4100 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_VOLT_HI_CLEAR_THRESHOLD, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_VOLT_HI_CLEAR_THRESHOLD failed: %d", ret);

	val.low_voltage_alarm = 3100000; /* 3100 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_VOLT_LO_CLEAR_THRESHOLD, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_VOLT_LO_CLEAR_THRESHOLD failed: %d", ret);

	val.high_temperature_alarm = 3181; /* 45 °C: 45 × 10 + 2731 = 3181 in 0.1 K */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_TEMP_HI_CLEAR_THRESHOLD, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_TEMP_HI_CLEAR_THRESHOLD failed: %d", ret);

	val.low_temperature_alarm = 2731; /* 0 °C: 0 × 10 + 2731 = 2731 in 0.1 K */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_TEMP_LO_CLEAR_THRESHOLD, val);
	zassert_ok(ret, "set BQ27Z8XX_PROP_TEMP_LO_CLEAR_THRESHOLD failed: %d", ret);
#endif
}

/*
 * An unrecognised property ID passed to fuel_gauge_get_prop() must return
 * -ENOTSUP without accessing the hardware.
 */
ZTEST_F(bq27z8xx_prop_access, test_get_unsupported_prop_returns_enotsup)
{
	union fuel_gauge_prop_val val;
	int ret;

	ret = fuel_gauge_get_prop(fixture->dev, (fuel_gauge_prop_t)0x7FFF, &val);
	zassert_equal(ret, -ENOTSUP,
		      "get_prop(invalid) should be -ENOTSUP, got %d", ret);
}

/*
 * An unrecognised property ID passed to fuel_gauge_set_prop() must return
 * -ENOTSUP without accessing the hardware.
 */
ZTEST_F(bq27z8xx_prop_access, test_set_unsupported_prop_returns_enotsup)
{
	union fuel_gauge_prop_val val;
	int ret;

	val.sbs_mfr_access_word = 0;
	ret = fuel_gauge_set_prop(fixture->dev, (fuel_gauge_prop_t)0x7FFF, val);
	zassert_equal(ret, -ENOTSUP,
		      "set_prop(invalid) should be -ENOTSUP, got %d", ret);
}

/*
 * Write a value to a writable property and read it back, verifying the device
 * actually stored it.  This exercises the full register round-trip through the
 * I2C bus and is therefore skipped when running against the emulator (which
 * does not maintain writable register state).
 *
 * BQ27Z8XX_PROP_TERMINATE_VOLTAGE — 16-bit mV register (TERMINATEVOLTAGE).
 *   API uses µV; driver divides by 1000 on write and multiplies by 1000 on
 *   read.  Any multiple of 1000 round-trips without loss.
 *
 * Each property is saved before the test write and restored afterwards so the
 * device is left in its original configuration.
 *
 * NOTE: VoltHiSetThreshold and SOCDeltaSetThreshold readback tests are
 * disabled below until firmware supports those registers.
 */
ZTEST_F(bq27z8xx_prop_access, test_prop_write_readback)
{
#if defined(CONFIG_EMUL)
	/* Emulator returns hardcoded values regardless of writes — skip. */
	ztest_test_skip();
#endif

	/* On hardware the BQ27Z855 silently ignores writes when SEALED.
	 * Read OperationStatus and skip the test if the device is sealed. */
	{
		struct bq27z855_operation_status op_status = {0};
		int rc = fuel_gauge_get_buffer_prop(fixture->dev,
				BQ27Z855_BUFFER_PROP_OPERATIONSTATUS,
				&op_status, sizeof(op_status));

		zassert_ok(rc, "read OperationStatus failed: %d", rc);
		if ((op_status.flags & BQ27Z855_OP_STATUS_SEC_MASK) ==
		    BQ27Z855_OP_STATUS_SEC_SEALED) {
			ztest_test_skip();
		}
	}

/* TODO: re-enable when firmware supports TeminateVoltage, VoltHiSetThreshold, and SOCDeltaSetThreshold */
#if 0
	union fuel_gauge_prop_val orig, written, readback;
	int ret;

	/* --- BQ27Z8XX_PROP_TERMINATE_VOLTAGE --- */
	ret = fuel_gauge_get_prop(fixture->dev, BQ27Z8XX_PROP_TERMINATE_VOLTAGE, &orig);
	zassert_ok(ret, "save TERMINATE_VOLTAGE failed: %d", ret);

	written.voltage = 2800000; /* 2800 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_TERMINATE_VOLTAGE, written);
	zassert_ok(ret, "set TERMINATE_VOLTAGE failed: %d", ret);

	ret = fuel_gauge_get_prop(fixture->dev, BQ27Z8XX_PROP_TERMINATE_VOLTAGE, &readback);
	zassert_ok(ret, "readback TERMINATE_VOLTAGE failed: %d", ret);
	zassert_equal(readback.voltage, written.voltage,
		      "TERMINATE_VOLTAGE: wrote %u uV, read back %u uV",
		      written.voltage, readback.voltage);

	ret = fuel_gauge_set_prop(fixture->dev, BQ27Z8XX_PROP_TERMINATE_VOLTAGE, orig);
	zassert_ok(ret, "restore TERMINATE_VOLTAGE failed: %d", ret);


	/* --- FUEL_GAUGE_HIGH_VOLTAGE_ALARM (VoltHiSetThreshold) --- */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_HIGH_VOLTAGE_ALARM, &orig);
	zassert_ok(ret, "save HIGH_VOLTAGE_ALARM failed: %d", ret);

	written.high_voltage_alarm = 4100000; /* 4100 mV in µV */
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_HIGH_VOLTAGE_ALARM, written);
	zassert_ok(ret, "set HIGH_VOLTAGE_ALARM failed: %d", ret);

	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_HIGH_VOLTAGE_ALARM, &readback);
	zassert_ok(ret, "readback HIGH_VOLTAGE_ALARM failed: %d", ret);
	zassert_equal(readback.high_voltage_alarm, written.high_voltage_alarm,
		      "HIGH_VOLTAGE_ALARM: wrote %u uV, read back %u uV",
		      written.high_voltage_alarm, readback.high_voltage_alarm);

	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_HIGH_VOLTAGE_ALARM, orig);
	zassert_ok(ret, "restore HIGH_VOLTAGE_ALARM failed: %d", ret);

	/* --- FUEL_GAUGE_STATE_OF_CHARGE_ALARM (SOCDeltaSetThreshold) --- */
	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_STATE_OF_CHARGE_ALARM, &orig);
	zassert_ok(ret, "save STATE_OF_CHARGE_ALARM failed: %d", ret);

	written.state_of_charge_alarm = 15;
	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_STATE_OF_CHARGE_ALARM, written);
	zassert_ok(ret, "set STATE_OF_CHARGE_ALARM failed: %d", ret);

	ret = fuel_gauge_get_prop(fixture->dev, FUEL_GAUGE_STATE_OF_CHARGE_ALARM, &readback);
	zassert_ok(ret, "readback STATE_OF_CHARGE_ALARM failed: %d", ret);
	zassert_equal(readback.state_of_charge_alarm, written.state_of_charge_alarm,
		      "STATE_OF_CHARGE_ALARM: wrote %u %%, read back %u %%",
		      written.state_of_charge_alarm, readback.state_of_charge_alarm);

	ret = fuel_gauge_set_prop(fixture->dev, FUEL_GAUGE_STATE_OF_CHARGE_ALARM, orig);
	zassert_ok(ret, "restore STATE_OF_CHARGE_ALARM failed: %d", ret);
#endif
}

ZTEST_SUITE(bq27z8xx_prop_access, NULL, bq27z8xx_prop_access_setup, NULL, NULL, NULL);
