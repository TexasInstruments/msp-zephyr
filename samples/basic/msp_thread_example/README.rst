.. zephyr:code-sample:: msp_thread_example
   :name: Morse Code Detector

   Decodes morse code transmission using a button press for long and short pulses. Transmit
   the decoded characters over UART.

Overview
********

This is a sample app demonstrating simple use of threading and callbacks in zephyr.

A button callback is attached to a GPIO interrupt on either a falling or rising edge. The callback
detects if the button is being pressed and records how long the button was held on the next release
of the button. Using this duration, either a long or short pulse in interpretted. A separate decoding
thread reads from a shared message queue to decode the pulses into a corresponding character that is
then echoed over the UART. Refer to the overlay file and corresponding board dts for pin locations.

Wiring
******

The UART TX pin can be used with the backchannel XDS by installing shunt jumper J21 1:2. To use
UART on the boosterpack connector, install shunt jumper J21 2:3.

Install shunt jumper J5 to see the LED pulse corresponding to the button held duration. 
