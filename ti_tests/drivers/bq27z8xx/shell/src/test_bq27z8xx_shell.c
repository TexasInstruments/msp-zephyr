/*
 * Copyright (c) 2026, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Shell command tests for the BQ27Z8XX driver, BQ27Z855 variant.
 *
 * All tests run against the native_sim emulator (CONFIG_EMUL_BQ27Z8XX=y) using
 * the shell_dummy backend.  The emulator returns all-zero payloads with correct
 * length and checksum for every supported MAC command.
 *
 * Test categories
 * ---------------
 * 1. Input validation  — bad device name, invalid address, wrong key count.
 * 2. Smoke tests       — every command returns 0 on the emulator.
 * 3. Output content    — key strings / section headers appear in captured output.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

/* DT node label in the native_sim overlay */
#define SHELL_DEV_NAME "bq27z855@55"

struct bq27z8xx_shell_fixture {
	const struct shell *sh;
};

/* ---------------------------------------------------------------------------
 * Suite setup / before
 * ---------------------------------------------------------------------------
 */

static void *bq27z8xx_shell_setup(void)
{
	static ZTEST_DMEM struct bq27z8xx_shell_fixture fixture;

	fixture.sh = shell_backend_dummy_get_ptr();
	zassert_not_null(fixture.sh, "shell_dummy backend not available");

	/* Yield to the shell thread so it transitions to SHELL_STATE_ACTIVE.
	 * Without this, shell_fprintf returns early (shell.c: state != ACTIVE)
	 * and all shell_print calls are silent no-ops. */
	k_sleep(K_MSEC(100));

	const struct device *dev = device_get_binding(SHELL_DEV_NAME);

	zassert_not_null(dev, "BQ27Z8XX device not found: " SHELL_DEV_NAME);
	zassert_true(device_is_ready(dev), "BQ27Z8XX device not ready");

	return &fixture;
}

static void bq27z8xx_shell_before(void *arg)
{
	const struct bq27z8xx_shell_fixture *f = arg;

	shell_backend_dummy_clear_output(f->sh);
}

/* ---------------------------------------------------------------------------
 * Helper
 * ---------------------------------------------------------------------------
 */

/*
 * Execute @p cmd via the dummy shell backend and return the captured output.
 * The caller receives the command return code in *ret_out.
 * The returned pointer is valid until the next call to run_cmd() or
 * shell_backend_dummy_clear_output().
 */
static const char *run_cmd(const struct bq27z8xx_shell_fixture *f, const char *cmd, int *ret_out)
{
	size_t len;

	shell_backend_dummy_clear_output(f->sh);
	*ret_out = shell_execute_cmd(f->sh, cmd);
	return shell_backend_dummy_get_output(f->sh, &len);
}

/* ---------------------------------------------------------------------------
 * 1. Input validation tests
 * ---------------------------------------------------------------------------
 */

/*
 * An unrecognised device name must return -ENODEV without issuing any I2C
 * traffic.
 */
ZTEST_F(bq27z8xx_shell, test_unknown_device_returns_enodev)
{
	int ret;

	run_cmd(fixture, "bq27z8xx status no_such_device", &ret);
	zassert_equal(ret, -ENODEV, "expected -ENODEV for unknown device, got %d", ret);
}

/*
 * A non-hex address string for the 'read' command must be rejected.
 */
ZTEST_F(bq27z8xx_shell, test_read_invalid_hex_address)
{
	int ret;

	run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " xyz", &ret);
	zassert_true(ret < 0, "expected error for invalid address, got %d", ret);
}

/*
 * 'unseal' requires either zero or two key arguments; supplying exactly one
 * key must return -EINVAL.
 */
ZTEST_F(bq27z8xx_shell, test_unseal_single_key_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx unseal " SHELL_DEV_NAME " 0414", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for single key, got %d", ret);
}

/*
 * 'full_access' has the same two-or-none key rule.
 */
ZTEST_F(bq27z8xx_shell, test_full_access_single_key_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx full_access " SHELL_DEV_NAME " ffff", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for single key, got %d", ret);
}

/* ---------------------------------------------------------------------------
 * 2. Smoke tests — every command completes with return code 0
 * ---------------------------------------------------------------------------
 */

