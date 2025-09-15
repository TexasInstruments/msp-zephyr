.. zephyr:code-sample:: mcumgr_mspm0_target_device
   :name: MSPM0 DFU using MCUBoot + MCUMGR 

   Demonstrates device firmware upgrades using MCUBoot + MCUMGR. The MSPM0 will enter
   serial recovery mode upon restart if S2 is held down. At this point, a new firmware image
   can be upload via MCUMGR with UART as the selected transport.