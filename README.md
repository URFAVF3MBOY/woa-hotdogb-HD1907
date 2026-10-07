# OnePlus 7T (hotdogb) UEFI builder

Builds the UEFI boot image that runs **Windows 11 on ARM** on the **OnePlus 7T (hotdogb, Snapdragon 855 / SM8150)**.
It is a trimmed, single-device version of [Project Aloha's `mu_aloha_platforms`](https://github.com/Project-Aloha/mu_aloha_platforms),
containing only what the hotdogb build needs.

```
bash build.sh                       # -> out/oneplus-hotdogb.img
fastboot boot out/oneplus-hotdogb.img
```

## Status
Tested on a OnePlus 7T with this repository's image and Windows 11 ARM64 (25H2).

> **Requires test signing.** The drivers used with this image are unsigned, so Windows must have test signing turned on
> (`bcdedit /set testsigning on`, then reboot).

| Feature | Status |
|---|---|
| Boot (UEFI → Windows 11 ARM64) | Working |
| Display and GPU acceleration | Working |
| Touch | Working |
| Wi-Fi | Working |
| Bluetooth | Working |
| Audio | Working |
| Sleep and wake (screen off when idle, power button wakes) | Working |
| Battery (level and status) | Working |
| Charging over USB-C (USB-C to USB-C, PD charger) | Working — roughly 3–4 W; faster charging is still being worked on |

**Known issues**

| Feature | Status |
|---|---|
| Charging from USB-A to USB-C chargers | Charges, but very slowly |
| USB host (OTG) | Not working yet — a fix is in progress |
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

* **GPU:** a new `MSHW1004` panel device that `GPU0` depends on (graphics acceleration with the Surface Duo graphics driver)
* **Touch:** `TSC1` no longer waits for `TECC`, and its I2C address is `0x20`
* **Charging:** the USB-C sink power list is 5 V / 2.9 A only (no 9 V or 12 V)

`docs/dsdt/DSDT.original.aml` is the unmodified original and `docs/dsdt/make_dsdt.py` rebuilds the DSDT from it.
To change the DSDT, replace that one file and rebuild.

## Building on GitHub (no Linux machine needed)
1. Create a new repository and push this folder to it. It is about 95 MB with no file over 20 MB, so Git LFS is not needed.
2. Open **Actions → Build hotdogb UEFI → Run workflow** (it also runs on every push to `main`).
3. Download the `oneplus-hotdogb-uefi` artifact from the finished run: `oneplus-hotdogb.img` plus `SHA256SUMS`.
   Rename the image to `windows.img` if your flashing steps expect that name.

Pushing a tag such as `v1` also attaches the image to a GitHub Release.
First run takes about 10 minutes (it downloads the 255 MB crypto package); later runs reuse a cache.

## Requirements
Tested on Ubuntu 24.04-like Linux with clang 18, Python 3.13 (standard library only — no pip packages,
no `stuart`/`edk2-pytool`, no nuget, no mono):

```
sudo apt install clang lld llvm build-essential uuid-dev python3 git curl
```
`build.sh` checks for `clang lld-link llvm-lib llvm-rc llvm-objcopy gcc g++ make python3 git curl` and libuuid headers.
Disk: about 90 MB for the repo, plus about 1.5 GB while the crypto package downloads and about 1 GB build output.

## What `build.sh` does
1. `scripts/fetch_deps.sh` downloads the two things that are deliberately not stored here:
   * `Mu_CryptoBin_v1_1_3.zip` from microsoft/mu_crypto_release (sha256 verified; only 11 needed files extracted)
   * `Binaries/` = Project-Aloha/SurfaceDuoBinaries-fork at commit `ce59dcf5…` (prebuilt EFI drivers)
2. Builds the edk2 C tools (`GenFv`, `GenFw`, …) from `MU_BASECORE/BaseTools/Source/C`.
3. Runs the edk2 `build.py` for `SurfaceDuo1Pkg/SurfaceDuo1NoSb.dsc` (AARCH64, CLANGPDB, RELEASE,
   `TARGET_DEVICE=oneplus-hotdogb`) — the same defines the original `PlatformBuildNoSb.py` sets.
4. Builds `BootShim` (base `0x9FC00000`, size `0x300000`) and packs shim + firmware + the Android DTB into an Android boot image (`tools/PostBuild.py`, `tools/mkbootimg.py`).

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
