# Test plan: measuring `SE_ItemViewItemText` across styles, widths and Qt versions

This document describes how the measurements in this repository are produced, so
that any reader can reproduce them. It covers what is measured, with which
instrument, under which conditions, and what each result is intended to
establish.

Nothing here modifies a system. Every procedure is read-only: no packages are
installed, no desktop settings are changed, and the style under test is loaded
into a test process rather than applied to the session.

---

## 1. Two instruments, two different questions

The repository contains two kinds of tool, and they answer different questions.
Neither replaces the other.

### `probe/` — does the narrowing exist at all?

The probes construct a `QStyleOptionViewItem` by hand and call
`subElementRect(SE_ItemViewItemText)` on a real, `QStyleFactory`-loaded style
plugin. There is deliberately **no application** in the path: no window, no item
view, no delegate. If the returned rectangle is narrower under one style than
another for the same input, that difference cannot be attributed to application
code, because there is none.

That is the probes' purpose and their limit. Because the option is assembled by
hand, a probe cannot show what a *running application* receives.

### `testbed/` — does it produce visible elision in a real view?

The test bed builds a live `QListWidget` configured to match SMPlayer's
Preferences sidebar, and takes its measurements from the option **the view
itself** constructs — `QAbstractItemView::viewOptions()` on Qt5,
`initViewItemOption()` on Qt6. Fields that an application never sets by hand,
notably `showDecorationSelected`, therefore arrive from the style hint exactly as
they would in a real program.

SMPlayer's sidebar is the reference case because it installs **no custom item
delegate**: its items are rendered by Qt's stock `QStyledItemDelegate`. The test
bed installs none either. Provenance for every construction line is recorded in
`testbed/itemview-testbed.cpp`, citing `smplayer-25.6.0/src/preferencesdialog.{ui,cpp}`.

---

## 2. What is measured

Both instruments report the same core quantities. Every dump file produced by the
test bed repeats these definitions in full, so a result file is self-contained.

| term | definition |
|---|---|
| `adv` | `fontMetrics.horizontalAdvance(text)` — width of the glyphs themselves. Independent of style. |
| `tMargin` | `pixelMetric(PM_FocusFrameHMargin) + 1` — the per-side padding `QCommonStylePrivate::viewItemLayout()` reserves around item text. |
| `natural` | `adv + 2*tMargin` — the width `QCommonStyle` allocates to a text-only item before any style-specific adjustment. |
| `textX`, `textW` | position and width of `subElementRect(SE_ItemViewItemText)`: the rectangle the style hands to the painter. |
| `L`, `R` | `textX - itemRect.left` and `itemRect.right - textRect.right` — the inset itself. |
| `decoX`, `decoW` | `subElementRect(SE_ItemViewItemDecoration)`, reported so that a change to the text branch can be shown not to disturb its sibling. |
| `drawn` | `textW - 2*tMargin`. `viewItemDrawText()` removes `tMargin` from each side of whatever `subElementRect()` returned, immediately before eliding, so this — not `textW` — is the width the string is laid out in. |
| `slack` | `drawn - adv`. Negative means the string cannot fit the rectangle it was given. |
| `elides` | whether `QFontMetrics::elidedText()` shortens the string at that width, i.e. whether a user sees an ellipsis. |

Two qualifications apply to the results and are stated in every dump:

- `drawn`, `slack` and `elides` assume the style allows `QCommonStyle` to paint
  the item. A style that overrides `CE_ItemViewItem` and draws item text itself
  never reaches `viewItemDrawText()`, so those three columns do not describe it.
  Kvantum is such a style. Breeze contains no reference to `CE_ItemViewItem`
  anywhere in `kstyle/`, so they do describe Breeze.
- With icons enabled, `L` also contains the decoration, since it measures from
  the item's left edge. Read the inset from an `--icons off` run. When comparing
  two builds of the same style the decoration contribution cancels.

---

## 3. Measurements are per row, not per selection

The effect is a property of the **set of rows**, not of whichever item is
selected. On Qt 5.15, `viewItemLayout()` sizes each row to its own natural width
before the style's inset is applied, so every row is narrowed independently. On
Qt 6 that clamp was removed and each row receives the full remaining width.

The consequence is that a single-row readout cannot distinguish the two: the
numbers for any one row are identical between Qt5 and Qt6, while the rendered
lists differ completely. Every table therefore reports **one line per row**.

---

## 4. Reproducing the measurements

### Build

```
cd probe    && make v3 v4      # command-line probes, Qt5 and Qt6
cd testbed  && make            # test bed, Qt5 and Qt6
```

### Environment

