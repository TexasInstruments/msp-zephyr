/*
 * Copyright (c) 2025 Texas Instruments
 * Copyright (c) 2025 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/msp_clock_control.h>

#include <ti/driverlib/driverlib.h>
#include <string.h>

/**
 * @brief Platform detection macros
 */
#if defined(CONFIG_SOC_SERIES_MSPM0G) || defined(CONFIG_SOC_SERIES_MSPM0L)
#define MSP_M0 1
#elif defined(CONFIG_SOC_SERIES_MSPM33C)
#define MSP_M33 1
#endif /* Platform detection macros */

/**
 * @brief Conversion factor from MHz to Hz
 */
#define MHZ_TO_HZ_FACTOR MHZ(1)

/**
 * @brief Clock divider definitions based on platform
 */
#if defined(MSP_M0)
#define MSP_ULPCLK_DIV                                                                             \
	COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div),	\
		(CONCAT(DL_SYSCTL_ULPCLK_DIV_,				\
			DT_PROP(DT_NODELABEL(ulpclk), clk_div))),	\
		(0))
#elif defined(MSP_M33)
#define MSP_ULPCLK_DIV                                                                             \
	COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div),	\
		(CONCAT(DL_SYSCTL_ULPCLK_DIV_1_DIV_2_DIV_,		\
			DT_PROP(DT_NODELABEL(ulpclk), clk_div))),	\
		(DL_SYSCTL_ULPCLK_DIV_1_DIV_2_DIV_4))
#endif /* MSP_M0 vs MSP_M33 ULPCLK_DIV */

#define MSP_MCLK_DIV                                                                               \
	COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(mclk), clk_div),		\
		(CONCAT(DL_SYSCTL_MCLK_DIVIDER_,			\
			DT_PROP(DT_NODELABEL(mclk), clk_div))),		\
		(0))

#define MSP_PLL_DIV                                                                                \
	COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(pll), pll_clk_div),		\
		(CONCAT(DL_SYSCTL_PLL_DIVIDER_DIV,			\
			DT_PROP(DT_NODELABEL(pll), pll_clk_div))),		\
		(-1))

#define MSP_MFPCLK_DIV                                                                             \
	COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(mfpclk), clk_div),	\
		(CONCAT(DL_SYSCTL_HFCLK_MFPCLK_DIVIDER_,		\
			DT_PROP(DT_NODELABEL(mfpclk), clk_div))),	\
		(0))

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mfpclk), okay)
#define MSP_MFPCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mfclk), okay)
#define MSP_MFCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pll), okay)
#define MSP_PLL_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(hfxt), okay)
#define MSP_HFCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(lfclk), okay)
#define MSP_LFCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(canclk), okay)
#define MSP_CANCLK_ENABLED 1
#endif

#define DT_MCLK_CLOCKS_CTRL   DT_CLOCKS_CTLR(DT_NODELABEL(mclk))
#define DT_LFCLK_CLOCKS_CTRL  DT_CLOCKS_CTLR(DT_NODELABEL(lfclk))
#define DT_HSCLK_CLOCKS_CTRL  DT_CLOCKS_CTLR(DT_NODELABEL(hsclk))
#define DT_HFCLK_CLOCKS_CTRL  DT_CLOCKS_CTLR(DT_NODELABEL(hfclk))
#define DT_MFPCLK_CLOCKS_CTRL DT_CLOCKS_CTLR(DT_NODELABEL(mfpclk))
#define DT_PLL_CLOCKS_CTRL    DT_CLOCKS_CTLR(DT_NODELABEL(pll))

/**
 * @brief Clock configuration structure
 */
struct msp_clk_cfg {
	uint32_t clk_div;  /* Clock divider value */
	uint32_t clk_freq; /* Clock frequency in Hz */
};

static struct msp_clk_cfg msp_lfclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(lfclk), clock_frequency),
};

#if MSP_CANCLK_ENABLED
static struct msp_clk_cfg msp_canclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(canclk), clock_frequency),
};
#endif /* MSP_CANCLK_ENABLED */

static struct msp_clk_cfg msp_ulpclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(ulpclk), clock_frequency),
	.clk_div = MSP_ULPCLK_DIV,
};

