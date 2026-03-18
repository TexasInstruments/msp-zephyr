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

#include "bq27z855.h"

LOG_MODULE_REGISTER(test_bq27z8xx_buffer_props, LOG_LEVEL_DBG);

struct bq27z8xx_buffer_props_fixture {
	const struct device *dev;
};

static void *bq27z8xx_buffer_props_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_buffer_props_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	return &fixture;
}

/*
 * Common buffer properties — all defined in bq27z8xx_common.h.
 * Each must return 0 and report a length byte equal to sizeof(struct) - 1.
 *
 * DEVICETYPE and FIRMWAREVERSION are omitted here because they are already
 * exercised in the i2c_stress_test suite; this test targets the six
 * properties that have no prior coverage.
 */
ZTEST_F(bq27z8xx_buffer_props, test_common_buffer_props)
{
	int ret;

	/* HardwareVersion: 1-byte length + 2-byte hw_version = 3 bytes total */
	struct bq27z8xx_hardware_version hw_ver;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_HARDWAREVERSION,
					 &hw_ver, sizeof(hw_ver));
	zassert_ok(ret, "HARDWAREVERSION failed: %d", ret);
	zassert_equal(hw_ver.length, sizeof(hw_ver) - 1,
		      "HARDWAREVERSION length: expected %zu got %u",
		      sizeof(hw_ver) - 1, hw_ver.length);

	/* IFSignature: 1-byte length + 2-byte signature = 3 bytes */
	struct bq27z8xx_if_signature if_sig;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_IFSIGNATURE,
					 &if_sig, sizeof(if_sig));
	zassert_ok(ret, "IFSIGNATURE failed: %d", ret);
	zassert_equal(if_sig.length, sizeof(if_sig) - 1,
		      "IFSIGNATURE length: expected %zu got %u",
		      sizeof(if_sig) - 1, if_sig.length);

	/* StaticDFSignature: 1-byte length + 2-byte signature = 3 bytes */
	struct bq27z8xx_static_df_signature sdf_sig;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_STATICDFSIGNATURE,
					 &sdf_sig, sizeof(sdf_sig));
	zassert_ok(ret, "STATICDFSIGNATURE failed: %d", ret);
	zassert_equal(sdf_sig.length, sizeof(sdf_sig) - 1,
		      "STATICDFSIGNATURE length: expected %zu got %u",
		      sizeof(sdf_sig) - 1, sdf_sig.length);

	/* StaticChemDFSignature: 1-byte length + 2-byte signature = 3 bytes */
	struct bq27z8xx_static_chem_df_signature scdf_sig;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_STATICCHEMDFSIGNATURE,
					 &scdf_sig, sizeof(scdf_sig));
	zassert_ok(ret, "STATICCHEMDFSIGNATURE failed: %d", ret);
	zassert_equal(scdf_sig.length, sizeof(scdf_sig) - 1,
		      "STATICCHEMDFSIGNATURE length: expected %zu got %u",
		      sizeof(scdf_sig) - 1, scdf_sig.length);

	/* AllDFSignature: 1-byte length + 2-byte signature = 3 bytes */
	struct bq27z8xx_all_df_signature adf_sig;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_ALLDFSIGNATURE,
					 &adf_sig, sizeof(adf_sig));
	zassert_ok(ret, "ALLDFSIGNATURE failed: %d", ret);
	zassert_equal(adf_sig.length, sizeof(adf_sig) - 1,
		      "ALLDFSIGNATURE length: expected %zu got %u",
		      sizeof(adf_sig) - 1, adf_sig.length);

	/*
	 * SecurityKeys (0x0035) is a privileged MAC command: it is only
	 * accessible when the device is in UNSEALED or FULL ACCESS mode.
	 * On a sealed device (power-on default) the command is silently
	 * ignored — the 0x3E buffer retains the previous command's echo,
	 * causing the driver's echo check to fail with -EIO.
	 * Accept 0 (unsealed hardware or emulator) or -EIO (sealed hardware).
	 */
	struct bq27z8xx_security_keys sec_keys;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_SECURITYKEYS,
					 &sec_keys, sizeof(sec_keys));
	zassert_true(ret == 0 || ret == -EIO,
		     "SECURITYKEYS: expected 0 or -EIO (sealed), got %d", ret);
	if (ret == 0) {
		zassert_equal(sec_keys.length, sizeof(sec_keys) - 1,
			      "SECURITYKEYS length: expected %zu got %u",
			      sizeof(sec_keys) - 1, sec_keys.length);
	}
}

