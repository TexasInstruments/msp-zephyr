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

static void set_mode_run(uint8_t state)
{
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
		break;
	case PM_STATE_STANDBY:
		set_mode_standby(substate_id);

		break;
	default:
		LOG_DBG("Unsupported power state %u", state);
		return;
	}

	/*
	 * Workaround for CPU_ERR_03
	 *
	 * If transitioning into a low power mode while a prefetch is pending, the prefetcher can
	 * erroneously fetch bad data. Corrective action is to disable the prefetcher prior to sleep
	 * and reenable it afterwards.
	 */

	/* Disable Prefetch */
	CPUSS->CTL &= 0x6;
	SYSCTL->SOCLOCK.SHUTDNSTORE0;
	__WFI();
	/* Re-enable Prefetch */
	CPUSS->CTL |= 0x1;
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	irq_unlock(0);
}