static struct msp_clk_cfg msp_mclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mclk), clock_frequency),
	.clk_div = MSP_MCLK_DIV,
};

#if defined(MSP_M33)
static struct msp_clk_cfg msp_mclkby2_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mclk), clock_frequency) / 2,
};
#endif /* defined(MSP_M33) */

#if MSP_MFCLK_ENABLED
static struct msp_clk_cfg msp_mfclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mfclk), clock_frequency),
};
#endif /* MSP_MFCLK_ENABLED */

#if defined(MSP_M33) && MSP_PLL_ENABLED
static struct msp_clk_cfg msp_pll_cfg = {
	.clk_div = MSP_PLL_DIV,
};
#endif /* defined(MSP_M33) && MSP_PLL_ENABLED */

#if MSP_MFPCLK_ENABLED
static struct msp_clk_cfg msp_mfpclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mfpclk), clock_frequency),
	.clk_div = MSP_MFPCLK_DIV,
};
#endif /* MSP_MFPCLK_ENABLED */

#if MSP_PLL_ENABLED
/* Basic checks of the devicetree to follow */
/* Currently this check is not valid for M33 */
#if defined(MSP_M0)
#if (DT_NODE_HAS_PROP(DT_NODELABEL(pll), clk2x_div) &&                                             \
     DT_NODE_HAS_PROP(DT_NODELABEL(pll), clk0_div))
#error "Only CLK2X or CLK0 can be enabled at a time on the PLL"
#endif /* PLL CLK2X and CLK0 check */
#endif /* defined(MSP_M0) */

static DL_SYSCTL_SYSPLLConfig clock_msp_cfg_syspll = {
	.inputFreq = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.sysPLLMCLK = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
	.sysPLLRef = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.rDivClk2x = (DT_PROP_OR(DT_NODELABEL(pll), clk2x_div, 1) - 1),
	.rDivClk1 = (DT_PROP_OR(DT_NODELABEL(pll), clk1_div, 1) - 1),
	.rDivClk0 = (DT_PROP_OR(DT_NODELABEL(pll), clk0_div, 1) - 1),
	.qDiv = (DT_PROP(DT_NODELABEL(pll), q_div) - 1),
	.pDiv = CONCAT(DL_SYSCTL_SYSPLL_PDIV_, DT_PROP(DT_NODELABEL(pll), p_div)),
	.enableCLK2x = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(pll), clk2x_div),
		(DL_SYSCTL_SYSPLL_CLK2X_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK2X_DISABLE)),
		 .enableCLK1 = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(pll), clk1_div),
		(DL_SYSCTL_SYSPLL_CLK1_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK1_DISABLE)),
			  .enableCLK0 = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(pll), clk0_div),
		(DL_SYSCTL_SYSPLL_CLK0_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK0_DISABLE)),
};
#endif /* MSP_PLL_ENABLED */

/**
 * @brief Turn on the specified clock
 *
 * @param dev Clock control device
 * @param sys Clock subsystem
 * @return 0 on success
 */
static int clock_msp_on(const struct device *dev, clock_control_subsys_t sys)
{
	return 0;
}

/**
 * @brief Turn off the specified clock
 *
 * @param dev Clock control device
 * @param sys Clock subsystem
 * @return 0 on success
 */
static int clock_msp_off(const struct device *dev, clock_control_subsys_t sys)
{
	return 0;
}

/**
 * @brief Get the clock rate for a specific clock
 *
 * @param dev Clock control device
 * @param sys Clock subsystem
 * @param rate Pointer to store the clock rate
 * @return 0 on success, negative errno on failure
 */