/*
 * bq27z855-specific buffer properties — all 12 defined in bq27z855.h.
 * Each must return 0 and report a length byte equal to sizeof(struct) - 1.
 *
 * NOLOADREMCAP, CHARGINGSTATUSEXT, ACCUMULATIONCHARGETHRESHOLD, and
 * ACCUMULATIONDISCHARGETHRESHOLD are marked "TODO: Check support" in the
 * header (hardware verification pending) but the emulator handles them,
 * so they are expected to return 0 here.
 */
ZTEST_F(bq27z8xx_buffer_props, test_855_buffer_props)
{
	int ret;

	/* SafetyAlert: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_safety_alert safety_alert;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_SAFETYALERT,
					 &safety_alert, sizeof(safety_alert));
	zassert_ok(ret, "SAFETYALERT failed: %d", ret);
	zassert_equal(safety_alert.length, sizeof(safety_alert) - 1,
		      "SAFETYALERT length: expected %zu got %u",
		      sizeof(safety_alert) - 1, safety_alert.length);

	/* SafetyStatus: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_safety_status safety_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_SAFETYSTATUS,
					 &safety_status, sizeof(safety_status));
	zassert_ok(ret, "SAFETYSTATUS failed: %d", ret);
	zassert_equal(safety_status.length, sizeof(safety_status) - 1,
		      "SAFETYSTATUS length: expected %zu got %u",
		      sizeof(safety_status) - 1, safety_status.length);

	/* PFAlert: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_pf_alert pf_alert;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_PFALERT,
					 &pf_alert, sizeof(pf_alert));
	zassert_ok(ret, "PFALERT failed: %d", ret);
	zassert_equal(pf_alert.length, sizeof(pf_alert) - 1,
		      "PFALERT length: expected %zu got %u",
		      sizeof(pf_alert) - 1, pf_alert.length);

	/* PFStatus: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_pf_status pf_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_PFSTATUS,
					 &pf_status, sizeof(pf_status));
	zassert_ok(ret, "PFSTATUS failed: %d", ret);
	zassert_equal(pf_status.length, sizeof(pf_status) - 1,
		      "PFSTATUS length: expected %zu got %u",
		      sizeof(pf_status) - 1, pf_status.length);

	/* OperationStatus: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_operation_status op_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_OPERATIONSTATUS,
					 &op_status, sizeof(op_status));
	zassert_ok(ret, "OPERATIONSTATUS failed: %d", ret);
	zassert_equal(op_status.length, sizeof(op_status) - 1,
		      "OPERATIONSTATUS length: expected %zu got %u",
		      sizeof(op_status) - 1, op_status.length);

	/* ChargingStatus: 1-byte length + 5-byte flags = 6 bytes */
	struct bq27z855_charging_status chg_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_CHARGINGSTATUS,
					 &chg_status, sizeof(chg_status));
	zassert_ok(ret, "CHARGINGSTATUS failed: %d", ret);
	zassert_equal(chg_status.length, sizeof(chg_status) - 1,
		      "CHARGINGSTATUS length: expected %zu got %u",
		      sizeof(chg_status) - 1, chg_status.length);

	/* GaugingStatus: 1-byte length + 5-byte flags = 6 bytes */
	struct bq27z855_gauging_status gau_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_GAUGINGSTATUS,
					 &gau_status, sizeof(gau_status));
	zassert_ok(ret, "GAUGINGSTATUS failed: %d", ret);
	zassert_equal(gau_status.length, sizeof(gau_status) - 1,
		      "GAUGINGSTATUS length: expected %zu got %u",
		      sizeof(gau_status) - 1, gau_status.length);

	/* ManufacturingStatus: 1-byte length + 2-byte flags = 3 bytes */
	struct bq27z855_manufacturing_status mfg_status;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_MANUFACTURINGSTATUS,
					 &mfg_status, sizeof(mfg_status));
	zassert_ok(ret, "MANUFACTURINGSTATUS failed: %d", ret);
	zassert_equal(mfg_status.length, sizeof(mfg_status) - 1,
		      "MANUFACTURINGSTATUS length: expected %zu got %u",
		      sizeof(mfg_status) - 1, mfg_status.length);

	/*
	 * The following four props are marked "TODO: Check support" in bq27z855.h
	 * (hardware verification pending).  The emulator supports them, so 0 is
	 * expected here; -ENOTSUP would also be acceptable if hardware support is
	 * removed in a future revision.
	 */

	/* NoLoadRemCap: 1-byte length + 2-byte capacity = 3 bytes */
	struct bq27z855_no_load_rem_cap no_load_cap;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_NOLOADREMCAP,
					 &no_load_cap, sizeof(no_load_cap));
	zassert_true(ret == 0 || ret == -ENOTSUP,
		     "NOLOADREMCAP: expected 0 or -ENOTSUP, got %d", ret);
	if (ret == 0) {
		zassert_equal(no_load_cap.length, sizeof(no_load_cap) - 1,
			      "NOLOADREMCAP length: expected %zu got %u",
			      sizeof(no_load_cap) - 1, no_load_cap.length);
	}

	/* ChargingStatusExt: 1-byte length + 4-byte flags = 5 bytes */
	struct bq27z855_charging_status_ext chg_status_ext;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z855_BUFFER_PROP_CHARGINGSTATUSEXT,
					 &chg_status_ext, sizeof(chg_status_ext));
	zassert_true(ret == 0 || ret == -ENOTSUP,
		     "CHARGINGSTATUSEXT: expected 0 or -ENOTSUP, got %d", ret);
	if (ret == 0) {
		zassert_equal(chg_status_ext.length, sizeof(chg_status_ext) - 1,
			      "CHARGINGSTATUSEXT length: expected %zu got %u",
			      sizeof(chg_status_ext) - 1, chg_status_ext.length);
	}

	/* AccumulationChargeThreshold: 1-byte length + 2-byte threshold = 3 bytes */
	struct bq27z855_accumulation_charge_threshold acc_chg_thr;

	ret = fuel_gauge_get_buffer_prop(fixture->dev,
					 BQ27Z855_BUFFER_PROP_ACCUMULATIONCHARGETHRESHOLD,
					 &acc_chg_thr, sizeof(acc_chg_thr));
	zassert_true(ret == 0 || ret == -ENOTSUP,
		     "ACCUMULATIONCHARGETHRESHOLD: expected 0 or -ENOTSUP, got %d", ret);
	if (ret == 0) {
		zassert_equal(acc_chg_thr.length, sizeof(acc_chg_thr) - 1,
			      "ACCUMULATIONCHARGETHRESHOLD length: expected %zu got %u",
			      sizeof(acc_chg_thr) - 1, acc_chg_thr.length);
	}

	/* AccumulationDischargeThreshold: 1-byte length + 2-byte threshold = 3 bytes */
	struct bq27z855_accumulation_discharge_threshold acc_dsg_thr;

	ret = fuel_gauge_get_buffer_prop(fixture->dev,
					 BQ27Z855_BUFFER_PROP_ACCUMULATIONDISCHARGETHRESHOLD,
					 &acc_dsg_thr, sizeof(acc_dsg_thr));
	zassert_true(ret == 0 || ret == -ENOTSUP,
		     "ACCUMULATIONDISCHARGETHRESHOLD: expected 0 or -ENOTSUP, got %d", ret);
	if (ret == 0) {
		zassert_equal(acc_dsg_thr.length, sizeof(acc_dsg_thr) - 1,
			      "ACCUMULATIONDISCHARGETHRESHOLD length: expected %zu got %u",
			      sizeof(acc_dsg_thr) - 1, acc_dsg_thr.length);
	}
}

