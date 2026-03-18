/*
 * Copyright (c) 2026, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include "bq27z8xx_common.h"

LOG_MODULE_REGISTER(test_bq27z8xx_stress, LOG_LEVEL_INF);

/* Number of back-to-back read cycles to execute */
#define STRESS_ITERATIONS 100

struct bq27z8xx_stress_fixture {
	const struct device             *dev;
	struct bq27z8xx_device_type      ref_device_type;
	struct bq27z8xx_chem_id          ref_chem_id;
	struct bq27z8xx_firmware_version ref_fw_version;
};

static void *bq27z8xx_stress_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_stress_fixture fixture;

	fixture.dev = DEVICE_DT_GET_ANY(ti_bq27z855);
	k_object_access_all_grant(fixture.dev);

	zassert_true(device_is_ready(fixture.dev), "BQ27Z8XX device not ready");

	/* Capture reference values on the first read — subsequent reads are
	 * compared against these to detect corruption or inconsistency. */
	zassert_ok(fuel_gauge_get_buffer_prop(fixture.dev,
					      BQ27Z8XX_BUFFER_PROP_DEVICETYPE,
					      &fixture.ref_device_type,
					      sizeof(fixture.ref_device_type)),
		   "Initial DeviceType read failed");

	zassert_ok(fuel_gauge_get_buffer_prop(fixture.dev,
					      BQ27Z8XX_BUFFER_PROP_CHEMID,
					      &fixture.ref_chem_id,
					      sizeof(fixture.ref_chem_id)),
		   "Initial ChemId read failed");

	zassert_ok(fuel_gauge_get_buffer_prop(fixture.dev,
					      BQ27Z8XX_BUFFER_PROP_FIRMWAREVERSION,
					      &fixture.ref_fw_version,
					      sizeof(fixture.ref_fw_version)),
		   "Initial FirmwareVersion read failed");

	return &fixture;
}

/*
 * Reads DeviceType, ChemId, and FirmwareVersion back-to-back STRESS_ITERATIONS
 * times with no delays.  Each read must succeed and return values consistent
 * with the reference captured in setup.
 */
ZTEST_USER_F(bq27z8xx_stress, test_rapid_mac_reads)
{
	for (int i = 0; i < STRESS_ITERATIONS; i++) {
		struct bq27z8xx_device_type      device_type;
		struct bq27z8xx_chem_id          chem_id;
		struct bq27z8xx_firmware_version fw_version;

		zassert_ok(fuel_gauge_get_buffer_prop(fixture->dev,
						      BQ27Z8XX_BUFFER_PROP_DEVICETYPE,
						      &device_type, sizeof(device_type)),
			   "DeviceType read failed at iteration %d", i);

		zassert_ok(fuel_gauge_get_buffer_prop(fixture->dev,
						      BQ27Z8XX_BUFFER_PROP_CHEMID,
						      &chem_id, sizeof(chem_id)),
			   "ChemId read failed at iteration %d", i);

		zassert_ok(fuel_gauge_get_buffer_prop(fixture->dev,
						      BQ27Z8XX_BUFFER_PROP_FIRMWAREVERSION,
						      &fw_version, sizeof(fw_version)),
			   "FirmwareVersion read failed at iteration %d", i);

		zassert_mem_equal(&device_type, &fixture->ref_device_type,
				  sizeof(device_type),
				  "DeviceType mismatch at iteration %d", i);

		zassert_mem_equal(&chem_id, &fixture->ref_chem_id,
				  sizeof(chem_id),
				  "ChemId mismatch at iteration %d", i);

		zassert_mem_equal(&fw_version, &fixture->ref_fw_version,
				  sizeof(fw_version),
				  "FirmwareVersion mismatch at iteration %d", i);
	}

	LOG_INF("Completed %d iterations without error", STRESS_ITERATIONS);
}

ZTEST_SUITE(bq27z8xx_stress, NULL, bq27z8xx_stress_setup, NULL, NULL, NULL);
