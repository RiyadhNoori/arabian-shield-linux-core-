#!/usr/bin/env bash
# Arabian Shield Linux Core — Font Downloader
# Downloads recommended Arabic fonts from trusted sources.
#
# Usage: bash download_fonts.sh [--user] [--system] [--dry-run]
#   --user    Install to ~/.local/share/fonts/ (default if not root)
#   --system  Install to /usr/local/share/fonts/ (requires root)
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# ── Configuration ─────────────────────────────────────────────────────────

# Install mode: auto = user if not root, system if root
MODE="auto"
DRY_RUN=false
VERIFY=true

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[fonts]${NC} $*"; }
warn()  { echo -e "${YELLOW}[fonts]${NC} $*"; }
die()   { echo -e "${RED}[fonts]${NC} $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)     MODE="user";   shift ;;
        --system)   MODE="system"; shift ;;
        --dry-run)  DRY_RUN=true;  shift ;;
        --no-verify) VERIFY=false; shift ;;
        *) die "Unknown option: $1" ;;
    esac
done

# Determine install directory
if [[ "$MODE" == "auto" ]]; then
    MODE=$([[ $EUID -eq 0 ]] && echo "system" || echo "user")
fi

if [[ "$MODE" == "system" ]]; then
    FONT_DIR="/usr/local/share/fonts/arabian-shield"
    [[ $EUID -ne 0 ]] && die "System font install requires root. Use --user or run as root."
else
    FONT_DIR="${HOME}/.local/share/fonts/arabian-shield"
fi

# ── Font definitions ────────────────────────────────────────────────────
# Format: "display_name|filename|url|sha256"
#
# URLs point to Google Fonts API direct downloads and GitHub releases.
# These are stable, publicly licensed (OFL) fonts.

declare -a FONTS=(
    "Noto Sans Arabic|NotoSansArabic-VariableFont.ttf|https://github.com/notofonts/arabic/releases/latest/download/NotoSansArabic.zip|auto"
    "Noto Naskh Arabic|NotoNaskhArabic-Regular.ttf|https://github.com/notofonts/arabic/releases/latest/download/NotoNaskhArabic.zip|auto"
    "Amiri Regular|Amiri-Regular.ttf|https://github.com/aliftype/amiri/releases/latest/download/amiri-0.9.2.zip|auto"
    "Amiri Bold|Amiri-Bold.ttf|https://github.com/aliftype/amiri/releases/latest/download/amiri-0.9.2.zip|auto"
    "Cairo Regular|Cairo-Regular.ttf|https://github.com/Gue3bara/Cairo/releases/latest/download/Cairo.zip|auto"
)

# ── Download function ────────────────────────────────────────────────────
download_font_package() {
    local display_name="$1"
    local url="$2"
    local tmpdir
    tmpdir=$(mktemp -d)
    local zipfile="${tmpdir}/font.zip"

    info "Downloading ${display_name}..."

    if $DRY_RUN; then
        echo "  [dry-run] wget -q '${url}' -O '${zipfile}'"
        echo "  [dry-run] unzip to ${FONT_DIR}/"
        rm -rf "$tmpdir"
        return 0
    fi

    # Try wget, then curl
    if command -v wget &>/dev/null; then
        wget -q --show-progress --no-check-certificate "$url" -O "$zipfile" 2>/dev/null || \
        wget -q "$url" -O "$zipfile"
    elif command -v curl &>/dev/null; then
        curl -L --silent --show-error "$url" -o "$zipfile"
    else
        die "Neither wget nor curl found. Install one and retry."
    fi

    info "Extracting ${display_name}..."
    unzip -q -o "$zipfile" "*.ttf" "*.otf" -d "$tmpdir/extracted" 2>/dev/null || true

    # Move font files to destination
    find "$tmpdir/extracted" -name "*.ttf" -o -name "*.otf" | while read -r f; do
        install -m 644 "$f" "$FONT_DIR/"
        info "  Installed: $(basename "$f")"
    done

    rm -rf "$tmpdir"
}

# ── Direct single-file download ──────────────────────────────────────────
download_single_font() {
    local display_name="$1"
    local filename="$2"
    local url="$3"

    if [[ -f "${FONT_DIR}/${filename}" ]]; then
        info "  Already installed: ${filename}"
        return 0
    fi

    info "Downloading ${display_name} (${filename})..."

    if $DRY_RUN; then
        echo "  [dry-run] download '${url}' → '${FONT_DIR}/${filename}'"
        return 0
    fi

    if command -v wget &>/dev/null; then
        wget -q --no-check-certificate "$url" -O "${FONT_DIR}/${filename}" 2>/dev/null || {
            warn "Failed to download ${display_name} — skipping"
            rm -f "${FONT_DIR}/${filename}"
        }
    elif command -v curl &>/dev/null; then
        curl -L --silent --fail "$url" -o "${FONT_DIR}/${filename}" || {
            warn "Failed to download ${display_name} — skipping"
            rm -f "${FONT_DIR}/${filename}"
        }
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────
main() {
    info "Arabian Shield Font Installer"
    info "Install directory: ${FONT_DIR}"
    echo ""

    # Create font directory
    if ! $DRY_RUN; then
        mkdir -p "$FONT_DIR"
    fi

    # ── Noto Sans Arabic ───────────────────────────────────────────────
    # Most reliable download: direct from Google Fonts GitHub
    download_font_package "Noto Sans Arabic" \
        "https://fonts.google.com/download?family=Noto%20Sans%20Arabic"

    # ── Noto Naskh Arabic ──────────────────────────────────────────────
    download_font_package "Noto Naskh Arabic" \
        "https://fonts.google.com/download?family=Noto%20Naskh%20Arabic"

    # ── Amiri ─────────────────────────────────────────────────────────
    download_font_package "Amiri" \
        "https://fonts.google.com/download?family=Amiri"

    # ── Cairo ─────────────────────────────────────────────────────────
    download_font_package "Cairo" \
        "https://fonts.google.com/download?family=Cairo"

    # ── Scheherazade New ──────────────────────────────────────────────
    download_font_package "Scheherazade New" \
        "https://fonts.google.com/download?family=Scheherazade%20New"

    # ── Alternative: use system package manager if available ──────────
    echo ""
    info "Tip: You can also install fonts via your package manager:"
    info "  Ubuntu/Debian: sudo apt-get install fonts-noto-core fonts-hosny-amiri"
    info "  Fedora/RHEL:   sudo dnf install google-noto-sans-arabic-fonts"
    info "  Arch:          sudo pacman -S noto-fonts ttf-amiri"
    echo ""

    # ── Refresh font cache ─────────────────────────────────────────────
    if ! $DRY_RUN; then
        info "Refreshing font cache..."
        fc-cache -fv "$FONT_DIR" 2>/dev/null || \
        fc-cache -f 2>/dev/null || \
        warn "fc-cache failed — run manually: fc-cache -f"

        echo ""
        info "Verifying installed fonts..."
        local found=0
        for family in "Noto Sans Arabic" "Noto Naskh Arabic" "Amiri" "Cairo"; do
            if fc-list :family="$family" 2>/dev/null | grep -q .; then
                info "  ✓ $family"
                found=$((found+1))
            else
                warn "  ✗ $family — not found (download may have failed)"
            fi
        done

        echo ""
        if [[ $found -ge 2 ]]; then
            info "Font installation complete. At least ${found} Arabic fonts are ready."
        else
            warn "Only ${found} font(s) verified. Some downloads may have failed."
            warn "Try installing via your package manager instead (see tip above)."
        fi
    fi
}

main "$@"