Both are launched through `probe/run-clean.sh`, which clears every environment
variable that could override the style, theme, fonts or scaling
(`QT_STYLE_OVERRIDE`, `QT_SCALE_FACTOR`, `QT_FONT_DPI`, `GTK_THEME`, and others),
while preserving the session context that routes style lookups normally. It also
prints the installed `plasma-breeze` package versions, so a captured terminal
records which build produced the numbers.

```
probe/run-clean.sh "$PWD/qt5-style-elide-test-v3"
testbed/../probe/run-clean.sh "$PWD/itemview-testbed-qt5"
```

### Interactive use

Run the test bed with no arguments for the GUI. The controls — style, string set,
list width, icons, elide mode — change the live view immediately, which is the
point: the rendered list is the ground truth, and the table beside it explains
what produced it.

### Scripted use

```
./itemview-testbed-qt5 --dump --style Breeze --width 400 --icons off
./itemview-testbed-qt5 --list-styles
./itemview-testbed-qt5 --help
```

In `--dump` mode the widget is laid out but never mapped to the screen
(`Qt::WA_DontShowOnScreen`), so a scripted sweep produces the same geometry as
the GUI without windows appearing.

### Full sweep

```
./sweep.sh "stock baseline"
```

writes one dump per configuration to

```
corpus/<installed plasma-breeze package>/<MMDDYYYY>-<NN>/
```

The Breeze package is the top level because it is the variable under test: a
measurement is meaningful only with respect to the build that produced it. The
dated, sequenced subdirectory keeps repeated runs from colliding and keeps each
run's files together, so two runs compare directory against directory:

```
diff -ru corpus/plasma-breeze-6.7.3-2.fc43.1.x86_64/07312026-01 \
         corpus/plasma-breeze-6.7.3-2.fc43.2.x86_64/08012026-01
```

This is the reason the test bed emits plain text as well as a display: two builds
are compared by diffing measurements, not by placing screenshots side by side.

---

## 5. The matrix

| axis | values | reason |
|---|---|---|
| Qt major | 5, 6 | `viewItemLayout()` differs between them |
| style | Breeze, Fusion, Windows, Kvantum, qt5ct-style | Fusion and Windows are controls; Kvantum is a third-party engine that paints its own item text; qt5ct-style is a `QProxyStyle` over Fusion |
| list width | 96, 120, 150, 200, 220, 260, 300, 400, 600 | 96 and 150 are real application geometries (see below); the remainder bracket the width at which `slack` crosses zero |
| strings | SMPlayer sections, KeePassXC Edit Entry nav, graded lengths | length is the other variable that decides whether a label fits |
| icons | on, off | with icons off, `L` reports the style's inset directly |
| elide mode | ElideRight, ElideNone | `ElideNone` clips instead of eliding, a visibly different failure |
| view mode | ListMode, IconMode | `IconMode` places the decoration above the text, a different branch of `viewItemLayout()` |
| word wrap | off, on | sets `QStyleOptionViewItem::WrapText`, which the style sees on every item |

The last two axes were added on 2026-08-01, after a change that behaved correctly
across all 972 configurations of the earlier matrix was found to elide labels in
KeePassXC's entry-editor sidebar — a `QListView` in `IconMode` with `wordWrap`
enabled. Neither property was represented, so no measurement in that matrix could
have detected it; a screenshot did. Adding them brought the matrix to 3888
configurations and added two header lines (`view mode`, `word wrap`) to every dump.

Results produced before that date have neither the new axes nor those header
lines. They remain valid — they are the ListMode, no-wrap subset — and have not
been regenerated, which would have required reinstalling each earlier build.
`compare.sh` detects the format difference and maps between the two.

Two widths correspond to shipping applications: **150px** is the minimum width of
SMPlayer's `sections` list (`preferencesdialog.ui:47-51`), and **96px** is the
width of KeePassXC's Edit Entry navigation list.

---

## 6. Visual captures

Some results are numerical and belong in the corpus. Others are visual, and a
rendered screenshot is the evidence — a reader should be able to see the
truncation rather than infer it from a column.

Captures are made with the launching terminal in frame, because
`run-clean.sh` prints the installed package versions there; that is what makes a
screenshot attributable to a build.

| capture | configuration | what it shows |
|---|---|---|
| C1 | Qt5, Breeze, 400px, SMPlayer strings | every label truncated in a list far wider than the text |
| C2 | Qt5, Fusion, 400px, identical otherwise | the same labels rendered in full |
| C3 | Qt6, Breeze, 400px | no truncation — the Qt5/Qt6 difference, with the style held constant |
| C4 | Qt6, Breeze, 150px | truncation returns once the view is genuinely constrained |
| C5 | Qt5, Kvantum, 400px | a style that narrows the rectangle *more* than Breeze and shows no truncation, because it paints its own text |

C1 and C2 differ only in the value of the Style dropdown, within one process, on
one screen, with the same strings and the same width. That is the single-variable
comparison the whole method rests on.

