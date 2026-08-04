#!/bin/bash
# sweep.sh — generate the measurement corpus from the test bed's CLI mode.
#
# Produces one plain-text dump per configuration, so a stock build and a patched
# build can be compared with diff(1) rather than by eye. Screenshots do not diff;
# text does.
#
# Usage:
#   ./sweep.sh [note]        optional free-text note recorded in SUMMARY.txt,
#                            e.g. ./sweep.sh "stock"  /  ./sweep.sh "patch3"
#
# Output layout:
#   corpus/<installed plasma-breeze package>/<MMDDYYYY>-<NN>/*.txt
#
# e.g. corpus/plasma-breeze-6.7.3-2.fc43.1.x86_64/07312026-01/
#
# The Breeze package is the top level because it is the variable under test — a
# measurement is only meaningful with respect to the build that produced it. The
# dated, two-digit sequence keeps repeated runs on one day from colliding and
# keeps each run's files together, so two runs diff directory-against-directory:
#
#   diff -ru corpus/plasma-breeze-6.7.3-2.fc43.1.x86_64/07312026-01\
#            corpus/plasma-breeze-6.7.3-2.fc43.2.x86_64/08012026-01
#
# The style/theme environment is cleared the same way probe/run-clean.sh does it,
# so a scripted sweep and a hand-driven GUI run see the same configuration.

set -u

NOTE="${1:-}"

cd "$(dirname "$0")" || exit 1

# Refuse to run twice at once. Two concurrent sweeps write into the same output
# directory and append to the same SUMMARY, which interleaves their lines and
# lets the summariser read a dump while it is still being written — producing
# plausible-looking rows that are silently wrong.
LOCK=".sweep.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "error: a sweep is already running (lock: $LOCK)" >&2
    echo "       if that is stale, remove it: rmdir $LOCK" >&2
    exit 1
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT INT TERM

PKG="$(rpm -q plasma-breeze 2>/dev/null | head -1)"
[[ -n "$PKG" ]] || PKG="unknown-breeze-version"

DATE="$(date +%m%d%Y)"
n=1
while [[ -d "corpus/$PKG/$(printf '%s-%02d' "$DATE" "$n")" ]]; do
    n=$((n + 1))
done
OUT="corpus/$PKG/$(printf '%s-%02d' "$DATE" "$n")"
mkdir -p "$OUT"

# Same variables probe/run-clean.sh clears, for the same reason: the style must
# come from the plugin under test, not from an environment override.
for v in QT_STYLE_OVERRIDE QT_QPA_STYLE QT_QUICK_CONTROLS_STYLE \
         QT_QUICK_CONTROLS_FALLBACK_STYLE KDESTYLE KDEDIRS QT_FONT_DPI \
         QT_SCALE_FACTOR QT_SCREEN_SCALE_FACTORS QT_AUTO_SCREEN_SCALE_FACTOR \
         QT_ENABLE_HIGHDPI_SCALING GTK2_RC_FILES GTK_THEME; do
    unset "$v"
done

# --- the matrix ------------------------------------------------------------
# Widths span both regimes and the real application geometries:
#   96  KeePassXC Edit Entry nav
#   150 SMPlayer sections minimum (preferencesdialog.ui:47-51)
#   others bracket the point where slack crosses zero
WIDTHS=(96 120 150 200 220 260 300 400 600)
STYLES=(Breeze Fusion Windows kvantum qt5ct-style)
STRINGS=(smplayer keepassxc graded)
ICONS=(on off)
ELIDES=(right none)
# Added 2026-08-01. A patch that behaved correctly across all 972 ListMode
# configurations still elided labels in KeePassXC's entry-editor sidebar, which
# is a QListView in IconMode with wordWrap enabled. Neither property was in the
# matrix, so no measurement could have caught it. IconMode puts the decoration
# above the text, a different branch of viewItemLayout(); wordWrap sets
# QStyleOptionViewItem::WrapText, which the style sees on every item.
VIEWMODES=(list icon)
WRAPS=(off on)

SUMMARY="$OUT/SUMMARY.txt"
: > "$SUMMARY"

{
    echo "run         : $OUT"
    [[ -n "$NOTE" ]] && echo "note        : $NOTE"
    echo "when        : $(date --iso-8601=seconds)"
    echo "breeze rpm  : $PKG"
    echo "breeze qt5  : $(rpm -q plasma-breeze-qt5 2>/dev/null || echo '(not installed)')"
    echo "breeze qt6  : $(rpm -q plasma-breeze-qt6 2>/dev/null || echo '(not installed)')"
    echo
    echo "One line per configuration. 'elide n/m' = n of m rows are truncated."
    echo "'worst slack' = the most negative (drawn - adv) across the rows; negative"
    echo "means at least one label cannot fit the rect the style gave it."
    echo "Column definitions are repeated in full in every dump file."
    echo
} >> "$SUMMARY"

count=0
for qt in 5 6; do
    BIN="./itemview-testbed-qt${qt}"
    [[ -x "$BIN" ]] || { echo "skip: $BIN not built" >&2; continue; }

    # Only query styles the binary can actually load for this Qt major.
    avail="$($BIN --list-styles 2>/dev/null)"

    for style in "${STYLES[@]}"; do
        grep -qix "$style" <<< "$avail" || continue
        for strings in "${STRINGS[@]}"; do
            for icons in "${ICONS[@]}"; do
                for elide in "${ELIDES[@]}"; do
                    for vm in "${VIEWMODES[@]}"; do
                    for wr in "${WRAPS[@]}"; do
                    for w in "${WIDTHS[@]}"; do
                        f="$OUT/qt${qt}-${style}-${strings}-icons${icons}-${elide}-${vm}-wrap${wr}-${w}px.txt"
                        "$BIN" --dump --style "$style" --width "$w" \
                               --strings "$strings" --icons "$icons" \
                               --elide "$elide" --viewmode "$vm" --wrap "$wr" > "$f" 2>/dev/null || continue
                        count=$((count + 1))

                        # One summary line: how many rows elide, and the worst slack.
                        elided=$(awk 'NF>10 && $NF=="YES"' "$f" | wc -l)
                        rows=$(awk 'NF>10 && ($NF=="YES"||$NF=="no")' "$f" | wc -l)
                        worst=$(awk 'NF>10 && ($NF=="YES"||$NF=="no") {print $(NF-1)}' "$f" \
                                | sort -n | head -1)

                        # A dump that parses to zero rows is a failure, not a
                        # result. Never let it summarise as a tidy "0/0" — that
                        # reads as "nothing elided" when it means "nothing was
                        # measured".
                        if [[ "$rows" -eq 0 ]]; then
                            printf 'qt%s %-12s %-10s icons%-3s %-5s %-4s wrap%-3s %4spx  *** NO ROWS PARSED (%s)\n' \
                                   "$qt" "$style" "$strings" "$icons" "$elide" "$vm" "$wr" "$w" \
                                   "$(basename "$f")" >> "$SUMMARY"
                            continue
                        fi

                        printf 'qt%s %-12s %-10s icons%-3s %-5s %-4s wrap%-3s %4spx  elide %2s/%-2s  worst slack %s\n' \
                               "$qt" "$style" "$strings" "$icons" "$elide" "$vm" "$wr" "$w" \
                               "$elided" "$rows" "${worst:-n/a}" >> "$SUMMARY"
                    done
                    done
                    done
                done
            done
        done
    done
done

echo "wrote $count dumps to $OUT"
echo "summary: $SUMMARY"
