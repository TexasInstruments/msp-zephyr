#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/rtc.h>
#include <ti/driverlib/m0p/dl_sysctl.h>
#include <ti/driverlib/dl_gpio.h>
#include <zephyr/dt-bindings/pinctrl/mspm0-pinctrl.h>

static const struct device *rtc = DEVICE_DT_GET(DT_NODELABEL(rtc));
static struct rtc_time calendar_time;

/* Before configuring the RTC and enabling the clock, the LFCLK must be changed to utilize
 * the external source. For this, first configure the desired pin with the specificed function: 
 * PINCM of 4 and PIN_FUNCTION_6 corresponds to PA4 on MSPM0G3507. Then set the LFCLK source to 
 * be external. It is also possible to ensure a valid clock signal with an LFCLK monitor by setting
 * the MONITOR bit in the LFCLKCFG register.
 */
int configure_lfclk_in(void)
{
	DL_GPIO_initPeripheralFunction(9, MSPM0_PIN_FUNCTION_6);
	DL_SYSCTL_setLFCLKSourceEXLF();
	return 0;
}
/* The SYS_INIT macro is used to ensure this configuration happens prior to the RTC driver device
 * init function.
 */
SYS_INIT(configure_lfclk_in, EARLY, 0);

int main(void)
{
	calendar_time.tm_sec = 0;
	calendar_time.tm_min = 0;
	calendar_time.tm_hour = 2;
	calendar_time.tm_wday = 3;
	calendar_time.tm_mday = 23;
	calendar_time.tm_mon = 9;
	calendar_time.tm_year = 2025;
	rtc_set_time(rtc, &calendar_time);

	while(1) {
		rtc_get_time(rtc, &calendar_time);
		k_sleep(K_MSEC(1000));
	}
}