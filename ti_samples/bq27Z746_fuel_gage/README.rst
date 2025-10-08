.. zephyr:code-sample:: bq27Z746_fuel_gage
   :name: MSPM0 I2C Fuel Gage Interface Example

   Demonstrates interfacing with the BQ27Z746 fuel gage via I2C communication. The example sets
   up two threads that consistently read the voltage and current readings.

Overview
********

This is a sample app demonstrating simple interfacing with an external fuel gage using the I2C
and fuel gage drivers. Two threads using the same I2C bus will continously read the voltage and
current readings from the fuel gage. The readings of both properties are also printed out on
the serial console.


Wiring
******

Connect SDA and SCL I2C lines. Refer to the BQ27Z746 EVM for specific wiring pin and external
power requirements. 
