/*
 * Copyright (c) 2025 Texas Instruments
 * Copyright (c) 2025 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mspm0_clock_control.h>
#include <zephyr/fatal.h>

#include <ti/driverlib/driverlib.h>
#include <string.h>

#define MSPM0_ULPCLK_DIV COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div),	\
		(CONCAT(DL_SYSCTL_ULPCLK_DIV_,				\
			DT_PROP(DT_NODELABEL(ulpclk), clk_div))),	\
		(0))

#define MSPM0_MCLK_DIV COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(mclk), clk_div),		\
		(CONCAT(DL_SYSCTL_MCLK_DIVIDER_,			\
			DT_PROP(DT_NODELABEL(mclk), clk_div))),		\
		(0))

#define MSPM0_MFPCLK_DIV COND_CODE_1(					\
		DT_NODE_HAS_PROP(DT_NODELABEL(mfpclk), clk_div),	\
		(CONCAT(DL_SYSCTL_HFCLK_MFPCLK_DIVIDER_,		\
			DT_PROP(DT_NODELABEL(mfpclk), clk_div))),	\
		(0))

#define DT_SYSOSC_FREQ	DT_PROP(DT_NODELABEL(sysosc), clock_frequency)
#if DT_SYSOSC_FREQ == 32000000
#define SYSOSC_FREQ	DL_SYSCTL_SYSOSC_FREQ_BASE
#elif DT_SYSOSC_FREQ == 4000000
#define SYSOSC_FREQ	DL_SYSCTL_SYSOSC_FREQ_4M
#else
#error "Set SYSOSC clock frequency not supported"
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mfclk), okay)
#define MSPM0_MFCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mfpclk), okay)
#define MSPM0_MFPCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(syspll), okay)
#define MSPM0_SYSPLL_ENABLED 1
#else
#define MSPM0_SYSPLL_ENABLED 0
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(hfxt), okay)
#define MSPM0_HFCLK_ENABLED 1
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(canclk), okay)
#define MSPM0_CANCLK_ENABLED 1
#endif

#define DT_MCLK_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(mclk))
#define DT_LFCLK_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(lfclk))
#define DT_HFCLK_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(hfclk))
#define DT_MFCLK_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(mfclk))
#define DT_MFPCLK_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(mfpclk))
#define DT_SYSPLL_CLOCKS_CTRL	DT_CLOCKS_CTLR(DT_NODELABEL(syspll))

struct mspm0_clk_cfg {
	uint32_t clk_div;
	uint32_t clk_freq;
};

static struct mspm0_clk_cfg mspm0_lfclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(lfclk), clock_frequency),
};

#if MSPM0_CANCLK_ENABLED
static struct mspm0_clk_cfg mspm0_canclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(canclk), clock_frequency),
};
#endif

static struct mspm0_clk_cfg mspm0_ulpclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(ulpclk), clock_frequency),
	.clk_div = MSPM0_ULPCLK_DIV,
};

#if MSPM0_MFCLK_ENABLED
static struct mspm0_clk_cfg mspm0_mfclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mfclk), clock_frequency),
};
#endif

#if MSPM0_MFPCLK_ENABLED
static struct mspm0_clk_cfg mspm0_mfpclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(mfpclk), clock_frequency),
	.clk_div = MSPM0_MFPCLK_DIV,
};
#endif

#if MSPM0_HFCLK_ENABLED
static struct mspm0_clk_cfg mspm0_hfclk_cfg = {
	.clk_freq = DT_PROP(DT_NODELABEL(hfclk), clock_frequency),
};
#endif

#if MSPM0_SYSPLL_ENABLED

#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
/* basic checks of the devicetree to follow */
#if ((DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk2x_div) && \
	DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk0_div)) || \
	(!DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk2x_div) && \
	!DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk0_div)))
#error "Either CLK2X or CLK0 must be enabled on the SYSPLL in order to supply MCLK"
#endif

#endif /* if SYSPLL is used to source MCLK*/


#if DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk2x_div)
#define MSPM0_SYSPLL_TO_MCLK_SOURCE DL_SYSCTL_SYSPLL_MCLK_CLK2X
#else
/* this is kept separate from the above checks because it is possible
 * to have SYSPLL1 up and not supply MCLK
 */
