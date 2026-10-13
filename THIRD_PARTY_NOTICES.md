# Third-party components

MIT in the repository root applies to project-authored code and documentation. It does not replace any third-party license or grant rights to an entire linked firmware image.

## Bundled ARM CMSIS-DAP sources

- Files: `libraries/WirelessDAP/src/arm/DAP.c`, `SW_DP.c`, `DAP_vendor.c`, `DAP.h`.
- Upstream: https://github.com/ARM-software/CMSIS-DAP
- Pinned commit: `12636590eec66fae2d1bba4518749426ad5a4595`.
- Copyright: ARM Limited, as retained in each source header.
- License: Apache-2.0, full text in `licenses/CMSIS-DAP-Apache-2.0.txt`.
- These four files are copied unchanged from that revision. Project adaptations live in board/configuration and wrapper code.

## Build-time dependencies installed separately

- Adafruit nRF52 Arduino Core 1.7.0: https://github.com/adafruit/Adafruit_nRF52_Arduino/tree/1.7.0
  Core notice is retained in `licenses/Adafruit-nRF52-NOTICE.txt`; core code generally uses LGPL-2.1-or-later, with components under their own licenses.
- Adafruit TinyUSB Arduino / TinyUSB: use the versions bundled with core1.7.0. https://github.com/adafruit/Adafruit_TinyUSB_Arduino and https://github.com/hathach/tinyusb ; retain the licenses supplied with those packages.
- FreeRTOS, Nordic SDK/HAL and SoftDevice interfaces: use the packages installed by core1.7.0 and retain their component-specific notices and conditions. The application build does not install or distribute a full SoftDevice or bootloader image.
- Toolchain and UF2 converter are external build tools, not included as binaries in this source repository.

The complete external SDK is not relicensed or vendored here. Install the pinned dependency through the Adafruit package index as documented in BUILD.md. `source-manifest.json` records copied project files and reference firmware identity.

If distributing a linked UF2/ELF, fulfill all applicable dependency terms. In particular, preserve notices and provide the required corresponding source and/or application material suitable for relinking with modified LGPL components, as applicable. The source-only initial GitHub upload avoids asserting that a linked firmware is wholly MIT-licensed.

The USB VID/PID in the source is a development configuration, not a grant of USB identifier rights. Product names and board references do not imply endorsement by Arm, Nordic or Adafruit.
