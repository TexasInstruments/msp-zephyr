# SPDX-License-Identifier: Apache-2.0

board_runner_args(xds110 "--device=MSPM0G3519" "--speed=4000")
#board_runner_args(openocd "_mspm0_enable_low_power_mode")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
#include(${ZEPHYR_BASE}/boards/common/xds110.board.cmake)