ZTEST_F(bq27z8xx_shell, test_status_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx status " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx status failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_security_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx security " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx security failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_lifetime_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx lifetime " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx lifetime failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_serial_completes)
{
	int ret;

	/*
	 * SerialNumber MAC (0x004E) is not modelled by the emulator; the shell
	 * falls back to ManufacturerInfo (0x0070) which IS modelled.  The
	 * emulator returns zeros → copy_ascii finds no printable bytes → the
	 * shell prints "Serial: <empty>" and returns 0.
	 */
	run_cmd(fixture, "bq27z8xx serial " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx serial failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_read_mac_address_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 0001", &ret);
	zassert_ok(ret, "bq27z8xx read (MAC addr) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_read_df_address_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 4000", &ret);
	zassert_ok(ret, "bq27z8xx read (DF addr) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_seal_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx seal " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx seal failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_unseal_default_keys_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx unseal " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx unseal (default keys) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_unseal_explicit_keys_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx unseal " SHELL_DEV_NAME " 0414 3672", &ret);
	zassert_ok(ret, "bq27z8xx unseal (explicit keys) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_full_access_default_keys_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx full_access " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx full_access (default keys) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_full_access_explicit_keys_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx full_access " SHELL_DEV_NAME " ffff ffff", &ret);
	zassert_ok(ret, "bq27z8xx full_access (explicit keys) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_read_multi_block_completes)
{
	int ret;

	/*
	 * Read 3 consecutive 32-byte DF blocks starting at 0x4000.
	 * Each block steps by MAC_DATA_LEN (0x20), so addresses are
	 * 0x4000, 0x4020, 0x4040 — all within the DF range (0x4000–0x5FFF)
	 * supported by the emulator.
	 */
	run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 4000 3", &ret);
	zassert_ok(ret, "bq27z8xx read (multi-block) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_read_zero_blocks_returns_error)
{
	int ret;

	run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 0060 0", &ret);
	zassert_true(ret < 0, "expected error for zero block count, got %d", ret);
}

/* ---------------------------------------------------------------------------
 * 3. Output content tests
 * ---------------------------------------------------------------------------
 */

/*
 * The status header must identify the connected part as BQ27Z855 so that the
 * operator can confirm which variant is running.
 */
ZTEST_F(bq27z8xx_shell, test_status_output_identifies_z855_variant)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx status " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "status failed: %d", ret);
	zassert_not_null(strstr(out, "BQ27Z855"),
			 "output missing BQ27Z855 variant label:\n%s", out);
}

/*
 * Every major status section must appear in the output.  An absent header
 * means the corresponding MAC read silently failed (emulator gap) and the
 * decoder was never called.
 */
ZTEST_F(bq27z8xx_shell, test_status_output_has_required_section_headers)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx status " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "status failed: %d", ret);
	zassert_not_null(strstr(out, "BatteryStatus:"),       "missing BatteryStatus");
	zassert_not_null(strstr(out, "SafetyAlert:"),         "missing SafetyAlert");
	zassert_not_null(strstr(out, "SafetyStatus:"),        "missing SafetyStatus");
	zassert_not_null(strstr(out, "OperationStatus:"),     "missing OperationStatus");
	zassert_not_null(strstr(out, "ChargingStatus:"),      "missing ChargingStatus");
	zassert_not_null(strstr(out, "GaugingStatus:"),       "missing GaugingStatus");
	zassert_not_null(strstr(out, "ManufacturingStatus:"), "missing ManufacturingStatus");
	zassert_not_null(strstr(out, "DAStatus1:"),           "missing DAStatus1");
	zassert_not_null(strstr(out, "DAStatus2:"),           "missing DAStatus2");
}

/*
 * The BQ27Z855 uses "GaugeStatus" labels for MAC 0x0073–0x0075; the BQ27Z758
 * uses "ITStatus" for the same addresses.  Confirm the right label appears
 * and the wrong one does not.
 */
ZTEST_F(bq27z8xx_shell, test_status_output_uses_z855_gaugestatus_labels)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx status " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "status failed: %d", ret);
	zassert_not_null(strstr(out, "GaugeStatus1:"), "missing GaugeStatus1");
	zassert_not_null(strstr(out, "GaugeStatus2:"), "missing GaugeStatus2");
	zassert_not_null(strstr(out, "GaugeStatus3:"), "missing GaugeStatus3");
	/* Z855 must not fall back to the Z758 ITStatus label */
	zassert_is_null(strstr(out, "ITStatus1:"),
			"unexpected ITStatus1 label (wrong variant path)");
}

/*
 * The security command must report the current mode.  Verify the "Security:"
 * field and that one of the four known mode labels is present.  On the
 * emulator, OperationStatus is all-zero so SEC[9:8] = 0b00 = "Reserved".  On
 * hardware the device may be Sealed, Unsealed, or Full Access depending on
 * prior test execution, so any valid label is acceptable.
 */
ZTEST_F(bq27z8xx_shell, test_security_output_has_mode_field)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx security " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "security failed: %d", ret);
	zassert_not_null(strstr(out, "Security:"), "missing Security field");
	zassert_true(strstr(out, "Reserved") || strstr(out, "Full Access") ||
			     strstr(out, "Unsealed") || strstr(out, "Sealed"),
		     "expected a security mode label in output:\n%s", out);
}

/*
 * The lifetime dump must include representative field names from different
 * lifetime blocks so that we know the block table is being iterated.
 */
ZTEST_F(bq27z8xx_shell, test_lifetime_output_has_known_field_names)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx lifetime " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "lifetime failed: %d", ret);
	/* Block 0 field — present on both variants */
	zassert_not_null(strstr(out, "Cell 1 Max Voltage"),
			 "missing 'Cell 1 Max Voltage' (block 0)");
	/*
	 * Block 2/3 field — label differs by variant:
	 *   Z855: "Total FW Runtime" (block 3 = index 2)
	 *   Z758: "Total Fw Runtime" (block 3 = index 2)
	 * Both share the common prefix "Total F"; check that.
	 */
	zassert_not_null(strstr(out, "Total F"),
			 "missing 'Total F...' runtime field (block 2/3)");
	/*
	 * RSOC histogram field — label differs by variant:
	 *   Z855: "Time In LFT_UT RSOC A" (block 6)
	 *   Z758: "Time In UT RSOC A"     (block 6)
	 * Both share the common suffix "RSOC A"; check that.
	 */
	zassert_not_null(strstr(out, "RSOC A"),
			 "missing 'RSOC A' histogram field (block 5/6)");
}

/*
 * The serial command must print a line starting with "Serial" regardless of
 * whether the device has a programmed serial number.  The exact format varies:
 *   "Serial: <empty>"                  — empty ManufacturerInfo
 *   "Serial (ManufacturerInfo): <str>" — non-empty ManufacturerInfo
 *   "Serial (MAC 0x004E): <str>"       — BQ27Z758 serial number field
 * All forms share the "Serial" prefix; check that rather than "Serial:".
 */
ZTEST_F(bq27z8xx_shell, test_serial_output_has_serial_label)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx serial " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "serial failed: %d", ret);
	zassert_not_null(strstr(out, "Serial"), "missing Serial label");
}

/*
 * The 'read' command labels MAC-range addresses as "MAC" and data-flash-range
 * addresses as "DF" in the hexdump header.
 */
ZTEST_F(bq27z8xx_shell, test_read_uses_mac_label_for_mac_range)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 0001", &ret);

	zassert_ok(ret, "read (MAC) failed: %d", ret);
	zassert_not_null(strstr(out, "MAC 0x0001 (len="),
			 "missing 'MAC 0x0001 (len=' header");
}

