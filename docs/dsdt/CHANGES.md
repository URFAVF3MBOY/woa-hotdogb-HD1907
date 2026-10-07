# DSDT changes (hotdogb)

<!-- SPDX-License-Identifier: Apache-2.0 -->
Copyright 2026 URFAVF3MBOY, Apache-2.0 (see `NOTICE` and `LICENSE-APACHE-2.0`). First written 2026-10-07. https://github.com/URFAVF3MBOY/woa-hotdogb-HD1907

`Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/ACPI/DSDT.aml` is the original Project Aloha DSDT
(`docs/dsdt/DSDT.original.aml`) plus the four edits below, and nothing else.
`python3 docs/dsdt/make_dsdt.py docs/dsdt/DSDT.original.aml DSDT.aml` rebuilds it byte for byte and checks every
expected byte before changing it.

Offsets are into the original file. Total size: 364,648 -> 364,680 bytes (+32).

## 1. GPU acceleration (original Surface Duo graphics driver)

| What                                            | Original        | Now                                                    |
| ----------------------------------------------- | --------------- | ------------------------------------------------------ |
| New device in`\_SB` (inserted at `0x545df`) | none            | `Device(SPNL) { Name(_HID, "MSHW1004") }` (22 bytes) |
| `GPU0` `_DEP` (inserted at `0x45cad`)     | 10 dependencies | 11: adds`\_SB.SPNL` (10 bytes)                       |

The GPU device now depends on a panel device with the `MSHW1004` ID, which the Surface Duo graphics driver expects.
The same edit also updates the lengths that enclose the new bytes: `GPU0` device length (`0x45be8`), its `_DEP`
package length and element count (`0x45c46`).

## 2. Touch (device `TSC1`, `_HID` `OPOP1003`)

| What                                          | Original                   | Now                                                       |
| --------------------------------------------- | -------------------------- | --------------------------------------------------------- |
| `_DEP` (at `0x57651`)                     | `GIO0, IC18, PEP0, TECC` | `GIO0, IC18, PEP0, PEP0` (no longer waits for `TECC`) |
| I2C slave address in`_CRS` (at `0x57677`) | `0x48`                   | `0x20`                                                  |

## 3. Charging: USB-C sink power list (`UCP0` `_DSD`, at `0x54fc7`)

| Entry | Original                      | Now                          |
| ----- | ----------------------------- | ---------------------------- |
| 1     | `0x0001912C` = 5 V / 3.0 A  | `0x00019122` = 5 V / 2.9 A |
| 2     | `0x0002D0C8` = 9 V / 2.0 A  | `0x00019122` = 5 V / 2.9 A |
| 3     | `0x0003C096` = 12 V / 1.5 A | `0x00019122` = 5 V / 2.9 A |

PDO format: voltage in bits 19:10 (50 mV units), current in bits 9:0 (10 mA units).
5 V only stops the charger from flapping (repeated detach and attach); 9 V and 12 V requests fail on this phone.
2.9 A instead of 3.0 A avoids a driver rule (`qcbattmngr8150`) that clamps the input current to 500 mA when the
source is 5 V and the reported limit is exactly 1500 or 3000 mA.

## 4. Length and checksum bookkeeping

The table length at `0x04` (`0x59068` -> `0x59088`), the `Scope(\_SB)` length at `0x26` (+32) and the checksum at `0x09`.

## Not changed on purpose

Battery profile (`FCC1` 2100 mA, `CCC1` 1000 mA, `DCMA` 900 mA, JEITA limits) is the original. Changing `CCC1` (1501 / 1600 /
2000 mA) and the JEITA limits made no difference, so they are not part of this tree. The SoC identification fields near the start
of the table are filled in by the firmware at boot (`AcpiPlatformUpdateLib`), so they are left as in the original.
