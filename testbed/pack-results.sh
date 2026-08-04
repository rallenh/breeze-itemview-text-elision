#!/bin/bash
# pack-results.sh — collapse a sweep run's per-configuration dumps into one
# archive, leaving the summary readable.
#
# Usage:
#   ./pack-results.sh                 pack every unpacked run under corpus/
#   ./pack-results.sh <run-dir> ...   pack specific run directories
#   ./pack-results.sh --unpack <dir>  restore the dumps in place
#   ./pack-results.sh --list          show what is packed and what is not
#
# Why
# ---
# A sweep writes one dump per configuration: 973 files for the original matrix,
# 3889 for the current one. That is unremarkable on disk but makes `git status`
# unreadable and buries the files anyone actually wants to look at.
#
# After packing, each run directory holds two things:
#
#   SUMMARY.txt     one line per configuration — left uncompressed, because it
#                   is the file a reader opens, and the only one worth diffing
#                   between runs
#   dumps.tar.gz    every per-configuration dump
#
# Choice of format
# ----------------
# tar+gzip, not zip. The dumps are near-identical to each other, so nearly all
# the compression available comes from redundancy *between* files. zip
# compresses each member separately and cannot see it. Measured on the 3888-file
# run:
#
#   zip -9     6.1M
#   tar.gz     315K
#   tar.zst     88K
#
# tar.zst is smaller again, but gzip is extractable everywhere without extra
# tooling, and at these sizes the difference does not matter. Pass ZSTD=1 to use
# zstd instead if you would rather have the smaller file.
#
# Note on the comparison tools
# ----------------------------
# compare.sh and `compare.sh --fusion-check` read directories of .txt files. Run
# `./pack-results.sh --unpack <dir>` before comparing a packed run, or keep the
# extracted copies locally and let .gitignore keep them out of the repository.

set -u

cd "$(dirname "$0")" || exit 1

ZSTD="${ZSTD:-0}"
if [[ "$ZSTD" == "1" ]]; then
    ARCHIVE="dumps.tar.zst"
    COMPRESS=(zstd -19 -q -T0)
    DECOMPRESS=(zstd -dq)
else
    ARCHIVE="dumps.tar.gz"
    COMPRESS=(gzip -9)
    DECOMPRESS=(gzip -d)
fi

# A run directory is one holding SUMMARY.txt.
find_runs() { find corpus -mindepth 2 -maxdepth 3 -name SUMMARY.txt -printf '%h\n' 2>/dev/null | sort; }

human() { numfmt --to=iec "$1" 2>/dev/null || echo "$1"; }

pack_one() {
    local d="$1"
    [[ -d "$d" ]] || { echo "not a directory: $d" >&2; return 1; }
    [[ -f "$d/SUMMARY.txt" ]] || { echo "no SUMMARY.txt, not a run directory: $d" >&2; return 1; }

    local n
    n=$(find "$d" -maxdepth 1 -name '*.txt' ! -name SUMMARY.txt | wc -l)
    if [[ "$n" -eq 0 ]]; then
        echo "already packed, nothing to do: $d"
        return 0
    fi
    if [[ -e "$d/$ARCHIVE" ]]; then
        echo "refusing to overwrite existing $ARCHIVE in $d" >&2
        return 1
    fi

    local before after
    before=$(du -sb "$d" | cut -f1)

    # Archive from inside the directory so paths are relative, then verify the
    # archive lists the expected number of members before deleting anything.
    ( cd "$d" && tar cf - --exclude=SUMMARY.txt -- *.txt ) | "${COMPRESS[@]}" > "$d/$ARCHIVE" || {
        echo "archive failed, leaving $d untouched" >&2
        rm -f "$d/$ARCHIVE"
        return 1
    }

    local members
    members=$("${DECOMPRESS[@]}" -c "$d/$ARCHIVE" | tar tf - | grep -c '\.txt$')
    if [[ "$members" -ne "$n" ]]; then
        echo "archive holds $members of $n dumps — leaving $d untouched" >&2
        rm -f "$d/$ARCHIVE"
        return 1
    fi

    find "$d" -maxdepth 1 -name '*.txt' ! -name SUMMARY.txt -delete
    after=$(du -sb "$d" | cut -f1)
    printf '%-58s %5s dumps  %8s -> %8s\n' "$d" "$n" "$(human "$before")" "$(human "$after")"
}

unpack_one() {
    local d="$1"
    local a=""
    [[ -f "$d/dumps.tar.gz"  ]] && a="$d/dumps.tar.gz"
    [[ -f "$d/dumps.tar.zst" ]] && a="$d/dumps.tar.zst"
    [[ -n "$a" ]] || { echo "no archive in $d" >&2; return 1; }
    case "$a" in
        *.zst) zstd -dq -c "$a" | tar xf - -C "$d" ;;
        *)     gzip -dc "$a" | tar xf - -C "$d" ;;
    esac
    echo "unpacked $(find "$d" -maxdepth 1 -name '*.txt' ! -name SUMMARY.txt | wc -l) dumps into $d"
}

case "${1:-}" in
    --list)
        printf '%-58s %s\n' "run" "state"
        while IFS= read -r d; do
            n=$(find "$d" -maxdepth 1 -name '*.txt' ! -name SUMMARY.txt | wc -l)
            if [[ "$n" -gt 0 ]]; then printf '%-58s %s\n' "$d" "unpacked ($n dumps)"
            else printf '%-58s %s\n' "$d" "packed"; fi
        done < <(find_runs)
        ;;
    --unpack)
        shift
        [[ $# -gt 0 ]] || { echo "usage: $0 --unpack <run-dir> ..." >&2; exit 1; }
        for d in "$@"; do unpack_one "$d"; done
        ;;
    "")
        while IFS= read -r d; do pack_one "$d"; done < <(find_runs)
        ;;
    -*)
        echo "usage: $0 [--list | --unpack <dir> ... | <run-dir> ...]" >&2
        exit 1
        ;;
    *)
        for d in "$@"; do pack_one "$d"; done
        ;;
esac