---

## 7. Interpreting a result

A measurement establishes that Breeze *causes* a label to be truncated only when
that label would otherwise have fitted. Three regimes must be distinguished, and
conflating them overstates the finding:

1. **Ample width.** `slack` is comfortably positive under every style; the inset
   costs nothing visible.
2. **The boundary.** `slack` is positive without the inset and negative with it.
   Here, and only here, the style is the cause.
3. **Insufficient width.** The label cannot fit under any style. Breeze is worse
   by a fixed amount, but it is not the cause of the truncation.

On Qt6, regime 2 is a narrow range of widths. On Qt 5.15 the clamp in
`viewItemLayout()` sets every row's rectangle to its own natural width, which
places every row at the boundary regardless of how wide the view is — which is
why the Qt5 tables show every label truncated at every width tested.

---

## 7.1 Measured results

Every build below was measured with the same harness at the same commit. The
`plasma-breeze` package under test is recorded in each run's `SUMMARY.txt` and in
the header of every dump.

The **Fusion reference check** asks, of each configuration, whether Breeze elides
a label that Fusion does not. Fusion applies no inset, so it is the reference for
the best a change to `SE_ItemViewItemText` could achieve.

| build | patch | matrix | Fusion check |
|---|---|---|---|
| `6.7.3-1.fc43.1` | none — **stock** | 3888 configurations | **218 failures** of 864 |
| `6.7.3-2.fc43.5` | 0004 — width guard, no exclusions | 3888 configurations | **0 failures** of 864 |

The check therefore discriminates in both directions on the full matrix: it
fails on the unmodified build and passes on the patched one. It was separately
verified to fail against build `6.7.3-2.fc43.4` (patch 0003), whose defect was
confined to configurations the matrix did not then contain.

Comparing the two runs directly, with the dumps for every other style included:

```
identical            : 3210
differing            :  678
non-Breeze differing :    0
by elide mode        :  339 ElideRight + 339 ElideNone
by view mode         :  396 IconMode  + 282 ListMode
```

Of 864 Breeze configurations, 678 change and **186 are byte-identical to stock** —
those being the ones where the item has width to spare, and where the inset is
consequently retained. No configuration for Fusion, Windows, Kvantum or
qt5ct-style differs, which is the expected result for a change confined to
Breeze and is checked explicitly rather than assumed.

**Reproducibility.** The v1 probe was re-run against `6.7.3-1.fc43.1` on
2026-08-02 and reproduced the result committed on 2026-07-15 byte for byte,
across the intervening two weeks and several package changes. The committed file
was not overwritten; the re-run was compared against it.

Earlier builds — `6.7.3-1.fc43.2` (patch 0001), `6.7.3-1.fc43.3` (patch 0002) and
`6.7.3-2.fc43.4` (patch 0003) — were measured against the 972-configuration
matrix that preceded the view-mode and word-wrap axes. Those runs are retained
unaltered; see §5 on format compatibility.

**A note on `6.7.3-1.fc43.1` versus `6.7.3-2.fc43.1`.** Both are unpatched
rebuilds of the same 6.7.3 source; every release bump between them is a rebuild,
recorded as such in the package changelog. Measured on the full matrix they give
identical results — 218 Fusion-check failures, the same 678 differing
configurations against patch 0004 — which is the expected outcome and was
confirmed rather than assumed. The `-1.fc43.1` figures are the ones quoted here,
because that is the build the screenshots in
[`screenshots/01-stock-6.7.3-1.fc43.1/`](../screenshots/01-stock-6.7.3-1.fc43.1/)
were taken against.

## 8. Why the instruments check themselves

The quantity under study is six pixels. Anything that shifts an item's width by a
few pixels is the same magnitude as the entire effect, which makes this a subject
where an instrument can be wrong by a plausible amount rather than an obvious
one. During development the tools in this repository produced three wrong results
that all *looked* like clean readings:

1. **A measurement that stopped one step short.** The test bed compared the
   string against the width `subElementRect()` returned, and reported that text
   fitted. It did not: `viewItemDrawText()` removes `tMargin` from each side
   afterwards. The reading was internally consistent and disagreed with the
   screen.
2. **A stale geometry.** `QListView` caches its content width and does not
   recompute it when the viewport shrinks. After the list had once been wide, the
   item rectangle was reported as 640px for a 150px list — a plausible number, in
   the right units, simply not current.
3. **A silent zero.** Two sweeps run concurrently wrote into one directory, and
   the summariser read a dump while it was still being written. The affected
   configurations summarised as `0/0` — indistinguishable, at a glance, from
   "no rows elided".

4. **A format change wearing the costume of a result.** After two axes were added
   to the matrix, every dump gained two header lines. Comparing a new run against
   an older one reported all 972 shared configurations as differing, including
   those for styles a Breeze change cannot reach — which is what gave it away.