#define MSPM0_SYSPLL_TO_MCLK_SOURCE DL_SYSCTL_SYSPLL_MCLK_CLK0
#endif


/* check for the FCC upper and lower Bound for the configured
 * SYSPLL output compared to LFCLK.
 *
 * Currently, the tolerance supported is 5% plus or minus the nominal
 * frequency with respect to LFCLK.
 */

#if DT_SAME_NODE(DT_SYSPLL_CLOCKS_CTRL, DT_NODELABEL(sysosc))
#define MSPM0_SYSPLL_INPUT_REF DL_SYSCTL_SYSPLL_REF_SYSOSC
#elif DT_SAME_NODE(DT_SYSPLL_CLOCKS_CTRL, DT_NODELABEL(hfclk))
#define MSPM0_SYSPLL_INPUT_REF DL_SYSCTL_SYSPLL_REF_HFCLK
#else
#error "not a valid input node for syspll"
#endif

#define MSPM0_SYSPLL_INPUT_FREQ DT_PROP(DT_CLOCKS_CTLR_BY_IDX(DT_NODELABEL(syspll), 0), clock_frequency)
#define MSPM0_SYSPLL_QDIV DT_PROP(DT_NODELABEL(syspll), q_div)
#define MSPM0_SYSPLL_PDIV DT_PROP(DT_NODELABEL(syspll), p_div)
#define MSPM0_SYSPLL_VCO (MSPM0_SYSPLL_INPUT_FREQ * MSPM0_SYSPLL_QDIV / MSPM0_SYSPLL_PDIV)

/* determine FCC input. Any of the following would work for FCC. */
#if DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk2x_div)

#define MSPM0_SYSPLL_FCC_INPUT DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK2X
#define MSPM0_SYSPLL_DIVIDER_VALUE (DT_PROP(DT_NODELABEL(syspll),clk2x_div))
#define MSPM0_SYSPLL_MULTIPLIER_VALUE (2)

#elif DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk0_div)

#define MSPM0_SYSPLL_FCC_INPUT DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK0
#define MSPM0_SYSPLL_DIVIDER_VALUE DT_PROP(DT_NODELABEL(syspll),clk0_div)
#define MSPM0_SYSPLL_MULTIPLIER_VALUE (1)

#elif DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk1_div)

#define MSPM0_SYSPLL_FCC_INPUT DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK1
#define MSPM0_SYSPLL_DIVIDER_VALUE DT_PROP(DT_NODELABEL(syspll),clk1_div)
#define MSPM0_SYSPLL_MULTIPLIER_VALUE (1)

#else
#error "Syspll is not in a valid configuration, no clock div is present"
#endif

#define SYSPLL_EXPECTED_FREQ                                                                       \
(((MSPM0_SYSPLL_VCO)/MSPM0_SYSPLL_DIVIDER_VALUE) * MSPM0_SYSPLL_MULTIPLIER_VALUE)

/* perform tolerance at 95% (also 19/20)
 * but the number of monitoring periods is two, thus we use 19/10
 */
#define FCC_LOWER_BOUND (SYSPLL_EXPECTED_FREQ * 19) / \
	(10 * DT_PROP(DT_NODELABEL(lfclk),clock_frequency))
/* upper bound tolerance is 105 % (also 21/20)
 * but the number of monitoring periods is two, thus we use 21/10
 */
#define FCC_UPPER_BOUND (SYSPLL_EXPECTED_FREQ * 21) / \
	(10 * DT_PROP(DT_NODELABEL(lfclk),clock_frequency))


/* internal helper functions */
static void mspm0_disable_syspll(void);

