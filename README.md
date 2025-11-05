<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://www.ti.com/content/dam/ticom/images/identities/ti-brand/ti-logo-hz-1c-white.svg" width="300">
  <img alt="Texas Instruments Logo" src="https://www.ti.com/content/dam/ticom/images/identities/ti-brand/ti-hz-2c-pos-rgb.svg" width="300">
</picture>

<div align="left">

# MSPM33 Downstream Support For Zephyr
This repository contains early Zephyr Support for MSPM33-class devices

The Texas Instruments Zephyr GitHub repository is the starting point for Zephyr
development on supported Texas Instruments devices. TI's Zephyr solution is
based on the Zephyr project and utilizes the same familiar environment, tools,
and dependencies.

## What is Zephyr?

The Zephyr Project is a scalable real-time operating system (RTOS) supporting
multiple hardware architectures, optimized for resource constrained devices,
and built with security in mind.

The Zephyr OS is based on a small-footprint kernel designed for use on
resource-constrained systems: from simple embedded environmental sensors and
LED wearables to sophisticated smart watches and IoT wireless gateways.

This downstream repository of TI Zephyr is based on the current upstream and
advance features concerning specific MSPM33 MCUs.

This release contains support for the `MSPM33C321A` device.

## Release Information

| Version | v4.2.0-ti-1.00.00.00_EA (pre-APL) |
|---------|--------------------------|
| Zephyr RTOS | <br>• Zephyr Base Device Support<br>• Board Configuration, DTS, Interrupts, Timers, Clock (Fix Clocked), Pinmux, GPIO<br>• HS-ADC (Basic)<br>• Blinky Example |

### Devices

Supported:

- [MSPM33C321A]

### Boards

Supported:

- [lp_mspm33c321a]

## Getting Started

For getting started, please refer to the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

For detailed board-specific information, building instructions, flashing procedures, and required development tools, please refer to the board documentation in `boards/ti/lp_mspm33c321a/doc/index.rst`.

> **_NOTE:_** When running `west init` in the getting-started guide it's
> important to instead run `west init -m https://github.com/TexasInstruments/msp-zephyr --mr v4.2.0-ti-1.00.00.00_ea zephyrproject`
> in order to use the TI Zephyr repository.
> Replace `zephyrproject` with your desired project directory name.
> The `v4.2.0-ti-1.00.00.00_ea` tag changes with each release from TI and zephyr .
>


## Versioning

TI will tag each release with the following format: {upstream-tag}-ti-M.mm.pp(\_optional-qualifier)

This tag can be broken down into 4 components:

- upstream-tag: This is the tag or commit of the [Zephyr](https://github.com/zephyrproject-rtos/zephyr)
  repo that the TI release is based on
- -ti-: Separator
- TI release version: This is TI's version on top of the upstream Zephyr
  version. The version scheme is explained below.
- Qualifier. The qualifier keyword is described below.


## Releases

All releases will be tagged using the version format above. Release notes are
provided in the form of GitHub's release notices.


### Disclaimer

This release is provided as-is and should be considered Beta quality. This
product is meant for demonstration purposes only. Please refer to the Release Notes for
details on specific limitations and known issues.

## Need help?

- For technical support with TI Zephyr, including bugs and feature requests -
  submit a ticket to [TI's MSP E2E forum](https://e2e.ti.com/support/microcontrollers/msp-low-power-microcontrollers-group/msp430/f/msp-low-power-microcontroller-forum)
  > **_NOTE:_** Please do not use the Github issue tracker for this project.

Additionally, we welcome any feedback that you can give to improve the
documentation!