static int clock_msp_get_rate(const struct device *dev, clock_control_subsys_t sys, uint32_t *rate)
{
	struct msp_sys_clock *sys_clock = (struct msp_sys_clock *)sys;

	switch (sys_clock->clk) {
	case MSP_CLOCK_LFCLK:
		*rate = msp_lfclk_cfg.clk_freq;
		break;

	case MSP_CLOCK_ULPCLK:
		*rate = msp_ulpclk_cfg.clk_freq;
		break;

	case MSP_CLOCK_MCLK:
		*rate = msp_mclk_cfg.clk_freq;
		break;

#if defined(MSP_M33)
	case MSP_CLOCK_MCLKBY2:
		*rate = msp_mclkby2_cfg.clk_freq;
		break;
#endif /* defined(MSP_M33) */

#if MSP_MFPCLK_ENABLED
	case MSP_CLOCK_MFPCLK:
		*rate = msp_mfpclk_cfg.clk_freq;
		break;
#endif /* MSP_MFPCLK_ENABLED */

#if MSP_MFCLK_ENABLED
	case MSP_CLOCK_MFCLK:
		*rate = msp_mfclk_cfg.clk_freq;
		break;
#endif /* MSP_MFCLK_ENABLED */

#if MSP_CANCLK_ENABLED
	case MSP_CLOCK_CANCLK:
		*rate = msp_canclk_cfg.clk_freq;
		break;
#endif /* MSP_CANCLK_ENABLED */

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Initialize the MSP clock control driver
 *
 * @param dev Clock control device
 * @return 0 on success, negative errno on failure
 */
static int clock_msp_init(const struct device *dev)
{
	/* Setup SYSOSC frequency */
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE); /* Base frequency (32MHz) */

	/* Configure clock dividers */
#if defined(MSP_M0)
	/* Configure MCLK divider - M0 specific */
	DL_SYSCTL_setMCLKDivider(msp_mclk_cfg.clk_div);
#endif /* defined(MSP_M0) */

#if DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div)
	DL_SYSCTL_setULPCLKDivider(msp_ulpclk_cfg.clk_div);
#endif /* DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div) */

#if MSP_PLL_ENABLED
	/* Configure PLL settings based on devicetree */
#if defined(MSP_M33) && DT_SAME_NODE(DT_HSCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	/* MSPM33 hsclk uses CLK0 output — override default CLK2X */
	clock_msp_cfg_syspll.sysPLLMCLK = DL_SYSCTL_SYSPLL_MCLK_CLK0;
#endif

#if DT_SAME_NODE(DT_PLL_CLOCKS_CTRL, DT_NODELABEL(hfclk))
	clock_msp_cfg_syspll.sysPLLRef = DL_SYSCTL_SYSPLL_REF_HFCLK;
#endif
	DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *)&clock_msp_cfg_syspll);

/*M33 PLL divider*/
#if defined(MSP_M33)
	if (msp_pll_cfg.clk_div != -1) {
		DL_SYSCTL_enablePLLDivider(msp_pll_cfg.clk_div);
	}
#endif /* defined(MSP_M33) */

#endif /* MSP_PLL_ENABLED */

#if MSP_HFCLK_ENABLED
#if DT_SAME_NODE(DT_HFCLK_CLOCKS_CTRL, DT_NODELABEL(hfxt))
	uint32_t hf_range;
	uint32_t hfxt_freq = DT_PROP(DT_NODELABEL(hfxt), clock_frequency) / MHZ_TO_HZ_FACTOR;
	uint32_t xtal_startup_delay = DT_PROP_OR(DT_NODELABEL(hfxt), ti_xtal_startup_delay_us, 0);
	bool monitor_enable = DT_PROP_OR(DT_NODELABEL(hfxt), ti_monitor_enable, true);

	/* Fix frequency range boundaries to ensure correct handling */
	if (hfxt_freq >= 4 && hfxt_freq <= 8) {
		hf_range = DL_SYSCTL_HFXT_RANGE_4_8_MHZ;
	} else if (hfxt_freq > 8 && hfxt_freq <= 16) {
		hf_range = DL_SYSCTL_HFXT_RANGE_8_16_MHZ;
	} else if (hfxt_freq > 16 && hfxt_freq <= 32) {
		hf_range = DL_SYSCTL_HFXT_RANGE_16_32_MHZ;
	} else if (hfxt_freq > 32 && hfxt_freq <= 48) {
		hf_range = DL_SYSCTL_HFXT_RANGE_32_48_MHZ;
	} else {
		return -EINVAL;
	}

	/* startup time in 64us resolution */
	DL_SYSCTL_setHFCLKSourceHFXTParams(hf_range, xtal_startup_delay / 64, monitor_enable);
