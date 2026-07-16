#!/bin/bash
# run-clean.sh — launch a Qt/KDE app with all style/look-and-feel env vars
# cleared. Echoes cleared vars and the active plasma-breeze RPM version so
# the terminal output can be captured in screenshot evidence.
#
# Usage: ./run-clean.sh /usr/bin/keepassxc [args...]
#        ./run-clean.sh /usr/bin/smplayer  [args...]
#        ./run-clean.sh /usr/bin/goldendict [args...]

BINARY="$1"
shift

if [[ -z "$BINARY" || ! -x "$BINARY" ]]; then
    echo "Usage: $0 /usr/bin/<app> [args...]"
    exit 1
fi

echo "========================================================"
echo "  Qt/Breeze clean-launch wrapper"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================================"
echo ""
echo "  Binary       : $BINARY"
echo "  plasma-breeze: $(rpm -q plasma-breeze 2>/dev/null || echo 'not installed')"
echo "  breeze-qt5   : $(rpm -q plasma-breeze-qt5 2>/dev/null || echo 'not installed')"
echo "  breeze-qt6   : $(rpm -q plasma-breeze-qt6 2>/dev/null || echo 'not installed')"
echo ""

# ---------------------------------------------------------------------------
# Vars that directly select or change the Qt/KDE visual style.
# Clearing these ensures the style comes exclusively from kdeglobals
# (via QT_QPA_PLATFORMTHEME=kde) — the normal user desktop path.
# ---------------------------------------------------------------------------
STYLE_VARS=(
    QT_STYLE_OVERRIDE           # forces a named QStyle regardless of kdeglobals
    QT_QPA_STYLE                # alternate spelling used by some Qt builds
    QT_QUICK_CONTROLS_STYLE     # Qt Quick style (Material, Fusion, etc.)
    QT_QUICK_CONTROLS_FALLBACK_STYLE
    KDESTYLE                    # KDE-specific style name override
)

# Vars that change colors, icons, or fonts independently of the style plugin.
THEME_VARS=(
    KDEDIRS                     # extra KDE data dirs — can shadow system icons/colors
    QT_FONT_DPI                 # overrides screen DPI → affects font metrics & layout
    QT_SCALE_FACTOR             # global HiDPI scale; changes widget sizes
    QT_SCREEN_SCALE_FACTORS     # per-screen scale
    QT_AUTO_SCREEN_SCALE_FACTOR # automatic HiDPI scale detection
    QT_ENABLE_HIGHDPI_SCALING
    XCURSOR_THEME               # cursor theme (cosmetic but visible in screenshots)
    XCURSOR_SIZE
    GTK2_RC_FILES               # GTK2 theme bleeding into Qt5 GTK integration
    GTK_THEME                   # GTK3/4 theme
)

# Vars that change Qt plugin/platform loading or enable debug output
# that can affect timing and layout.
PLATFORM_VARS=(
    QT_QPA_GENERIC_PLUGINS      # additional platform plugins
    QT_DEBUG_PLUGINS            # plugin debug output (cosmetic; safe to clear)
    BREEZE_DECORATION_DEBUG     # Breeze-specific debug flags
    KWIN_DPI                    # KWin DPI hint (affects decoration metrics)
)

echo "--- style / theme / platform vars (clearing) ---"
ALL_CLEAR_VARS=( "${STYLE_VARS[@]}" "${THEME_VARS[@]}" "${PLATFORM_VARS[@]}" )
any_set=0
for var in "${ALL_CLEAR_VARS[@]}"; do
    if [[ -n "${!var}" ]]; then
        printf "  %-42s was: %s  → unset\n" "$var" "${!var}"
        any_set=1
    fi
    unset "$var"
done
if [[ $any_set -eq 0 ]]; then
    echo "  (none were set — environment already clean)"
fi
echo ""

# ---------------------------------------------------------------------------
# Vars kept as-is — these establish the KDE session context that routes
# Qt5 and Qt6 style lookups through kdeglobals → Breeze plugin.
# Unsetting these would break the very path under test.
# ---------------------------------------------------------------------------
echo "--- session context (kept as-is) ---"
for var in QT_QPA_PLATFORMTHEME XDG_CURRENT_DESKTOP KDE_FULL_SESSION \
           KDE_SESSION_VERSION DISPLAY WAYLAND_DISPLAY XAUTHORITY \
           XDG_CONFIG_HOME XDG_DATA_HOME; do
    printf "  %-42s %s\n" "$var" "${!var:-(unset)}"
done
echo ""
echo "--- exec: $BINARY $* ---"
echo ""

exec "$BINARY" "$@"
