# OnePlus 7T (hotdogb)

Builds the UEFI boot image that runs **Windows on ARM** on the **OnePlus 7T (hotdogb, Snapdragon 855 / SM8150)**.
It is a trimmed, modified, single-device version of [Project Aloha's `mu_aloha_platforms`](https://github.com/Project-Aloha/mu_aloha_platforms), containing only what the hotdogb build needs.

---

## 🛑 CRITICAL WARNING — READ BEFORE FLASHING

> [!WARNING]
> **This firmware is highly experimental. Documentation is intentionally minimal.**
> If you rush, skip steps, or blindly copy-paste commands, **you WILL hard-brick your smartphone.** 

* **No Hand-Holding:** This repository is built for intermediate to advanced developers. If you do not know how to handle low-level device flashing, take a step back.
* **Your Lifeline:** If you have no clue what you're doing have MSM Tool handy to get your device back into a working state. If you don't and brick your device it is not my responsibility. 
* **Test Signing:** The drivers used are completely unsigned. You **MUST** run `bcdedit /set testsigning on` in Windows ARM or your installation will instantly crash on boot.

---

## 👥 Reusing this Work (Attribution Policy)

This variant fixes severe hardware-level bugs that plague generic Snapdragon 855 ports on the 7T—specifically bypassing the charging power-negotiation panic (BSOD) and remapping panel variables to fix display freezing.

If you are an upstream developer, port maintainer, or community member cherry-picking these DSDT/ACPI patches or using this tree layout, **you are required by the BSD-2-Clause license to preserve copyright.** Please explicitly credit **URFAVF3MBOY** and link directly back to this repository:
`https://github.com`

---

## 📦 Releases & Pre-Packaged Bundles
Check the **Releases** tab for compiled assets. 
* *Planned Archive:* A pre-compiled `.7z` / `.zip` containing the stable UEFI boot image, matching driver configurations, and a native touch driver setup script so you don't need external USB peripherals during initial Windows setup.

---

## Status
Tested on a OnePlus 7T (HD1907 / converted to Global hardware variant) with this repository's image.

| Feature | Status |
|---|---|
| Boot | Working |
| Display and GPU acceleration | Working (Using hotdog drivers) |
| Touch | Working natively (No external mouse needed) |
| Wi-Fi | Working |
| Bluetooth | Working |
| Audio | Working |
| Sleep and wake | Working |
| Battery (level and status) | Working |
| Charging over USB-C (USB-C to USB-C, PD charger) | Working — Capped safely at ~3–4W to prevent power-negotiation BSOD |

**Known issues**

| Feature | Status |
|---|---|
| Charging from USB-A to USB-C chargers | Charges, but very slowly |
| USB host (OTG) | Sorta working, has stability issues |
| Cameras | Not working |
| Sensors | Not working (Will cause system instability if loaded) |

* **Not yet tested:** Cellular/Modem (Global firmware baseband partitions should map bands automatically, APN configuration packages may be required manually).

---

## Drivers
The Windows drivers used with this image come mostly from [n00b69/woa-op7](https://github.com/n00b69/woa-op7/tree/main). This repository only builds the UEFI firmware; it does not contain the raw Windows drivers.

---

## ACPI (DSDT)
The image uses one DSDT: `Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/ACPI/DSDT.aml`. It is the original Project Aloha DSDT with four critical edits, listed byte by byte in [`docs/dsdt/CHANGES.md`](docs/dsdt/CHANGES.md):

* **GPU:** A new `MSHW1004` panel device that `GPU0` depends on (enables hardware graphics acceleration without freezing).
* **Touch:** `TSC1` no longer waits for `TECC`, and its I2C address is adjusted to `0x20` (enables working native touchscreen).
* **Charging:** The USB-C sink power list is strictly limited to `5 V / 2.9 A` only (disables 9V/12V negotiation to kill the charging BSOD loop).

`docs/dsdt/DSDT.original.aml` is the unmodified original and `docs/dsdt/make_dsdt.py` rebuilds the DSDT from it. To change the DSDT, replace that one file and rebuild.

---

## What is in the tree

| Path | Content |
|---|---|
| `Platforms/SurfaceDuo1Pkg` | NoSb DSC, FDF, DEC, includes and **only** `Device/oneplus-hotdogb` |
| `Platforms/AndromedaPkg`, `QcomModulePkg`, `CranePkg`, `OpensslPkg`, `SurfaceDuoACPI` | Only the files the hotdogb build opens (plus every source file named in the INFs used) |
| `Silicon/QC/Sm8150` | SM8150 silicon package |
| `MU_BASECORE`, `Common/*`, `Silicon/Arm/MU_TIANO`, `Features/*` | Project Mu / edk2 files that are actually used; BaseTools C and Python sources; `MdePkg/Include` |
| `BootShim`, `ImageResources/emptyramdisk`, `tools/` | Boot image packaging infrastructure |

**Removed:** Every other device and SoC, the secure-boot variant (`SurfaceDuo1.dsc`), the Surface Duo / Epsilon / Zeta images (stripped ~100 MB of bloat), docs, CI files, Docker files, stuart scripts, unit tests, and unused submodule components.

---

## Credits
* [n00b69/woa-op7](https://github.com/n00b69/woa-op7) — Windows drivers and foundational OnePlus 7 series guides
* [Project Aloha](https://github.com/Project-Aloha) — `mu_aloha_platforms`, SM8150 platform core layout, and binaries
* [WOA-Project](https://github.com/WOA-Project) — Qualcomm ACPI tables
* [Project Mu](https://github.com/microsoft/mu_basecore) and [TianoCore EDK II](https://github.com/tianocore/edk2) — Firmware core framework
* OpenSSL

---

## Licences
Project Mu / edk2 (BSD-2-Clause-Patent), OpenSSL (Apache-2.0), the repository `LICENSE`, and the license files at the root of each included tree are fully preserved. Crypto binaries and `Binaries/` are fetched at build time. `Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/{Binaries,PatchedBinaries}` contain verified upstream prebuilt drivers.
