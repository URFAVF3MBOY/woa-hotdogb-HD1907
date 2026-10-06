#!/usr/bin/env bash
# Build one or more images in a row (used by the GitHub workflow, also fine to run locally).
#   scripts/ci_build.sh repo-default                    the DSDT that is committed in Platforms/.../ACPI
#   scripts/ci_build.sh sink5v29 sink5v29_dcma1500      names of files in dsdt/ (matched on the suffix)
#   scripts/ci_build.sh all                             repo-default + every file in dsdt/
#   scripts/ci_build.sh path/to/my.aml                  any .aml file
# Each result lands in out/ as oneplus-hotdogb_<name>.img, plus out/SHA256SUMS.
set -euo pipefail
cd "$(dirname "$0")/.."
[ $# -gt 0 ] || { echo "usage: $0 repo-default | all | <dsdt name or path>..."; exit 1; }

want=()
for a in "$@"; do
  if [ "$a" = all ]; then
    want+=(repo-default)
    for f in dsdt/*.aml; do want+=("$f"); done
  else
    want+=("$a")
  fi
done

orig_dsdt="Platforms/SurfaceDuo1Pkg/Device/oneplus-hotdogb/ACPI/DSDT.aml"
cp "$orig_dsdt" /tmp/DSDT.repo-default.aml      # restored at the end so the checkout stays clean
trap 'cp /tmp/DSDT.repo-default.aml "$orig_dsdt"' EXIT

mkdir -p out
for w in "${want[@]}"; do
  if [ "$w" = repo-default ]; then
    name=repo-default; unset DSDT || true
  else
    f=""
    if [ -f "$w" ]; then f="$w"
    else
      for c in "dsdt/$w" "dsdt/$w.aml" "dsdt/DSDT_hotdogb_$w.aml" "dsdt/DSDT_hotdogb_mshw1004_$w.aml"; do [ -f "$c" ] && { f="$c"; break; }; done
    fi
    [ -n "$f" ] || { echo "no such DSDT: $w"; exit 1; }
    name="$(basename "$f" .aml)"; name="${name#DSDT_hotdogb_}"
    export DSDT="$PWD/$f"
  fi
  echo "=================== $name ==================="
  OUT_SUFFIX="_$name" ./build.sh
done
( cd out && sha256sum oneplus-hotdogb_*.img > SHA256SUMS && cat SHA256SUMS )
