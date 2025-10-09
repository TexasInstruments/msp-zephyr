.. zephyr:code-sample:: lp5810_led_display
   :name: LP5810 LED display using MSPM0 I2C.

   This example communicates with an external LP5810 using the LED driver
   and underlying MSPM0 I2C implementation. The example will continously loop
   and display a marquee pattern on LED 0-3.

   Connect the I2C SDA and SCL lines to the evm. 
   For wiring and external power supply requirements, refer to the LP5810-LP5811 EVM.