# # SPDX-License-Identifier: Apache-2.0

board_runner_args(xds110 "--device=MSPM0L2228" "--speed=4000")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/xds110.board.cmake)
