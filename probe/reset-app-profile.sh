#!/bin/bash
# reset-app-profile.sh — reset a test application's persisted state so it starts
# from pristine defaults, without disturbing the KDE desktop configuration.
#
# Purpose
# -------
# Reproducing this bug requires knowing the width of the item view being
# measured. Qt applications persist window and dock geometry between sessions,
# so a view's width in your session is not necessarily the width another person
# sees on first launch. Resetting an application's own state before a capture
# makes "default geometry" a verifiable claim rather than an assumption.
#
# Why not XDG_CONFIG_HOME=$(mktemp -d)?
# -------------------------------------
# That is the obvious approach and it is subtly wrong for this purpose.
# kdeglobals lives in ~/.config, and the KDE platform theme reads it to resolve
# which QStyle a Qt application loads. Redirecting XDG_CONFIG_HOME hides
# kdeglobals, so the application may resolve a *different* style than the one
# you intended to test — which is the single variable this bug report is
# measuring. Redirecting HOME has the same problem, more broadly.
#
# This script therefore touches only the target application's own directories
# and leaves the desktop configuration alone.
#
# Safety
# ------
# The default action MOVES directories aside with a .bak-<timestamp> suffix and
# can be undone with --restore. Deletion happens only if you pass --purge.
#
# Usage
# -----
#   ./reset-app-profile.sh --list    <app>   # show what would change; changes nothing
#   ./reset-app-profile.sh           <app>   # move aside (reversible; default)
#   ./reset-app-profile.sh --restore <app>   # move the newest backup back
#   ./reset-app-profile.sh --purge   <app>   # DELETE permanently (asks first)
#
#   <app> is one of: goldendict | smplayer | keepassxc
#
# Companion to run-clean.sh, which clears style/theme environment variables and
# records the installed plasma-breeze version. Typical sequence:
#
#   ./reset-app-profile.sh goldendict
#   ./run-clean.sh /usr/bin/goldendict-ng

set -u

STAMP=$(date +%Y%m%d-%H%M%S)
MODE=move

case "${1:-}" in
    --list)    MODE=list;    shift ;;
    --restore) MODE=restore; shift ;;
    --purge)   MODE=purge;   shift ;;
esac

APP="${1:-}"
if [[ -z "$APP" ]]; then
    echo "Usage: $0 [--list|--restore|--purge] <goldendict|smplayer|keepassxc>"
    exit 1
fi

case "$APP" in
    goldendict)
        # Verified 2026-07-29 on Fedora 43: goldendict (Qt5) and goldendict-ng
        # (Qt6) both use the org/app name "goldendict" and therefore SHARE these
        # paths, including persisted window and dock geometry. Resetting one
        # resets the other. If you intend to compare the two applications, reset
        # between them or you are comparing two programs that have been
        # overwriting each other's layout.
        TARGETS=( "$HOME/.config/goldendict" \
                  "$HOME/.local/share/goldendict" \
                  "$HOME/.cache/goldendict" \
                  "$HOME/.goldendict" )
        echo "NOTE: goldendict and goldendict-ng share these paths — both are reset."
        ;;
    smplayer)
        TARGETS=( "$HOME/.config/smplayer" "$HOME/.config/smplayerrc" )
        ;;
    keepassxc)
        TARGETS=( "$HOME/.config/keepassxc" "$HOME/.cache/keepassxc" )
        echo "NOTE: this resets KeePassXC interface settings, including the"
        echo "      recent-database list and window geometry."
        echo "      It does NOT touch any .kdbx database file. Database files"
        echo "      live wherever you saved them and are never referenced here."
        ;;
    *)
        echo "Unknown app: $APP"
        echo "Supported: goldendict | smplayer | keepassxc"
        exit 1 ;;
esac

echo

# Banner printed for state-changing modes. Because captures for this bug report
# screenshot the terminal alongside the application, this line remains in
# scrollback above run-clean.sh's header — so one image documents both that the
# profile was reset and which plasma-breeze build was active.
banner() {
    echo "========================================================"
    echo "  Application profile reset — $1"
    echo "  $(date '+%Y-%m-%d %H:%M:%S')"
    echo "========================================================"
    echo "  App          : $APP"
    echo "  plasma-breeze: $(rpm -q plasma-breeze 2>/dev/null || echo 'n/a (not an rpm system)')"
    echo ""
    # Style-selecting environment variables. run-clean.sh clears these at launch
    # and reports their prior values; this is a read-only pre-check so the reset
    # banner is self-contained if the two scripts are ever run separately.
    # Read-only pre-check. Quiet when clean: one line naming every variable
    # checked, so the capture is still verifiable without a wall of "(unset)".
    # Expands to per-variable detail only when something is actually set.
    local vars=( QT_STYLE_OVERRIDE QT_QPA_STYLE QT_QUICK_CONTROLS_STYLE KDESTYLE QT_QPA_PLATFORMTHEME )
    local set_list=()
    for v in "${vars[@]}"; do
        [[ -n "${!v:-}" ]] && set_list+=( "$v" )
    done
    if [[ ${#set_list[@]} -eq 0 ]]; then
        echo "  style env    : all unset (${vars[*]})"
    else
        echo "  style env    : ${#set_list[@]} of ${#vars[@]} SET — style resolution may not be from kdeglobals:"
        for v in "${set_list[@]}"; do
            echo "                 $v = ${!v}"
        done
        echo "                 run-clean.sh clears these before launching."
    fi
    echo ""
}

case "$MODE" in
list)
    for t in "${TARGETS[@]}"; do
        if [[ -e "$t" ]]; then echo "  present: $t"; else echo "  absent : $t"; fi
    done
    echo
    echo "No changes made."
    ;;

move)
    banner "moved aside (reversible)"
    for t in "${TARGETS[@]}"; do
        if [[ -e "$t" ]]; then
            mv -v "$t" "${t}.bak-${STAMP}"
        else
            echo "  absent (nothing to do): $t"
        fi
    done
    echo
    echo "Launch the application to regenerate pristine state."
    echo "Undo with: $0 --restore $APP"
    ;;

restore)
    restored=0
    for t in "${TARGETS[@]}"; do
        newest=$(ls -1dt "${t}".bak-* 2>/dev/null | head -1)
        if [[ -n "${newest}" ]]; then
            [[ -e "$t" ]] && mv -v "$t" "${t}.discarded-${STAMP}"
            mv -v "$newest" "$t"
            restored=1
        fi
    done
    [[ $restored -eq 0 ]] && echo "  no .bak-* backups found for $APP"
    ;;

purge)
    echo "  The following will be DELETED PERMANENTLY:"
    found=0
    for t in "${TARGETS[@]}"; do
        if [[ -e "$t" ]]; then echo "    $t"; found=1; fi
    done
    if [[ $found -eq 0 ]]; then
        echo "    (nothing present)"
        exit 0
    fi
    echo
    read -r -p "  Type DELETE to confirm: " answer
    if [[ "$answer" != "DELETE" ]]; then
        echo "  Aborted. Nothing was changed."
        exit 1
    fi
    banner "PURGED (permanent)"
    for t in "${TARGETS[@]}"; do
        [[ -e "$t" ]] && rm -rfv "$t"
    done
    ;;
esac