/*
 * Passing a dst_len that does not exactly match the expected struct size must
 * return -EINVAL.  Uses BQ27Z8XX_BUFFER_PROP_DEVICETYPE (struct size = 3) with
 * dst_len = 1 to trigger the size mismatch.
 */
ZTEST_F(bq27z8xx_buffer_props, test_buffer_prop_wrong_dst_len_returns_einval)
{
	uint8_t small_buf[1];
	int ret;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, BQ27Z8XX_BUFFER_PROP_DEVICETYPE,
					 small_buf, sizeof(small_buf));
	zassert_equal(ret, -EINVAL,
		      "get_buffer_prop(DEVICETYPE, dst_len=1) should be -EINVAL, got %d", ret);
}

/*
 * An unrecognised property ID must be rejected with -ENOTSUP by both the
 * variant handler and the common handler.
 */
ZTEST_F(bq27z8xx_buffer_props, test_buffer_prop_unsupported_id_returns_enotsup)
{
	uint8_t buf[4];
	int ret;

	ret = fuel_gauge_get_buffer_prop(fixture->dev, (fuel_gauge_prop_t)0x7FFF,
					 buf, sizeof(buf));
	zassert_equal(ret, -ENOTSUP,
		      "get_buffer_prop(invalid) should be -ENOTSUP, got %d", ret);
}

ZTEST_SUITE(bq27z8xx_buffer_props, NULL, bq27z8xx_buffer_props_setup, NULL, NULL, NULL);