ZTEST_F(bq27z8xx_shell, test_read_uses_df_label_for_df_range)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx read " SHELL_DEV_NAME " 4000", &ret);

	zassert_ok(ret, "read (DF) failed: %d", ret);
	zassert_not_null(strstr(out, "DF 0x4000 (len="),
			 "missing 'DF 0x4000 (len=' header");
}

ZTEST_F(bq27z8xx_shell, test_ra_table_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx ra_table " SHELL_DEV_NAME, &ret);
	zassert_ok(ret, "bq27z8xx ra_table failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_ra_table_tsv_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx ra_table " SHELL_DEV_NAME " tsv", &ret);
	zassert_ok(ret, "bq27z8xx ra_table tsv failed: %d", ret);
}

/*
 * The ra_table output must contain the top-level header and both block
 * prefixes, confirming that both Cell0 and xCell0 DF blocks were read.
 */
ZTEST_F(bq27z8xx_shell, test_ra_table_output_has_header_and_both_blocks)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx ra_table " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "ra_table failed: %d", ret);
	zassert_not_null(strstr(out, "Ra Table (Data Flash)"), "missing header");
	zassert_not_null(strstr(out, "Cell0"),  "missing Cell0 block");
	zassert_not_null(strstr(out, "xCell0"), "missing xCell0 block");
}