#else
	DL_SYSCTL_setHFCLKSourceHFCLKIN();
#endif /* DT_SAME_NODE(DT_HFCLK_CLOCKS_CTRL, DT_NODELABEL(hfxt)) */
#endif /* MSP_HFCLK_ENABLED */

#if MSP_LFCLK_ENABLED
#if DT_SAME_NODE(DT_LFCLK_CLOCKS_CTRL, DT_NODELABEL(lfxt))
	uint32_t drive_strength = DT_PROP_OR(DT_NODELABEL(lfxt), ti_drive_strength, 0);
	uint32_t xt1Drive;

	/* Map the drive strength value to the appropriate enum */
	switch (drive_strength) {
	case 0:
		xt1Drive = DL_SYSCTL_LFXT_DRIVE_STRENGTH_LOWEST;
		break;
	case 1:
		xt1Drive = DL_SYSCTL_LFXT_DRIVE_STRENGTH_LOWER;
		break;
	case 2:
		xt1Drive = DL_SYSCTL_LFXT_DRIVE_STRENGTH_HIGHER;
		break;
	case 3:
		xt1Drive = DL_SYSCTL_LFXT_DRIVE_STRENGTH_HIGHEST;
		break;
	default:
		xt1Drive = DL_SYSCTL_LFXT_DRIVE_STRENGTH_LOWEST;
		break;
	}

	DL_SYSCTL_LFCLKConfig config = {
		.lowCap = DT_PROP_OR(DT_NODELABEL(lfxt), ti_low_cap, false),
		.xt1Drive = xt1Drive,
		.monitor = DT_PROP_OR(DT_NODELABEL(lfxt), ti_monitor_enable, true)};

	DL_SYSCTL_setLFCLKSourceLFXT(&config);
#elif DT_SAME_NODE(DT_LFCLK_CLOCKS_CTRL, DT_NODELABEL(lfdig_in))
	DL_SYSCTL_setLFCLKSourceEXLF();
#endif /* DT_SAME_NODE check for LFCLK source */
#endif /* MSP_LFCLK_ENABLED */

	/* Configure MCLK source */
#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(hsclk))
#if DT_SAME_NODE(DT_HSCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk))
	DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_HFCLK);
#elif MSP_PLL_ENABLED
	/* Handle PLL as MCLK source */
#if DT_SAME_NODE(DT_HSCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
#endif /* DT_SAME_NODE(DT_HSCLK_CLOCKS_CTRL, DT_NODELABEL(syspll)) */
#endif /* MSP_PLL_ENABLED */

#elif defined(MSP_M0) && DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(lfclk))
	DL_SYSCTL_setMCLKSource(SYSOSC, LFCLK, false);
#endif /* DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(hsclk)) */

	/* Configure platform-specific clocks */
#if MSP_MFPCLK_ENABLED
	/* MFPCLK is only supported by M0 */
#if DT_SAME_NODE(DT_MFPCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk))
	DL_SYSCTL_setHFCLKDividerForMFPCLK(msp_mfpclk_cfg.clk_div);
	DL_SYSCTL_setMFPCLKSource(DL_SYSCTL_MFPCLK_SOURCE_HFCLK);
#else
	DL_SYSCTL_setMFPCLKSource(DL_SYSCTL_MFPCLK_SOURCE_SYSOSC);
#endif /* DT_SAME_NODE(DT_MFPCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk)) */
	DL_SYSCTL_enableMFPCLK();
#endif /* MSP_MFPCLK_ENABLED */

#if MSP_MFCLK_ENABLED
	/* MFCLK is only supported by M33 */
	DL_SYSCTL_enableMFCLK();
#endif /* MSP_MFCLK_ENABLED */

	return 0;
}

/**
 * @brief Clock control driver API structure
 */
static DEVICE_API(clock_control, clock_msp_driver_api) = {
	.on = clock_msp_on,
	.off = clock_msp_off,
	.get_rate = clock_msp_get_rate,
};

DEVICE_DT_DEFINE(DT_NODELABEL(ckm), &clock_msp_init, NULL, NULL, NULL, PRE_KERNEL_1,
		 CONFIG_CLOCK_CONTROL_INIT_PRIORITY, &clock_msp_driver_api);
