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

#include <zephyr/drivers/clock_control/mspm0_clock_control.h>

LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(syspll), okay)
#define MSPM0_SYSPLL_ENABLED
#endif


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
#ifdef MSPM0_SYSPLL_ENABLED
		mspm0_switch_and_disable_syspll();
#endif
		break;
	case PM_STATE_STANDBY:
		set_mode_standby(substate_id);
#ifdef MSPM0_SYSPLL_ENABLED
		mspm0_switch_and_disable_syspll();
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

#ifdef MSPM0_SYSPLL_ENABLED
	/* re-enable the PLL if present */
	if(state == PM_STATE_STANDBY || state == PM_STATE_SUSPEND_TO_IDLE){
		mspm0_enable_syspll();
		mspm0_verify_syspll();
		mspm0_switch_to_syspll();
	}
#endif
	/* reset the power policy to RUN/SLEEP. This way if the cpu_idle
	 * thread is entered during a semaphore pend, the peripherals
	 * and other threads will not enter too low of a power state.
	 */
	set_mode_run(0);

	/* interrupts should return to enabled
	 */
	irq_unlock(0);
}
