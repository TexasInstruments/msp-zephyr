# SPDX-License-Identifier: Apache-2.0

board_runner_args(openocd
  "--openocd=${ZEPHYR_BASE}/../openocd/src/openocd"
  "--openocd-search=${ZEPHYR_BASE}/../openocd/tcl"
  # Halt the target before reset init to handle cases where the CPU
  # is running from a previous flash and won't respond to sysresetreq.
  "--cmd-pre-load=halt"
  # After flash+verify, tell the examine-start handler to skip on the next
  # re-examine (triggered by 'reset run'). This prevents FRCACT from being
  # re-asserted after the final reset, so the boot ROM boots cleanly into
  # the flash application instead of entering the debug wait loop.
  "--cmd-post-verify=set ::_mspm33_skip_examine 1"
)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
