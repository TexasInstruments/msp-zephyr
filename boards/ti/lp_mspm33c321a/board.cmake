# SPDX-License-Identifier: Apache-2.0

board_runner_args(openocd
  "--openocd=${ZEPHYR_BASE}/../openocd/src/openocd"
  "--openocd-search=${ZEPHYR_BASE}/../openocd/tcl"
)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
