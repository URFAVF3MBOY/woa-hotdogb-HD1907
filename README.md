# OnePlus 7T (hotdogb)

Builds the UEFI boot image that runs **Windows on ARM** on the **OnePlus 7T (hotdogb, Snapdragon 855 / SM8150)**.
It is a trimmed, modified, single-device version of [Project Aloha's `mu_aloha_platforms`](https://github.com/Project-Aloha/mu_aloha_platforms),
containing only what the hotdogb build needs.

## Status
Tested on a OnePlus 7T with this repository's image.

> **Requires test signing.** The drivers used with this image are unsigned, so Windows must have test signing turned on
> (`bcdedit /set testsigning on`).

| Feature | Status |
|---|---|
| Boot | Working |
| Display and GPU acceleration | Working (Using hotdog drivers) |
| Touch | Working |
| Wi-Fi | Working |
| Bluetooth | Working |
| Audio | Working |
| Sleep and wake | Working |
| Battery (level and status) | Working |
| Charging over USB-C (USB-C to USB-C, PD charger) | Working — roughly 3–4 W; faster charging is still being worked on |

**Known issues**

| Feature | Status |
|---|---|
| Charging from USB-A to USB-C chargers | Charges, but very slowly |
| USB host (OTG) | Sorta working has issues |
| Cameras | Not working |
| Sensors | Not working |

**Not yet tested:** cellular/modem.

## Drivers
The Windows drivers used with this image come mostly from
[n00b69/woa-op7](https://github.com/n00b69/woa-op7/tree/main). This repository only builds the UEFI firmware;
it does not contain the Windows drivers.

## ACPI (DSDT)
The image uses one DSDT: `Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/ACPI/DSDT.aml`. It is the original Project Aloha
DSDT with four small edits, listed byte by byte in [`docs/dsdt/CHANGES.md`](docs/dsdt/CHANGES.md):

* **GPU:** a new `MSHW1004` panel device that `GPU0` depends on (graphics acceleration)
* **Touch:** `TSC1` no longer waits for `TECC`, and its I2C address is `0x20`
* **Charging:** the USB-C sink power list is 5 V / 2.9 A only (no 9 V or 12 V)

`docs/dsdt/DSDT.original.aml` is the unmodified original and `docs/dsdt/make_dsdt.py` rebuilds the DSDT from it.
To change the DSDT, replace that one file and rebuild.

## What is in the tree
| Path | Content |
|---|---|
| `Platforms/SurfaceDuo1Pkg` | NoSb DSC, FDF, DEC, includes and **only** `Device/oneplus-hotdogb` |
| `Platforms/AndromedaPkg`, `QcomModulePkg`, `CranePkg`, `OpensslPkg`, `SurfaceDuoACPI` | only the files the hotdogb build opens (plus every source file named in the INFs used) |
| `Silicon/QC/Sm8150` | SM8150 silicon package |
| `MU_BASECORE`, `Common/*`, `Silicon/Arm/MU_TIANO`, `Features/*` | Project Mu / edk2 files that are actually used; BaseTools C and Python sources; `MdePkg/Include` |
| `BootShim`, `ImageResources/emptyramdisk`, `tools/` | boot image packaging |

Removed: every other device and SoC, the secure-boot variant (`SurfaceDuo1.dsc`), the Surface Duo / Epsilon / Zeta
images (about 100 MB), docs, CI files, Docker files, stuart scripts, unit tests, unused parts of the submodules and their nested submodules
(brotli, oniguruma, googletest, cmocka, libspdm and similar).

## Credits
* [n00b69/woa-op7](https://github.com/n00b69/woa-op7) — Windows drivers and the OnePlus 7T guide
* [Project Aloha](https://github.com/Project-Aloha) — `mu_aloha_platforms`, the SM8150 platform code and prebuilt binaries
* [WOA-Project](https://github.com/WOA-Project) — Qualcomm ACPI tables
* [Project Mu](https://github.com/microsoft/mu_basecore) and [TianoCore EDK II](https://github.com/tianocore/edk2) — firmware core
* OpenSSL

## Licences
Project Mu / edk2 (BSD-2-Clause-Patent), OpenSSL (Apache-2.0), the repository `LICENSE`, and the licence files at the
root of each included tree are kept. The crypto binaries and `Binaries/` are downloaded at build time, not stored here.
`Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/{Binaries,PatchedBinaries}` contain prebuilt drivers from the upstream project.
