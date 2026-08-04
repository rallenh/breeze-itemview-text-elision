# Test bed

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

An interactive Qt application that measures `SE_ItemViewItemText` in a **live
item view**, plus scripts to sweep it across a matrix of configurations and
compare one Breeze build against another.

## How this differs from `probe/`

They answer different questions and neither replaces the other.

| | measures | cannot show |
|---|---|---|
| [`probe/`](../probe/) | `subElementRect()` on a hand-built `QStyleOptionViewItem`, with **no application in the path** — so a difference between styles cannot be attributed to application code | what a running application actually receives |
| `testbed/` | the option **the view itself builds** (`viewOptions()` on Qt5, `initViewItemOption()` on Qt6), so fields an application never sets by hand — `showDecorationSelected` above all — arrive from the style hint as they would in a real program | anything about a view it does not model |

The list under test is constructed to match SMPlayer's Preferences sidebar, line
by line, with every construction call cited to `preferencesdialog.{ui,cpp}` in
the source. It installs **no item delegate**, because SMPlayer installs none.

Measurements are reported **per row, not per selection**: on Qt 5.15 the clamp in
`viewItemLayout()` sizes each row to its own natural width before the inset is
taken, so the effect is a property of the row set. The numbers for any single row
are identical between Qt5 and Qt6 while the rendered lists differ completely.

## Building and running

```bash
make                    # itemview-testbed-qt5 and itemview-testbed-qt6
../probe/run-clean.sh "$PWD/itemview-testbed-qt5"      # GUI, clean environment
./itemview-testbed-qt5 --help                          # scripted use
```

In the GUI, the controls change the live view immediately — that is the point.
The rendered list is the ground truth and the table beside it explains what
produced it. Where the two disagree, the list is right.

In `--dump` mode the widget is laid out but never mapped to the screen, so a
scripted sweep produces identical geometry with no windows appearing.

## Scripts

| | |
|---|---|
| `sweep.sh [note]` | Runs the full matrix — Qt major × style × string set × icons × elide mode × view mode × word wrap × list width — and writes one dump per configuration, plus a `SUMMARY.txt` digest. |
| `compare.sh <A> <B> [--full]` | Diffs two runs. Not for discovering the effect; it answers *what changed besides the thing the patch was meant to change*, across thousands of configurations at once. Warns if a style the change cannot reach appears to have changed. |
| `compare.sh --fusion-check <run>` | Answers, within a single run, whether Breeze elides any label that Fusion does not. Fusion applies no inset, so it is the reference for the best a change to `SE_ItemViewItemText` could achieve — and this needs no comparison build installed. |
| `pack-results.sh` | Collapses a run's per-configuration dumps into one archive. See below. |

The matrix and the reasoning behind each axis are documented in
[`../docs/test-plan.md`](../docs/test-plan.md).

## Output layout

```
corpus/<installed plasma-breeze package>/<MMDDYYYY>-<NN>/
    SUMMARY.txt      one line per configuration
    dumps.tar.gz     every per-configuration dump
```

The Breeze package is the top level because it is the variable under test: a
measurement is meaningful only with respect to the build that produced it. The
dated, two-digit sequence keeps repeat runs on one day from colliding and keeps a
run's files together, so two runs compare directory against directory.

Every dump repeats the full column glossary, so a single file read out of context
is still self-describing.

## Packing

A sweep writes thousands of files. `pack-results.sh` archives them and leaves
`SUMMARY.txt` uncompressed, because that is the file a reader opens and the only
one worth diffing between runs.

```bash
./pack-results.sh              # pack every unpacked run
./pack-results.sh --list       # show what is packed
./pack-results.sh --unpack <run-dir>
```

It uses `tar` + `gzip` rather than `zip`. The dumps are near-identical to one
another, so almost all the available compression comes from redundancy *between*
files, which `zip` cannot see because it compresses each member separately.
Measured on the 3888-file run: `zip -9` 6.1M, `tar.gz` 315K, `tar.zst` 88K. Set
`ZSTD=1` for the last of those; gzip is the default because it extracts anywhere
without extra tooling and the difference is immaterial at this size.

`compare.sh` reads directories of `.txt` files, so unpack a run before comparing
it.

## A note on trusting these numbers

During development this harness produced several results that were wrong in
plausible ways rather than obvious ones — a measurement that stopped one step
short of what Qt actually draws, a cached geometry reported as current, a failure
that summarised as a tidy zero, and a matrix that covered everything it measured
while missing a view configuration a real application uses. Each was caught by
comparing one source of truth against another, not by an error message.

The checks that exist because of them are listed in
[`../docs/test-plan.md`](../docs/test-plan.md) §8. The short version: treat
agreement between the table and the rendered list as what makes a result
believable, and treat the [screenshots](../screenshots/) as evidence rather than
illustration — on one occasion they were the only thing that contradicted the
measurements, and they were right.
