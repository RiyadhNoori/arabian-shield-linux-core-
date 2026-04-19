#!/usr/bin/env bash
# Arabian Shield Linux Core — Installation Script
# Usage: sudo bash install.sh [--prefix /usr/local] [--post-install] [--dry-run]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# ── Defaults ────────────────────────────────────────────────────────────
PREFIX="${PREFIX:-/usr/local}"
SYSCONFDIR="${SYSCONFDIR:-/etc}"
LIBDIR="${PREFIX}/lib"
INCLUDEDIR="${PREFIX}/include/arabic_shield"
FONTCFGDIR="${SYSCONFDIR}/fonts/conf.d"
XRESDIR="${SYSCONFDIR}/X11/Xresources.d"
DRY_RUN=false
POST_INSTALL=false

# ── Colours ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()    { echo -e "${GREEN}[install]${NC} $*"; }
warn()    { echo -e "${YELLOW}[install]${NC} $*"; }
error()   { echo -e "${RED}[install]${NC} $*" >&2; }
die()     { error "$*"; exit 1; }

# ── Argument parsing ─────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)      PREFIX="$2";     shift 2 ;;
        --sysconfdir)  SYSCONFDIR="$2"; shift 2 ;;
        --post-install) POST_INSTALL=true; shift ;;
        --dry-run)     DRY_RUN=true;    shift ;;
        -h|--help)
            echo "Usage: $0 [--prefix DIR] [--sysconfdir DIR] [--dry-run]"
            exit 0
            ;;
        *) die "Unknown option: $1" ;;
    esac
done

# ── Helpers ──────────────────────────────────────────────────────────────
run() {
    if $DRY_RUN; then
        echo "  [dry-run] $*"
    else
        "$@"
    fi
}

require_root() {
    if [[ $EUID -ne 0 ]]; then
        die "System-wide installation requires root. Run: sudo bash install.sh"
    fi
}

check_dependencies() {
    info "Checking dependencies..."
    local missing=()
    for lib in libpango-1.0 libfribidi libharfbuzz libfontconfig; do
        if ! pkg-config --exists "${lib}" 2>/dev/null; then
            missing+=("${lib}-dev")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        warn "Missing development libraries: ${missing[*]}"
        warn "Install with: sudo apt-get install ${missing[*]}"
        warn "Continuing anyway — some features may not work."
    else
        info "All dependencies found."
    fi
}

# ── Post-install steps ───────────────────────────────────────────────────
post_install() {
    info "Running post-install steps..."

    # Refresh font cache
    info "Refreshing font cache (fc-cache)..."
    run fc-cache -fv 2>/dev/null || warn "fc-cache failed — run manually: fc-cache -fv"

    # Reload X11 resources if X is running
    if [[ -n "${DISPLAY:-}" ]]; then
        info "Reloading X11 resources..."
        if [[ -f "${XRESDIR}/99-arabic-shield.conf" ]]; then
            run xrdb -merge "${XRESDIR}/99-arabic-shield.conf" 2>/dev/null || \
                warn "xrdb failed — restart your session for X11 changes to take effect"
        fi
    else
        warn "DISPLAY not set — X11 resources will apply on next login"
    fi

    # Verify at least one Arabic font is available
    info "Checking Arabic font availability..."
    if fc-list :lang=ar | grep -qi "noto\|amiri\|scheherazade\|cairo"; then
        info "Arabic fonts found."
    else
        warn "No recommended Arabic fonts found!"
        warn "Run: bash fonts/download_fonts.sh"
    fi

    # Optional: restart IBus for Arabic input
    if command -v ibus &>/dev/null; then
        info "Restarting IBus input method..."
        run ibus restart 2>/dev/null || true
    fi

    info "Post-install complete."
}

# ── Main installation ────────────────────────────────────────────────────
main() {
    if $POST_INSTALL; then
        post_install
        exit 0
    fi

    require_root
    check_dependencies

    local SCRIPT_DIR
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local REPO_ROOT
    REPO_ROOT="$(dirname "$SCRIPT_DIR")"

    info "Installing Arabian Shield Linux Core to ${PREFIX}..."

    # Check build artifacts exist
    if [[ ! -f "${REPO_ROOT}/libarabic_shield.so."* ]] && \
       [[ ! -f "${REPO_ROOT}/libarabic_shield.a" ]]; then
        die "Build artifacts not found. Run 'make' first."
    fi

    # Create directories
    run install -d "${LIBDIR}"
    run install -d "${INCLUDEDIR}"
    run install -d "${FONTCFGDIR}"
    run install -d "${XRESDIR}" 2>/dev/null || true

    # Install shared library
    local SOFILE
    SOFILE=$(ls "${REPO_ROOT}"/libarabic_shield.so.* 2>/dev/null | head -1)
    if [[ -n "$SOFILE" ]]; then
        local SONAME
        SONAME=$(basename "$SOFILE")
        run install -m 755 "$SOFILE" "${LIBDIR}/${SONAME}"
        run ln -sf "${SONAME}" "${LIBDIR}/libarabic_shield.so"
        run ldconfig "${LIBDIR}" 2>/dev/null || true
        info "Installed shared library: ${LIBDIR}/${SONAME}"
    fi

    # Install static library
    if [[ -f "${REPO_ROOT}/libarabic_shield.a" ]]; then
        run install -m 644 "${REPO_ROOT}/libarabic_shield.a" "${LIBDIR}/"
        info "Installed static library: ${LIBDIR}/libarabic_shield.a"
    fi

    # Install header
    run install -m 644 "${REPO_ROOT}/src/arabic_shield.h" "${INCLUDEDIR}/"
    info "Installed header: ${INCLUDEDIR}/arabic_shield.h"

    # Install pkg-config file
    cat > /tmp/arabic-shield.pc << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include/arabic_shield

Name: arabic-shield
Description: Arabian Shield Linux Core — Arabic text rendering middleware
Version: 0.1.0
Libs: -L\${libdir} -larabic_shield
Cflags: -I\${includedir}
Requires: pango fribidi harfbuzz fontconfig freetype2
EOF
    run install -d "${PREFIX}/lib/pkgconfig"
    run install -m 644 /tmp/arabic-shield.pc "${PREFIX}/lib/pkgconfig/arabic-shield.pc"
    info "Installed pkg-config: arabic-shield.pc"

    # Install Fontconfig rules
    run install -m 644 "${REPO_ROOT}/config/fonts.conf" \
                       "${FONTCFGDIR}/99-arabic-shield.conf"
    info "Installed Fontconfig rules: ${FONTCFGDIR}/99-arabic-shield.conf"

    # Install X11 resources
    if [[ -d "$XRESDIR" ]]; then
        run install -m 644 "${REPO_ROOT}/config/99-arabic-shield.conf" \
                           "${XRESDIR}/99-arabic-shield.conf"
        info "Installed X11 resources: ${XRESDIR}/99-arabic-shield.conf"
    else
        warn "X11 Xresources.d directory not found at ${XRESDIR} — skipping"
    fi

    # Run post-install steps
    post_install

    echo ""
    info "════════════════════════════════════════════════"
    info " Arabian Shield Linux Core installed successfully"
    info " Version: 0.1.0"
    info " Prefix:  ${PREFIX}"
    info "════════════════════════════════════════════════"
    echo ""
    info "To use in your application:"
    echo "  pkg-config --cflags --libs arabic-shield"
    echo ""
    info "To verify installation:"
    echo "  bash ${REPO_ROOT}/scripts/test_render.sh"
}

main "$@"