5. **A complete matrix that was not comprehensive.** The most consequential of
   the five. A change measured across 972 configurations, with every acceptance
   criterion met, still elided labels in a real application, because two
   properties of that application's view had no axis in the matrix. Coverage of
   everything measured says nothing about what was never measured.

6. **A comparison against archived results.** Once a run's dumps are packed
   (§4), `compare.sh` finds no files to read and reports "0 identical, 0
   differing, 3888 only in A" — which reads as a result rather than as the
   mistake it is. It now refuses to compare a packed run and says which one to
   unpack.

None of the six announced itself. Each was found by comparing one source of
truth against another — the numbers against the rendering, a measurement against
the geometry it claimed to describe, or a style's results against a style the
change could not have touched.

The tools therefore carry checks that make these failures loud:

- The rendered list is treated as ground truth. Where a computed column and the
  display disagree, the display is correct and the column is marked with the
  assumption that makes it wrong (§2).
- Item geometry is recomputed on every width change, so a cached width cannot
  survive into a measurement.
- `sweep.sh` takes a lock and refuses to run concurrently with itself.
- A dump that parses to zero rows is reported as `*** NO ROWS PARSED`, never as
  `0/0`. A failure must not be able to present as a result.
- `compare.sh` detects a dump-format mismatch between two runs and says so,
  rather than reporting the difference as thousands of changed measurements.
- `compare.sh` warns when a comparison shows a style changing that the change
  under test cannot reach. That is the signal that something other than the
  change is being measured.
- `compare.sh --fusion-check` answers, within a single run, whether Breeze elides
  any label that Fusion does not. Fusion applies no inset, so it is the reference
  for the best a change to `SE_ItemViewItemText` could achieve. The check was
  verified to **fail** against a build known to have the defect before it was
  trusted against the build proposed to fix it — an acceptance test that has
  never failed is an assertion, not a test.

Readers reproducing these measurements are encouraged to treat agreement between
the table and the rendering as the thing that makes a result believable, rather
than the absence of an error message. The visual captures in
[`screenshots/`](../screenshots/) are not illustrations of the measurements; on
one occasion they were the only thing that contradicted them, and they were
right.

## 9. Known limitations

Recorded so that results are not read as claiming more than they establish.

*Added 08/02/2026:* the full statement of what this matrix does and does not
reach is in the repository overview under
[What Was and Was Not Tested](../README.md#what-was-and-was-not-tested). The
items below are the ones bearing directly on how the instruments work; that
section covers the coverage boundary as a whole — notably tree and table views,
`SelectRows` selection behaviour, RTL layouts, explicit line breaks, complex
text shaping, and display scaling, none of which are exercised here. The
regression itself is established independently of that list; what the list
bounds is what can be claimed for a *patch*.

- The `elides` column is computed with `QFontMetrics::elidedText()`. Qt lays text
  out through `QTextLayout`, so a shortfall of one or two pixels may not always
  cross the same threshold. Where the column and the rendered list disagree, the
  rendered list is correct.
- The measurements describe a `QListWidget` with `SelectItems` behaviour.
  `Helper::itemViewItemMargins()` contains a further branch on
  `viewItemPosition` that is only reached with `SelectRows`, as used by some
  tree and table views. That path is not covered here.
- The difference in available text width between Breeze and Fusion, in a live
  view, is **8px**, and it has two independent causes. Quoting the total as
  though it were the inset overstates what `SE_ItemViewItemText` does.

  **6px is the inset.** `itemViewItemMargins()` returns 1px per side once the
  view's `QFrame::StyledPanel` is detected, plus `ItemView_ItemPaddingWidth`
  of 2px per side: 3 per side, 6 in total. This is the subject of the change
  under discussion.

  **2px is the frame.** Breeze's `PM_DefaultFrameWidth` returns
  `Metrics::Frame_FrameWidth` (2) for a `QAbstractScrollArea` that sits in a
  layout with spacing and more than one item — *"Add frame when scroll area is
  in a layout with more than an item and the layout has some spacing"*,
  `breezestyle.cpp:708-712`. Fusion returns 1. The viewport is therefore 2px
  narrower under Breeze, before any item-text calculation happens. This is
  deliberate, is unrelated to text layout, and would not be altered by a change
  to `SE_ItemViewItemText`.

  The same metric returns 0 for a `QListWidget` with no parent and no layout,
  so this contribution is context-dependent and is absent from measurements
  taken on a bare, unparented widget.

- The probes force both styles to an identical `itemWidth`, so they measure the
  6px inset alone. The test bed lets each style determine its own viewport, so
  it measures 6 + 2 = 8. Neither is wrong; they answer different questions, and
  a result should state which.
