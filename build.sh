#!/usr/bin/env bash
# Builds the OnePlus 7T (hotdogb, SM8150) UEFI boot image.  Output: out/oneplus-hotdogb.img
#   ./build.sh                          build with the DSDT that is in the repo
#   DSDT=/path/to/DSDT.aml ./build.sh   build with your own DSDT (copied over the device's ACPI/DSDT.aml)
#   JOBS=8 ./build.sh                   parallel jobs (default: nproc)
#   OUT_SUFFIX=_v29 ./build.sh          output name suffix -> out/oneplus-hotdogb_v29.img
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$PWD"
DEVICE=oneplus-hotdogb
PKG=SurfaceDuo1Pkg
TARGET=RELEASE            # only RELEASE is tested
UEFI_BASE=0x9FC00000      # from the sm8150 config of the original project
UEFI_SIZE=0x00300000
JOBS="${JOBS:-$(nproc)}"

for t in clang lld-link llvm-lib llvm-rc llvm-objcopy gcc g++ make python3 git curl; do
  command -v "$t" >/dev/null || { echo "missing tool: $t  (see README: Requirements)"; exit 1; }
done
[ -f /usr/include/uuid/uuid.h ] || { echo "missing libuuid headers (apt install uuid-dev)"; exit 1; }

./scripts/fetch_deps.sh

if [ -n "${DSDT:-}" ]; then
  cp "$DSDT" "Platforms/$PKG/Device/$DEVICE/ACPI/DSDT.aml"
  echo "[build] using DSDT: $DSDT  (md5 $(md5sum "$DSDT" | cut -d' ' -f1))"
fi

# --- BaseTools (C tools) ---
BT="$ROOT/MU_BASECORE/BaseTools"
if [ ! -x "$BT/Source/C/bin/GenFv" ]; then
  make -C "$BT/Source/C" -j"$JOBS"
fi

# --- edk2 environment ---
mkdir -p Conf
for f in build_rule tools_def target; do
  [ -f "Conf/$f.txt" ] || cp "$BT/Conf/$f.template" "Conf/$f.txt"
done
export WORKSPACE="$ROOT" EDK_TOOLS_PATH="$BT" BASE_TOOLS_PATH="$BT" CONF_PATH="$ROOT/Conf"
export PYTHONPATH="$BT/Source/Python"
export PATH="$BT/BinWrappers/PosixLike:$BT/Source/C/bin:$PATH"
export CLANG_BIN=/usr/bin/ CLANGPDB_BIN=/usr/bin/
export PACKAGES_PATH="$ROOT/Platforms:$ROOT/MU_BASECORE:$ROOT/Common/MU:$ROOT/Common/MU_TIANO:$ROOT/Common/MU_OEM_SAMPLE:$ROOT/Silicon/Arm/MU_TIANO:$ROOT/Features/DFCI:$ROOT/Features/CONFIG:$ROOT/Binaries:$ROOT/Silicon/QC/Sm8150"

python3 "$BT/Source/Python/build/build.py" \
  -p "$PKG/SurfaceDuo1NoSb.dsc" -a AARCH64 -t CLANGPDB -b "$TARGET" -n "$JOBS" \
  -D TARGET_DEVICE=$DEVICE -D FDT=hotdogb.dtb -D BUILDID_STRING=Unknown \
  -D MEMORY_PROTECTION=TRUE -D SHIP_MODE=FALSE -D EMPTY_DRIVE=FALSE \
  -D SHARED_CRYPTO_PATH=CryptoPkg/Binaries/edk2-basecrypto-driver-bin_extdep

# --- BootShim + Android boot image ---
rm -f BootShim/BootShim.elf BootShim/BootShim.bin
make -C BootShim UEFI_BASE=$UEFI_BASE UEFI_SIZE=$UEFI_SIZE
mkdir -p out
python3 - <<PY
import os, sys
sys.path.insert(0, "tools")
import PostBuild
b = "Build/$PKG/${TARGET}_CLANGPDB"
PostBuild.makeAndroidImage(b, "out", os.getcwd(), "$DEVICE", "hotdogb.dtb")
PY
OUT="out/$DEVICE${OUT_SUFFIX:-}.img"
[ "$OUT" = "out/$DEVICE.img" ] || mv "out/$DEVICE.img" "$OUT"
echo
echo "Built: $OUT   md5 $(md5sum "$OUT" | cut -d' ' -f1)"
echo "Test it with:  fastboot boot $OUT"
