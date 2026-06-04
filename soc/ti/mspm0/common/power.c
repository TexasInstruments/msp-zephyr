/*
 * Copyright (c) 2025 Linumiz GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/logging/log.h>
#include <ti/driverlib/driverlib.h>

LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(syspll), okay)
#define SYSPLL_ENABLED
#endif

#ifdef SYSPLL_ENABLED
static void disable_syspll(void)
{
	/* Disabling is not functionally necessary but is recommended by the
	 * Low power optimiation guide.
	 */
#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	/* switch MCLK away from SYSPLL before entering low power mode */
	DL_SYSCTL_switchMCLKfromHSCLKtoSYSOSC();
#endif

	DL_SYSCTL_disableSYSPLL();
	/* wait for SYSPLL to disable before continuing */
	while ((DL_SYSCTL_getClockStatus() & (DL_SYSCTL_CLK_STATUS_SYSPLL_OFF)) !=
		(DL_SYSCTL_CLK_STATUS_SYSPLL_OFF));
}

static void enable_syspll(void)
{
	/* enable SYSPLL and wait until stabilized before switching */
	SYSCTL->SOCLOCK.HSCLKEN |= SYSCTL_HSCLKEN_SYSPLLEN_ENABLE;

	while ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) !=
		DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD);

#if DT_SAME_NODE(DT_MCLK_CLOCKS_CTRL, DT_NODELABEL(syspll))
	DL_SYSCTL_switchMCLKfromSYSOSCtoHSCLK(DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
#endif /* end if same node */
}

#endif /* if SYSPLL enabled */

static void set_mode_run(uint8_t state)
{
	/* this creates a run/sleep pair */
	SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk);
}

static void set_mode_stop(uint8_t state)
{
	switch (state) {
	case DL_SYSCTL_POWER_POLICY_STOP0:
		DL_SYSCTL_setPowerPolicySTOP0();
		break;
	case DL_SYSCTL_POWER_POLICY_STOP1:
		DL_SYSCTL_setPowerPolicySTOP1();
		break;
	case DL_SYSCTL_POWER_POLICY_STOP2:
		DL_SYSCTL_setPowerPolicySTOP2();
		break;
	}
}

static void set_mode_standby(uint8_t state)
{
	switch (state) {
	case DL_SYSCTL_POWER_POLICY_STANDBY0:
		DL_SYSCTL_setPowerPolicySTANDBY0();
		break;
	case DL_SYSCTL_POWER_POLICY_STANDBY1:
		DL_SYSCTL_setPowerPolicySTANDBY1();
		break;
	}
}

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	switch (state) {
	case PM_STATE_RUNTIME_IDLE:
		set_mode_run(substate_id);
		break;
	case PM_STATE_SUSPEND_TO_IDLE:
		set_mode_stop(substate_id);
#ifdef SYSPLL_ENABLED
		disable_syspll();
#endif
		break;
	case PM_STATE_STANDBY:
		set_mode_standby(substate_id);
#ifdef SYSPLL_ENABLED
		disable_syspll();
#endif
		break;
	default:
		LOG_DBG("Unsupported power state %u", state);
		return;
	}



	__WFI();
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	irq_unlock(0);

#ifdef SYSPLL_ENABLED
	/* re-enable the PLL if present */
	if(state == PM_STATE_STANDBY || state == PM_STATE_SUSPEND_TO_IDLE){
		enable_syspll();
	}
#endif

	/* reset the power policy to RUN/SLEEP. This way if the cpu_idle
	 * thread is entered during a semaphore pend, the peripherals
	 * and other threads will not enter too low of a power state.
	 */
	set_mode_run(0);

}