static DL_SYSCTL_SYSPLLConfig clock_mspm0_cfg_syspll = {
	.inputFreq = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.sysPLLMCLK = MSPM0_SYSPLL_TO_MCLK_SOURCE,
	.sysPLLRef = MSPM0_SYSPLL_INPUT_REF,
	.rDivClk2x = (DT_PROP_OR(DT_NODELABEL(syspll), clk2x_div, 1) - 1),
	.rDivClk1 = (DT_PROP_OR(DT_NODELABEL(syspll), clk1_div, 1) - 1),
	.rDivClk0 = (DT_PROP_OR(DT_NODELABEL(syspll), clk0_div, 1) - 1),
	.qDiv = (DT_PROP(DT_NODELABEL(syspll), q_div) - 1),
	.pDiv = CONCAT(DL_SYSCTL_SYSPLL_PDIV_,
		       DT_PROP(DT_NODELABEL(syspll), p_div)),
	.enableCLK2x = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk2x_div),
		(DL_SYSCTL_SYSPLL_CLK2X_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK2X_DISABLE)),
	.enableCLK1 = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk1_div),
		(DL_SYSCTL_SYSPLL_CLK1_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK1_DISABLE)),
	.enableCLK0 = COND_CODE_1(
		DT_NODE_HAS_PROP(DT_NODELABEL(syspll), clk0_div),
		(DL_SYSCTL_SYSPLL_CLK0_ENABLE),
		(DL_SYSCTL_SYSPLL_CLK0_DISABLE)),
};
#endif

void mspm0_enable_syspll(void){
	SYSCTL->SOCLOCK.HSCLKEN |= SYSCTL_HSCLKEN_SYSPLLEN_ENABLE;
}

static int clock_mspm0_on(const struct device *dev, clock_control_subsys_t sys)
{
	return 0;
}

static int clock_mspm0_off(const struct device *dev, clock_control_subsys_t sys)
{
	return 0;
}

static int clock_mspm0_get_rate(const struct device *dev,
				clock_control_subsys_t sys,
				uint32_t *rate)
{
	struct mspm0_sys_clock *sys_clock = (struct mspm0_sys_clock *)sys;

	switch (sys_clock->clk) {
	case MSPM0_CLOCK_LFCLK:
		*rate = mspm0_lfclk_cfg.clk_freq;
		break;

	case MSPM0_CLOCK_ULPCLK:
		*rate = mspm0_ulpclk_cfg.clk_freq;
		break;

	case MSPM0_CLOCK_MCLK:
		*rate = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
		break;

#if MSPM0_MFCLK_ENABLED
	case MSPM0_CLOCK_MFCLK:
		*rate = mspm0_mfclk_cfg.clk_freq;
		break;
#endif

#if MSPM0_MFPCLK_ENABLED
	case MSPM0_CLOCK_MFPCLK:
		*rate = mspm0_mfpclk_cfg.clk_freq;
		break;
#endif

#if MSPM0_CANCLK_ENABLED
	case MSPM0_CLOCK_CANCLK:
		*rate = mspm0_canclk_cfg.clk_freq;
		break;
#endif

#if MSPM0_HFCLK_ENABLED
	case MSPM0_CLOCK_HFCLK:
		*rate = mspm0_hfclk_cfg.clk_freq;
		break;
#endif

	default:
		return -ENOTSUP;
	}

	return 0;
}

static void mspm0_switch_to_sysosc()
{
	switch(DL_SYSCTL_getMCLKSource()){
	case DL_SYSCTL_MCLK_SOURCE_HSCLK:
		DL_SYSCTL_switchMCLKfromHSCLKtoSYSOSC();
		break;
	case DL_SYSCTL_MCLK_SOURCE_LFCLK:
		DL_SYSCTL_switchMCLKfromLFCLKtoSYSOSC();
		break;
	case DL_SYSCTL_MCLK_SOURCE_SYSOSC:
	default:
		/* already configured correctly, no further action */
		break;
	}
}

