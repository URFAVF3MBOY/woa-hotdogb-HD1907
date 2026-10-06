# hotdogb-builder

A minimal, self-contained builder for the **OnePlus 7T (hotdogb, SM8150)** UEFI boot image
(Windows on ARM), cut down from `URFAVF3MBOY/mu_aloha_platforms` (a fork of Project-Aloha's
`mu_aloha_platforms`) to only what this one device needs.

```
bash build.sh                       # -> out/oneplus-hotdogb.img
fastboot boot out/oneplus-hotdogb.img
```

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

## How the trimmed tree was verified
The file list was recorded with `strace` during a full build of the upstream-style tree, completed with every
file named in the INF `[Sources]/[Binaries]` sections and the include dirs named in the DEC files (edk2 checks
those exist even when it does not compile them). A clean build from this tree then produced the same 107 modules
as the full tree, the same FVMAIN size (12,562,432 bytes), and 104 of 107 `.efi` files were byte-identical
(the other 3 differ only by build-path/timestamp content, which varies between any two builds).

**Not verified:** booting the resulting image on a phone, and Debug/NOOPT targets. Only RELEASE / NoSb was built.

## The DSDT
There is exactly one: `Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/ACPI/DSDT.aml`. It is the **sink5v29** DSDT
(md5 `3b893e9ba516f08d08957ce37d517bb9`): the `mshw1004` base with the USB-C sink PDO set to 5 V / 2.9 A.
It does **not** include the DCMA 900 → 1500 change. To try something else, replace that one file and rebuild.

## Licences
Project Mu / edk2 (BSD-2-Clause-Patent), OpenSSL (Apache-2.0), the fork's `LICENSE`, and the licence files that
sit at the root of each vendored tree are included. `Binaries/` and the crypto binaries are downloaded, not
redistributed. `Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/{Binaries,PatchedBinaries}` contain prebuilt
vendor drivers that come from the fork — check you are allowed to republish them before making this public.
