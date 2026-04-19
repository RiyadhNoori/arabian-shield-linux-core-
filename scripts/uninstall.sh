#!/usr/bin/env bash
# Arabian Shield Linux Core — Uninstall Script
# Usage: sudo bash uninstall.sh [--prefix /usr/local]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
SYSCONFDIR="${SYSCONFDIR:-/etc}"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[uninstall]${NC} $*"; }
warn()  { echo -e "${YELLOW}[uninstall]${NC} $*"; }
die()   { echo -e "${RED}[uninstall]${NC} $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)     PREFIX="$2";     shift 2 ;;
        --sysconfdir) SYSCONFDIR="$2"; shift 2 ;;
        *) die "Unknown option: $1" ;;
    esac
done

if [[ $EUID -ne 0 ]]; then
    die "Uninstall requires root. Run: sudo bash uninstall.sh"
fi

info "Removing Arabian Shield Linux Core from ${PREFIX}..."

# Remove libraries
for f in "${PREFIX}/lib/libarabic_shield.so"* \
          "${PREFIX}/lib/libarabic_shield.a"; do
    if [[ -f "$f" ]]; then
        rm -f "$f" && info "Removed: $f"
    fi
done

# Remove header
if [[ -d "${PREFIX}/include/arabic_shield" ]]; then
    rm -rf "${PREFIX}/include/arabic_shield"
    info "Removed: ${PREFIX}/include/arabic_shield/"
fi

# Remove pkg-config
if [[ -f "${PREFIX}/lib/pkgconfig/arabic-shield.pc" ]]; then
    rm -f "${PREFIX}/lib/pkgconfig/arabic-shield.pc"
    info "Removed: ${PREFIX}/lib/pkgconfig/arabic-shield.pc"
fi

# Remove Fontconfig rules
FC_CONF="${SYSCONFDIR}/fonts/conf.d/99-arabic-shield.conf"
if [[ -f "$FC_CONF" ]]; then
    rm -f "$FC_CONF"
    info "Removed: $FC_CONF"
    fc-cache -fv 2>/dev/null || true
fi

# Remove X11 resources
XRES="${SYSCONFDIR}/X11/Xresources.d/99-arabic-shield.conf"
if [[ -f "$XRES" ]]; then
    rm -f "$XRES"
    info "Removed: $XRES"
fi

ldconfig 2>/dev/null || true

info "Arabian Shield Linux Core uninstalled."
warn "Note: Arabic fonts installed by fonts/download_fonts.sh were NOT removed."
warn "To remove those, delete /usr/local/share/fonts/arabian-shield/ manually."