static int clock_mspm0_init(const struct device *dev)
{
	/* state may have come from another image or somewhere with a different clock configuration
	 * so we add a setting to SYSOSC to start cleanly */
	mspm0_switch_to_sysosc();

	/* setup clocks based on specific rates
	 * As Per TRM, the Frequency field in the sysosc must only be changed when SYSOSC is the
	 * MCLK source, as proven above.
	 */
	DL_SYSCTL_setSYSOSCFreq(SYSOSC_FREQ);

	/* If a syspll is present but not enabled, we take this step to verify that the
	 * syspll is enabled. Only expected case for the SYSPLL to be enabled at this state is if
	 * this is an image paired with MCUBoot or another coding element.
	 */
#if DT_NODE_EXISTS(DT_NODELABEL(syspll)) && !MSPM0_SYSPLL_ENABLED
	mspm0_disable_syspll();
#endif

#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(sysosc)) && (DT_SYSOSC_FREQ == 4000000)
	DL_SYSCTL_setMCLKDivider(MSPM0_MCLK_DIV);
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(ulpclk), clk_div)
	DL_SYSCTL_setULPCLKDivider(mspm0_ulpclk_cfg.clk_div);
#endif

#if MSPM0_HFCLK_ENABLED
#if DT_SAME_NODE(DT_HFCLK_CLOCKS_CTRL, DT_NODELABEL(hfxt))
	uint32_t hf_range;
	uint32_t hfxt_freq = DT_PROP(DT_NODELABEL(hfxt),
				     clock_frequency)  / MHZ(1);
	uint32_t xtal_startup_delay = DT_PROP_OR(DT_NODELABEL(hfxt),
						 ti_xtal_startup_delay_us, 0);

	if (hfxt_freq >= 4 &&
	    hfxt_freq <= 8) {
		hf_range = DL_SYSCTL_HFXT_RANGE_4_8_MHZ;
	} else if (hfxt_freq > 8 &&
		   hfxt_freq <= 16) {
		hf_range = DL_SYSCTL_HFXT_RANGE_8_16_MHZ;
	} else if (hfxt_freq > 16 &&
		   hfxt_freq <= 32) {
		hf_range = DL_SYSCTL_HFXT_RANGE_16_32_MHZ;
	} else if (hfxt_freq > 32 &&
		   hfxt_freq <= 48) {
		hf_range = DL_SYSCTL_HFXT_RANGE_32_48_MHZ;
	} else {
		return -EINVAL;
	}

	/* startup time in 64us resolution */
	DL_SYSCTL_setHFCLKSourceHFXTParams(hf_range,
					   xtal_startup_delay / 64,
					   true);
#else
	DL_SYSCTL_setHFCLKSourceHFCLKIN();
#endif
#endif

#if MSPM0_SYSPLL_ENABLED
	DL_SYSCTL_configSYSPLL(
			(DL_SYSCTL_SYSPLLConfig *)&clock_mspm0_cfg_syspll);

	/* verify that locking occurred correctly */
	mspm0_verify_syspll();
#endif

#if DT_SAME_NODE(DT_LFCLK_CLOCKS_CTRL, DT_NODELABEL(lfxt))
	DL_SYSCTL_LFCLKConfig config = {0};

	DL_SYSCTL_setLFCLKSourceLFXT(&config);
#elif DT_SAME_NODE(DT_LFCLK_CLOCKS_CTRL, DT_NODELABEL(lfdig_in))
	DL_SYSCTL_setLFCLKSourceEXLF();

#endif

#if MSPM0_MFCLK_ENABLED
	DL_SYSCTL_enableMFCLK();
#endif /* MSPM0_MFCLK_ENABLED */

#if MSPM0_MFPCLK_ENABLED
#if DT_SAME_NODE(DT_MFPCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk))
	DL_SYSCTL_setHFCLKDividerForMFPCLK(mspm0_mfpclk_cfg.clk_div);
	DL_SYSCTL_setMFPCLKSource(DL_SYSCTL_MFPCLK_SOURCE_HFCLK);
#else
	DL_SYSCTL_setMFPCLKSource(DL_SYSCTL_MFPCLK_SOURCE_SYSOSC);
#endif
	DL_SYSCTL_enableMFPCLK();
#endif /* MSPM0_MFPCLK_ENABLED */

#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk))
	DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK,
				DL_SYSCTL_HSCLK_SOURCE_HFCLK);

#elif DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	mspm0_switch_to_syspll();

#elif DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(lfclk))
	DL_SYSCTL_setMCLKSource(SYSOSC, LFCLK, false);

#endif /* DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(hfclk)) */

	return 0;
}

