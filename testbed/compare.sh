#!/bin/bash
# compare.sh — diff two test-bed sweep runs.
#
# Usage:
#   ./compare.sh <run-A> <run-B> [--full]
#
#   ./compare.sh corpus/plasma-breeze-6.7.3-2.fc43.1.x86_64/07312026-01 \
#                corpus/plasma-breeze-6.7.3-1.fc43.2.x86_64/08012026-01
#
#   --full   also print the per-file diffs, not just which files differ
#
# Purpose
# -------
# Not for discovering the effect — the sweep already shows that. This is the
# regression net for a proposed patch: across ~970 configurations, it answers
# "what changed *besides* the thing the patch was meant to change?"
#
# For a width-guard patch the acceptance criteria are statements about this
# diff:
#   - rows with positive slack must be identical to stock (otherwise it is a
#     removal, not a guard)
#   - rows with negative slack may change by at most the inset, never more
#   - decoX/decoW must not move at all; the patch touches the text branch, not
#     its sibling
#   - runs for other styles (Fusion, Windows, Kvantum) must not change at all,
#     since a Breeze patch cannot reach them
#
# Filename compatibility
# ----------------------
# Dumps produced before 2026-08-01 have no view-mode or word-wrap component in
# their names, because the harness had no such axes: every run was implicitly
# ListMode with wrapping off. Those runs are still valid and are not re-made.
# Names are normalised by removing "-list-wrapoff" before matching, so an older
# run compares against the corresponding subset of a newer one. Configurations
# that exist only in the newer run (icon mode, or wrapping on) are reported
# separately as new coverage rather than as differences.
#
# Fusion reference check
# ----------------------
#   ./compare.sh --fusion-check <run>
#
# Answers, within a single run and with no baseline build required: is there any
# configuration in which Breeze elides a label that Fusion does not? Fusion
# applies no inset, so it is the reference for "as good as removing the inset
# entirely". This is the property a width-guard patch has to hold, and checking
# it needs one sweep rather than a reboot to install a comparison build.
#
# Two header lines are filtered out because they are expected to differ between
# any two runs:
#
#   when        : the timestamp
#   breeze rpm  : the package under test — the whole point of comparing
#
# Nothing else is filtered. In particular `item rect`, `tMargin` and
# `showDecoSel` are kept, because those are measurements: a change in frame
# metrics would show up in `item rect`, and dropping a fixed number of header
# lines would hide it.

set -u

cd "$(dirname "$0")" || exit 1

