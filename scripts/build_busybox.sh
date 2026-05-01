#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <busybox-src> <output-binary>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$1"
OUT="$2"
CC_BIN="${BUSYBOX_CC:-musl-gcc}"

if [ ! -d "$SRC" ]; then
  echo "busybox source tree not found: $SRC" >&2
  exit 1
fi

SRC="$(cd "$SRC" && pwd)"
mkdir -p "$(dirname "$OUT")"
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"

if ! command -v "$CC_BIN" >/dev/null 2>&1; then
  echo "compiler not found: $CC_BIN" >&2
  exit 1
fi

for generated in lex.zconf.c zconf.tab.c zconf.hash.c; do
  if [ ! -f "$SRC/scripts/kconfig/$generated" ] && [ ! -f "$SRC/scripts/kconfig/${generated}_shipped" ]; then
    echo "missing generated Kconfig source: $SRC/scripts/kconfig/$generated" >&2
    exit 1
  fi
done

WORKDIR="$(mktemp -d /tmp/aos-busybox.XXXXXX)"
cleanup() {
  rm -rf "$WORKDIR"
}
trap cleanup EXIT

WORKSRC="$WORKDIR/src"
cp -a "$SRC" "$WORKSRC"

cd "$WORKSRC"
make mrproper >/dev/null

for generated in lex.zconf.c zconf.tab.c zconf.hash.c; do
  if [ -f "$SRC/scripts/kconfig/$generated" ]; then
    cp "$SRC/scripts/kconfig/$generated" "scripts/kconfig/$generated"
  else
    cp "$SRC/scripts/kconfig/${generated}_shipped" "scripts/kconfig/$generated"
  fi
done

make CC="$CC_BIN" allnoconfig >/dev/null

set_bool() {
  local sym="$1"
  local val="$2"
  if grep -q "^CONFIG_${sym}=" .config; then
    sed -i "s/^CONFIG_${sym}=.*/CONFIG_${sym}=${val}/" .config
  elif grep -q "^# CONFIG_${sym} is not set" .config; then
    if [ "$val" = "y" ]; then
      sed -i "s/^# CONFIG_${sym} is not set$/CONFIG_${sym}=y/" .config
    fi
  else
    if [ "$val" = "y" ]; then
      printf 'CONFIG_%s=y\n' "$sym" >> .config
    else
      printf '# CONFIG_%s is not set\n' "$sym" >> .config
    fi
  fi
}

set_str() {
  local sym="$1"
  local val="$2"
  if grep -q "^CONFIG_${sym}=" .config; then
    sed -i "s|^CONFIG_${sym}=.*|CONFIG_${sym}=\"${val}\"|" .config
  else
    printf 'CONFIG_%s="%s"\n' "$sym" "$val" >> .config
  fi
}

set_bool LONG_OPTS y
set_bool SHOW_USAGE y
set_bool FEATURE_COMPRESS_USAGE y
set_bool LFS y
set_bool TIME64 y
set_bool STATIC y
set_bool PIE n
set_bool USE_PORTABLE_CODE y
set_bool STATIC_LIBGCC y
set_bool BUSYBOX y
set_bool FEATURE_SHOW_SCRIPT y
set_str BUSYBOX_EXEC_PATH "busybox"
set_bool INSTALL_APPLET_DONT y
set_bool BASH_IS_NONE y
set_bool ASH y
set_bool ASH_OPTIMIZE_FOR_SIZE y
set_bool ASH_INTERNAL_GLOB y
set_bool ASH_BASH_COMPAT y
set_bool ASH_ECHO y
set_bool ASH_PRINTF y
set_bool ASH_TEST y
set_bool ASH_CMDCMD y
set_bool FEATURE_SH_MATH y
set_bool FEATURE_SH_MATH_64 y
set_bool FEATURE_SH_MATH_BASE y
set_bool FEATURE_SH_EXTRA_QUIET y
set_bool CAT y
set_bool ECHO y
set_bool PRINTF y
set_bool FEATURE_FANCY_ECHO y
set_bool ENV y
set_bool LS y
set_bool PWD y
set_bool TEST y
set_bool TEST1 y
set_bool TRUE y
set_bool FALSE y
set_bool HEAD y
set_bool TAIL y
set_bool UNAME y
set_str CROSS_COMPILER_PREFIX ""
set_str SYSROOT ""
set_str EXTRA_CFLAGS "-fcf-protection=none -march=x86-64 -mno-avx -mno-avx2 -mno-bmi -mno-bmi2"
set_str EXTRA_LDFLAGS ""
set_str EXTRA_LDLIBS ""

set +o pipefail
yes '' | make CC="$CC_BIN" oldconfig >/dev/null
set -o pipefail
make CC="$CC_BIN" -j"$(nproc)" busybox >/dev/null

python3 "$SCRIPT_DIR/prepare_busybox.py" busybox "$OUT" >/dev/null