static DEVICE_API(clock_control, clock_mspm0_driver_api) = {
	.on = clock_mspm0_on,
	.off = clock_mspm0_off,
	.get_rate = clock_mspm0_get_rate,
};

DEVICE_DT_DEFINE(DT_NODELABEL(ckm), &clock_mspm0_init, NULL, NULL, NULL,
		 PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		 &clock_mspm0_driver_api);


#if DT_NODE_EXISTS(DT_NODELABEL(syspll))

static void mspm0_disable_syspll(void){
	/* Disabling is not functionally necessary but is recommended by the
	 * Low power optimiation guide.
	 */

	/* first confirm that the SYSPLL has reached a steady state of either good or off */
	while((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD |
+			    DL_SYSCTL_CLK_STATUS_SYSPLL_OFF)) == 0U);

	if((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_OFF)) !=
		(DL_SYSCTL_CLK_STATUS_SYSPLL_OFF))
	{
		DL_SYSCTL_disableSYSPLL();
		/* wait for SYSPLL to disable before continuing */
		while ((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_OFF)) !=
			(DL_SYSCTL_CLK_STATUS_SYSPLL_OFF));
	}

}

void mspm0_switch_and_disable_syspll(void)
{
	#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
		/* switch MCLK away from SYSPLL before entering low power mode */
		DL_SYSCTL_switchMCLKfromHSCLKtoSYSOSC();
	#endif
	mspm0_disable_syspll();
}

/* should be entered after SYSPLL is enabled, will check for the hardware to indicate a
 * lock condition, and will also use Frequency Clock Counter (FCC) destructively to
 * check that the frequency was correctly locked onto 80 MHz
 */
#define MSPM0_SYSPLL_TIMEOUT_CYCLES (150u)
static bool mspm0_is_syspll_locked(void){
	uint32_t fccTimeoutCounter = 0u;
	uint32_t fccCount;
	bool isValidLock = false;

	/* wait for the SYSPLL signal to be considered good */
	while (((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) !=
		DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD) && fccTimeoutCounter < MSPM0_SYSPLL_TIMEOUT_CYCLES)
	{
		delay_cycles(97);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 3.05us */
		fccTimeoutCounter++;
	}

	DL_SYSCTL_setFCCPeriods(DL_SYSCTL_FCC_TRIG_CNT_02);

	DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
		            DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
			    MSPM0_SYSPLL_FCC_INPUT);

	DL_SYSCTL_startFCC();

	while(!DL_SYSCTL_isFCCDone() && fccTimeoutCounter < MSPM0_SYSPLL_TIMEOUT_CYCLES){
		delay_cycles(97);  /* 1x LFCLK cycle = 32MHz/32.768kHz = 977, 30.5us */
		fccTimeoutCounter++;
	}

	fccCount = DL_SYSCTL_readFCC();

	if(fccTimeoutCounter < MSPM0_SYSPLL_TIMEOUT_CYCLES &&
		(fccCount > FCC_LOWER_BOUND) && (fccCount < FCC_UPPER_BOUND))
	{
		isValidLock = true;
	}

	return isValidLock;
}

/* function implementing the workaround to add SYSPLL and verify that the
 * frequency lock has been correctly achieved.
 * Also switches MCLK to SYSPLL if configured that way in the devicetree
 */
void mspm0_verify_syspll(void){
	uint32_t locking_attempts = 0u;

	while(!mspm0_is_syspll_locked() && locking_attempts < 10){
		locking_attempts++;

		/* disable SYSPLL */
		mspm0_disable_syspll();

		/* re-enable SYSPLL */
		mspm0_enable_syspll();
	}

	if(locking_attempts >= 10){
		/* The SYSPLL failed to lock after several attempts */
		k_sys_fatal_error_handler(K_ERR_ARCH_START, NULL);
	}
}

void mspm0_switch_to_syspll(void){
#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
#endif
}

#else

void mspm0_switch_and_disable_syspll(void){
	return;
}

void mspm0_enable_syspll(void){
	return;
}

void mspm0_verify_syspll(void){
	return;
}

void mspm0_switch_to_syspll(void){
	return;
}

#endif /* MSPM0_SYSPLL_EXISTS */