/*
 * Every block must contain the flag field and at least the first resistance
 * value, and the output must include the mOhm unit label produced by the
 * raw→milliohm conversion.
 */
ZTEST_F(bq27z8xx_shell, test_ra_table_output_has_flag_and_resistance_values)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx ra_table " SHELL_DEV_NAME, &ret);

	zassert_ok(ret, "ra_table failed: %d", ret);
	zassert_not_null(strstr(out, "R_a flag"), "missing R_a flag field");
	zassert_not_null(strstr(out, "R_a 0"),    "missing R_a 0 field");
	zassert_not_null(strstr(out, "mOhm"),     "missing mOhm unit label");
}

/*
 * In TSV mode the first data line must be the column header "Label\tRaw\tmOhm"
 * and subsequent data lines must use tab separators rather than the normal
 * colon/space formatting.
 */
ZTEST_F(bq27z8xx_shell, test_ra_table_tsv_output_format)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx ra_table " SHELL_DEV_NAME " tsv", &ret);

	zassert_ok(ret, "ra_table tsv failed: %d", ret);
	zassert_not_null(strstr(out, "Label\tRaw\tmOhm"), "missing TSV column header");
	/* Data lines use tabs: "Cell0 R_a 0\t<raw>\t<mohm>" */
	zassert_not_null(strstr(out, "Cell0 R_a 0\t"), "missing tab-separated Cell0 R_a 0");
	zassert_not_null(strstr(out, "xCell0 R_a 0\t"), "missing tab-separated xCell0 R_a 0");
}

/* ---------------------------------------------------------------------------
 * reg_read — input validation
 * ---------------------------------------------------------------------------
 */

/*
 * "zz" is not a valid hex byte; shell_strtoul must set err and the handler
 * must return -EINVAL before issuing any I2C traffic.
 */
ZTEST_F(bq27z8xx_shell, test_reg_read_invalid_register_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx reg_read " SHELL_DEV_NAME " zz", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for non-hex register, got %d", ret);
}

/*
 * A byte count of zero is outside the valid range of 1–32 and must be
 * rejected before issuing any I2C traffic.
 */
ZTEST_F(bq27z8xx_shell, test_reg_read_invalid_bytecount_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx reg_read " SHELL_DEV_NAME " 08 0", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for zero byte count, got %d", ret);
}

/* ---------------------------------------------------------------------------
 * reg_read — smoke and output
 * ---------------------------------------------------------------------------
 */

ZTEST_F(bq27z8xx_shell, test_reg_read_completes)
{
	int ret;

	/* Register 0x08 = VOLTAGE; emulator returns a predictable value. */
	run_cmd(fixture, "bq27z8xx reg_read " SHELL_DEV_NAME " 08", &ret);
	zassert_ok(ret, "bq27z8xx reg_read failed: %d", ret);
}

/*
 * The output must contain the "REG 0x<reg>:" prefix produced by the
 * handler's shell_print call.
 */
ZTEST_F(bq27z8xx_shell, test_reg_read_output_format)
{
	int ret;
	const char *out = run_cmd(fixture, "bq27z8xx reg_read " SHELL_DEV_NAME " 08", &ret);

	zassert_ok(ret, "reg_read failed: %d", ret);
	zassert_not_null(strstr(out, "REG 0x08:"),
			 "output missing 'REG 0x08:' prefix:\n%s", out);
}

/* ---------------------------------------------------------------------------
 * reg_write — input validation
 * ---------------------------------------------------------------------------
 */

ZTEST_F(bq27z8xx_shell, test_reg_write_invalid_register_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx reg_write " SHELL_DEV_NAME " zz 01", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for non-hex register, got %d", ret);
}

/*
 * "gg" is not a valid hex byte; parse_hex_bytes() must reject it with
 * -EINVAL before any I2C write is issued.
 */