# --- Fusion reference check --------------------------------------------------
if [[ "${1:-}" == "--fusion-check" ]]; then
    RUN="${2:-}"
    [[ -d "$RUN" ]] || { echo "usage: $0 --fusion-check <run-dir>" >&2; exit 1; }

    echo "Fusion reference check: $RUN"
    grep -h '^breeze rpm' "$RUN"/*.txt 2>/dev/null | head -1
    echo
    echo "Any configuration where Breeze elides a label that Fusion does not is a"
    echo "failure: Fusion applies no inset, so it is the reference for the best a"
    echo "change to SE_ItemViewItemText could achieve."
    echo

    fails=0
    checked=0
    for bf in "$RUN"/*-Breeze-*.txt; do
        [[ -e "$bf" ]] || continue
        ff="${bf//-Breeze-/-Fusion-}"
        [[ -f "$ff" ]] || continue
        checked=$((checked + 1))
        be=$(awk 'NF>10 && $NF=="YES"' "$bf" | wc -l)
        fe=$(awk 'NF>10 && $NF=="YES"' "$ff" | wc -l)
        if [[ "$be" -gt "$fe" ]]; then
            fails=$((fails + 1))
            printf '  FAIL  %-62s breeze %s elided, fusion %s\n' "$(basename "$bf")" "$be" "$fe"
        fi
    done

    echo
    echo "configurations checked : $checked"
    echo "failures               : $fails"
    [[ "$fails" -eq 0 ]] && echo "PASS — Breeze never elides where Fusion does not."
    exit $(( fails > 0 ))
fi

A="${1:-}"
B="${2:-}"
FULL="${3:-}"

if [[ -z "$A" || -z "$B" ]]; then
    echo "usage: $0 <run-A> <run-B> [--full]" >&2
    exit 1
fi
for d in "$A" "$B"; do
    [[ -d "$d" ]] || { echo "error: not a directory: $d" >&2; exit 1; }
done

# A packed run holds only SUMMARY.txt and an archive. Comparing against one
# silently yields "0 identical, 0 differing, N only in A", which reads like a
# result rather than the mistake it is. Refuse instead.
for d in "$A" "$B"; do
    n=$(find "$d" -maxdepth 1 -name '*.txt' ! -name SUMMARY.txt | wc -l)
    if [[ "$n" -eq 0 ]]; then
        echo "error: $d holds no dumps — it looks packed." >&2
        echo "       ./pack-results.sh --unpack $d" >&2
        exit 1
    fi
done

# Older runs have no view-mode/wrap component; they are implicitly list+wrapoff.
canon() { sed 's/-list-wrapoff-/-/' <<< "$1"; }

# The only lines expected to differ between any two runs.
VOLATILE='^(when|breeze rpm) '

# Dumps from before 2026-08-01 have no "view mode" or "word wrap" header lines,
# because the harness had no such axes. Comparing across that boundary, those
# two lines are present on one side and absent on the other, which would report
# every file as differing — a format change masquerading as 972 behavioural
# changes. When the formats differ, drop them from both sides; the filename
# mapping already records that the older run is list + wrapoff.
fmt_of() { grep -lE '^view mode ' "$1"/*.txt >/dev/null 2>&1 && echo new || echo old; }
if [[ "$(fmt_of "$A")" != "$(fmt_of "$B")" ]]; then
    VOLATILE='^(when|breeze rpm|view mode|word wrap) '
    echo "note: runs use different dump formats (one predates the view-mode/wrap"
    echo "      axes). Those two header lines are excluded from this comparison."
    echo
fi

strip() { grep -vE "$VOLATILE" "$1"; }

echo "A: $A"
echo "   $(grep -h '^breeze rpm' "$A/SUMMARY.txt" 2>/dev/null | head -1)"
echo "B: $B"
echo "   $(grep -h '^breeze rpm' "$B/SUMMARY.txt" 2>/dev/null | head -1)"
echo

same=0
diffs=0
only_a=0
new_cov=0
declare -a differing=()
declare -A MAP_A=()
declare -A MAP_B=()

# Index both runs by canonical name so a pre-2026-08-01 run (no view-mode/wrap
# component) matches the list+wrapoff subset of a newer one.
while IFS= read -r path; do
    f="$(basename "$path")"
    [[ "$f" == "SUMMARY.txt" ]] && continue
    MAP_A["$(canon "$f")"]="$f"
done < <(find "$A" -maxdepth 1 -name '*.txt' | sort)

while IFS= read -r path; do
    f="$(basename "$path")"
    [[ "$f" == "SUMMARY.txt" ]] && continue
    MAP_B["$(canon "$f")"]="$f"
done < <(find "$B" -maxdepth 1 -name '*.txt' | sort)

for key in "${!MAP_A[@]}"; do
    fa="${MAP_A[$key]}"
    fb="${MAP_B[$key]:-}"
    if [[ -z "$fb" ]]; then
        only_a=$((only_a + 1))
        continue
    fi
    if diff -q <(strip "$A/$fa") <(strip "$B/$fb") >/dev/null 2>&1; then
        same=$((same + 1))
    else
        diffs=$((diffs + 1))
        differing+=("$fb")
    fi
done

# Present only in B: either a style the other build lacks, or — far more often —
# a configuration on an axis the older run predates. Counted as new coverage,
# not as a difference, since there is nothing to compare it against.
for key in "${!MAP_B[@]}"; do
    [[ -n "${MAP_A[$key]:-}" ]] || new_cov=$((new_cov + 1))
done

echo "identical      : $same"
echo "differing      : $diffs"
[[ $only_a -gt 0 ]]  && echo "only in A      : $only_a"
[[ $new_cov -gt 0 ]] && echo "new coverage   : $new_cov  (configurations absent from A, nothing to compare)"
echo

if [[ $diffs -gt 0 ]]; then
    echo "--- differing configurations ---"
    printf '%s\n' "${differing[@]}"
    echo

    # A patch to a Breeze code path must not change any other style's numbers.
    # Surface that explicitly rather than leaving it to be spotted in the list.
    foreign=$(printf '%s\n' "${differing[@]}" | grep -vi 'breeze' | wc -l)
    if [[ "$foreign" -gt 0 ]]; then
        echo "*** WARNING: $foreign differing file(s) are for styles other than Breeze."
        echo "    A change confined to Breeze cannot alter those. Investigate before"
        echo "    trusting this comparison:"
        printf '%s\n' "${differing[@]}" | grep -vi 'breeze' | sed 's/^/      /'
        echo
    fi
fi

echo "--- SUMMARY.txt ---"
if [[ -f "$A/SUMMARY.txt" && -f "$B/SUMMARY.txt" ]]; then
    diff <(strip "$A/SUMMARY.txt") <(strip "$B/SUMMARY.txt") \
        && echo "(summaries identical)"
else
    echo "(one or both SUMMARY.txt missing)"
fi

if [[ "$FULL" == "--full" && $diffs -gt 0 ]]; then
    echo
    echo "--- per-file diffs ---"
    for f in "${differing[@]}"; do
        echo
        echo "=== $f ==="
        diff <(strip "$A/$f") <(strip "$B/$f")
    done
fi
