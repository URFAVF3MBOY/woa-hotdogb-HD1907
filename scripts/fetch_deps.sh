#!/usr/bin/env bash
# Fetches the two things that are intentionally NOT stored in this repo:
#   1. Mu crypto driver binaries (pinned GitHub release, sha256-verified, only the needed members are extracted)
#      (set CRYPTO_ZIP=/path/to/Mu_CryptoBin_v1_1_3.zip to use a copy you already downloaded)
#   2. Binaries/ (prebuilt Qualcomm/Surface Duo EFI drivers) from Project-Aloha/SurfaceDuoBinaries-fork at a pinned commit
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CRYPTO_URL="https://github.com/microsoft/mu_crypto_release/releases/download/v1.1.3/Mu_CryptoBin_v1_1_3.zip"
CRYPTO_SHA="2136c410dc44135830e2e33062243563d642e0be868d6affc2930c6b0854f5cf"
CRYPTO_DIR="MU_BASECORE/CryptoPkg/Binaries/edk2-basecrypto-driver-bin_extdep"
BIN_URL="https://github.com/Project-Aloha/SurfaceDuoBinaries-fork"
BIN_COMMIT="ce59dcf54753e112d7b847dd997d3ae934094e0d"

if [ ! -f "$CRYPTO_DIR/STANDARD/RELEASE/AARCH64/CryptoDxe.efi" ]; then
  echo "[deps] getting crypto binaries (~255 MB download)..."
  TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
  if [ -n "${CRYPTO_ZIP:-}" ]; then
    cp "$CRYPTO_ZIP" "$TMP/crypto.zip"
  else
    curl -fL --retry 5 --retry-all-errors -C - -o "$TMP/crypto.zip" "$CRYPTO_URL"
  fi
  echo "$CRYPTO_SHA  $TMP/crypto.zip" | sha256sum -c -
  mkdir -p "$CRYPTO_DIR"
  python3 - "$TMP/crypto.zip" "$CRYPTO_DIR" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); out = sys.argv[2]
want = [
 "License.txt",
 "Driver/Bin/CryptoDriver.inc.dsc",
 "Driver/Bin/Crypto.pcd.STANDARD.inc.dsc",
 "Driver/Bin/CryptoDriver.DXE.inc.fdf",
 "Driver/Bin/CryptoDriver.RUNTIMEDXE.inc.fdf",
 "Driver/Bin/CryptoDriverBin_STANDARD_Dxe_RELEASE_AARCH64.inf",
 "Driver/Bin/CryptoDriverBin_STANDARD_RuntimeDxe_RELEASE_AARCH64.inf",
 "STANDARD/RELEASE/AARCH64/CryptoDxe.efi",
 "STANDARD/RELEASE/AARCH64/CryptoDxe.depex",
 "STANDARD/RELEASE/AARCH64/CryptoRuntimeDxe.efi",
 "STANDARD/RELEASE/AARCH64/CryptoRuntimeDxe.depex",
]
names = set(z.namelist())
for w in want:
    if w not in names: sys.exit("missing in zip: " + w)
    z.extract(w, out)
print("extracted", len(want), "files")
PY
fi

if [ ! -f Binaries/.fetched ]; then
  echo "[deps] fetching Binaries @ $BIN_COMMIT ..."
  rm -rf Binaries && mkdir Binaries && cd Binaries
  git init -q && git remote add origin "$BIN_URL"
  GIT_LFS_SKIP_SMUDGE=1 git fetch -q --depth 1 origin "$BIN_COMMIT"
  git checkout -q FETCH_HEAD
  touch .fetched
fi
echo "[deps] ok"