ZTEST_F(bq27z8xx_shell, test_reg_write_invalid_data_byte_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx reg_write " SHELL_DEV_NAME " 3e gg", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for non-hex data byte, got %d", ret);
}

/* ---------------------------------------------------------------------------
 * reg_write — smoke and output
 * ---------------------------------------------------------------------------
 */

ZTEST_F(bq27z8xx_shell, test_reg_write_completes)
{
	int ret;

	/* Write 0x54 0x00 to register 0x3E (ALTMANUFACTURERACCESS). */
	run_cmd(fixture, "bq27z8xx reg_write " SHELL_DEV_NAME " 3e 54 00", &ret);
	zassert_ok(ret, "bq27z8xx reg_write failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_reg_write_output_format)
{
	int ret;
	const char *out =
		run_cmd(fixture, "bq27z8xx reg_write " SHELL_DEV_NAME " 3e 54 00", &ret);

	zassert_ok(ret, "reg_write failed: %d", ret);
	zassert_not_null(strstr(out, "Wrote 2 byte(s) to REG 0x3e."),
			 "output missing write confirmation:\n%s", out);
}

/* ---------------------------------------------------------------------------
 * mac_write — input validation
 * ---------------------------------------------------------------------------
 */

/*
 * "zzzz" is not valid hex; shell_strtoul must set err and the handler must
 * return -EINVAL before calling bq27z8xx_write_mac().
 */
ZTEST_F(bq27z8xx_shell, test_mac_write_invalid_command_returns_einval)
{
	int ret;

	run_cmd(fixture, "bq27z8xx mac_write " SHELL_DEV_NAME " zzzz", &ret);
	zassert_equal(ret, -EINVAL, "expected -EINVAL for non-hex command, got %d", ret);
}

/* ---------------------------------------------------------------------------
 * mac_write — smoke and output
 * ---------------------------------------------------------------------------
 */

ZTEST_F(bq27z8xx_shell, test_mac_write_no_data_completes)
{
	/*
	 * bq27z8xx_write_mac() with data_len=0 writes to MACDATASUM (0x60)
	 * without a prior MACDATA (0x40) write.  The real device NACKs this
	 * because the block-commit registers are only writable when a data
	 * transfer to 0x40 has preceded them.  The emulator accepts all writes
	 * unconditionally, so this path is only testable there.
	 */
	if (!IS_ENABLED(CONFIG_EMUL)) {
		ztest_test_skip();
	}

	int ret;

	/* 0x0021 = GAUGING control command — zero-payload MAC write. */
	run_cmd(fixture, "bq27z8xx mac_write " SHELL_DEV_NAME " 0021", &ret);
	zassert_ok(ret, "bq27z8xx mac_write (no data) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_mac_write_with_data_completes)
{
	int ret;

	run_cmd(fixture, "bq27z8xx mac_write " SHELL_DEV_NAME " 0044 01 02 03", &ret);
	zassert_ok(ret, "bq27z8xx mac_write (with data) failed: %d", ret);
}

ZTEST_F(bq27z8xx_shell, test_mac_write_output_format_no_data)
{
	/* Same hardware limitation as test_mac_write_no_data_completes. */
	if (!IS_ENABLED(CONFIG_EMUL)) {
		ztest_test_skip();
	}

	int ret;
	const char *out =
		run_cmd(fixture, "bq27z8xx mac_write " SHELL_DEV_NAME " 0021", &ret);

	zassert_ok(ret, "mac_write failed: %d", ret);
	zassert_not_null(strstr(out, "MAC write 0x0021 sent (0 data byte(s))."),
			 "output missing zero-payload confirmation:\n%s", out);
}

ZTEST_F(bq27z8xx_shell, test_mac_write_output_format_with_data)
{
	int ret;
	const char *out =
		run_cmd(fixture, "bq27z8xx mac_write " SHELL_DEV_NAME " 0044 01 02 03", &ret);

	zassert_ok(ret, "mac_write failed: %d", ret);
	zassert_not_null(strstr(out, "MAC write 0x0044 sent (3 data byte(s))."),
			 "output missing payload confirmation:\n%s", out);
}

ZTEST_SUITE(bq27z8xx_shell, NULL, bq27z8xx_shell_setup, bq27z8xx_shell_before, NULL, NULL);
