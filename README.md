# breeze-itemview-text-elision

> **Partially superseded — see [Update 08/01/2026](#update-08012026) below.**
> Further measurement refined the size of the effect and produced a fourth patch,
> which is now the suggested one. The original findings stand and are kept as
> written; the update states what changed and why.

Reproduction case, numeric probe, patches, and documentation for a label elision
bug in the KDE Breeze style plugin introduced in February 2026.

**Bug**: commit [`aba0f922b`](https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431) in `plasma/breeze` added two lines to
`Style::subElementRect(SE_ItemViewItemText)` that unconditionally discard 8px from
the width of every list and tree view item's text rect — 4px per side — regardless
of column width. Any Qt5 or Qt6 application using the Breeze style with narrow or
medium-width columns will elide labels that would otherwise fit.

> **Correction, 08/02/2026 — two figures in the paragraph above are superseded by
> later measurement. The paragraph is retained so the correction is visible.**
>
> - **The inset is 3px per side, not 4.** In a framed item view —
>   which is every view in every application examined here —
>   `Helper::itemViewItemMargins()` reduces its margins once it detects the
>   view's `QFrame::StyledPanel`. The total taken from the text rect is
>   **6px**, not 8px. The 8px figure describes an *unframed* view, which is
>   what the original probes measured; see finding 1 of
>   [Update 08/01/2026](#update-08012026).
> - **"Any Qt5 or Qt6 application" overstates the Qt6 case.** On Qt 5.15 the
>   inset causes truncation even in views with ample width to spare, because
>   of an interaction with Qt's own natural-width clamp. Qt 6 removed that
>   clamp, so there the inset generally becomes visible only near the
>   available-width boundary. Both are avoidable truncation; they are not the
>   same magnitude of effect. See finding 3 of the update.
>
> The located cause is unchanged: the two lines added by `aba0f922b` take
> width from the text rect unconditionally, and removing or guarding them
> eliminates the truncation measured here.

**Fix**: remove the two horizontal-narrowing lines. See [`patches/`](patches/).
*(Superseded — [Patch 4](#patch-4--the-proposed-change) keeps the inset where the item has
room for it and yields it only where it would cause elision. See the update below.)*

**KDE Bug Report**: https://bugs.kde.org/show_bug.cgi?id=523118

**KDE Merge Request**: https://invent.kde.org/plasma/breeze/-/merge_requests/627

---

## Update 08/01/2026

Everything above was written from the first round of measurement and remains
accurate in substance: commit `aba0f922b` narrows the item text rect
unconditionally, the narrowing is absent from Fusion and Windows under identical
conditions, and removing the two lines eliminates it. A second round of
measurement — a live-view test harness and two additional probes, both described
in [`docs/test-plan.md`](docs/test-plan.md) — refined the size of the effect,
identified two mechanisms that had been missed, and produced a fourth patch which
is now the suggested one.

**1. The narrowing is 6px in a framed item view, not 8px.**
`Helper::itemViewItemMargins()` reduces its left and right margins from 2px to 1px
once it detects the view's `QFrame::StyledPanel`, so the inset is
`1 + ItemView_ItemPaddingWidth` = 3px per side. Every item view in every
application examined here is framed. The 8px figure is correct for an *unframed*
view, and the original probes measured that case because
`QStyleOption::initFrom()` does not populate `QStyleOptionViewItem::widget` — the
frame check therefore never fired. Probe v3 reports both cases side by side.

A live view does show 8px less text under Breeze than under Fusion, but only 6px
of that belongs to `SE_ItemViewItemText`. The remaining 2px is Breeze's
`PM_DefaultFrameWidth` returning `Metrics::Frame_FrameWidth` (2) for a scroll area
in a spaced multi-item layout where Fusion returns 1, making the viewport 2px
narrower before any text calculation begins. That difference is deliberate,
unrelated to text layout, and unaffected by any of these patches.

**2. Qt removes the padding a second time, after the style has finished.**
`QCommonStylePrivate::viewItemDrawText()` removes `PM_FocusFrameHMargin + 1` from
each side of whatever `subElementRect()` returned, immediately before eliding. The
width the text is actually laid out in is therefore `textWidth - 2 * textMargin`,
not `textWidth`. Any measurement that stops at `subElementRect()` reports that
text fits when it visibly elides.

**3. The effect is far worse on Qt 5.15 than on Qt 6, for a reason outside
Breeze.** Qt 5.15's `viewItemLayout()` clamps the text rect to the string's
natural width when `showDecorationSelected` is false — which is what Breeze's
style hint reports, while Fusion's reports true. Qt 6 removed that branch. The
consequence is that on Qt5 every label is short by exactly `2 * textMargin`
regardless of how wide the view is: measured, all seven labels of a test list
truncate in a **600px-wide** list, including the two-character string "Ii".

**4. A width-guarded patch, now the suggested one.**
[Patch 4](#patch-4--the-proposed-change) takes the inset only from space the text does not
need. It fixes every case Patch 1 fixes, and preserves the clearance wherever the
item is wide enough that removing it would achieve nothing.

**5. The test harness was expanded, and its output format changed.** Patch 3, an
earlier form of the same idea, excluded word-wrapped text from the guard. That
looked correct across 972 configurations and still elided labels in KeePassXC's
entry-editor sidebar, which is a `QListView` in `IconMode` with `wordWrap`
enabled. Neither property was an axis in the matrix, so no measurement could have
caught it; a screenshot did. View mode and word wrap are now axes, which grew the
matrix to 3888 configurations and added two header lines to every dump file.

Results produced before 2026-08-01 have neither the new axes nor those header
lines. They are still valid — they are simply the ListMode, no-wrap subset — and
they have not been regenerated. The comparison tool maps between the two formats
rather than requiring old builds to be reinstalled and re-measured.

Supporting measurements for all of the above are in
[`probe/results/`](probe/results/) and reproducible via
[`docs/test-plan.md`](docs/test-plan.md).

---

## Affected Versions

- `plasma-breeze` 6.7.3 (and likely earlier point releases after 2026-02-25)
- Both `breeze5.so` (Qt5) and `breeze6.so` (Qt6) are compiled from the same
  `kstyle/breezestyle.cpp` and carry the same bug

## Confirmed Affected Applications

| Application | Qt | Symptom |
|---|---|---|
| KeePassXC 2.7.x | Qt5 | Entry edit dialog sidebar: Entry/Advanced/Icon/Auto-Type/Properties tabs elide |
| SMPlayer 25.6.0 | Qt5 | Preferences sidebar elides when style is set to Breeze |
| GoldenDict | Qt5 | Word list pane elides multi-word entries |

SMPlayer defaults to the Fusion style and is not affected under default
configuration; elision appears when the user changes Preferences → General → Style
to Breeze. Source-level trace: [`docs/smplayer-source-analysis.md`](docs/smplayer-source-analysis.md)
— the Preferences sidebar has no custom item delegate at all, so it is the
most directly exposed of the three tested applications.

## The Regression Is in the Breeze Style, Not in Application Code

> *Section heading changed 08/02/2026.* It previously read "This Is a Qt API
> Defect, Not a KDE-Application Defect," which was misleading: the code in
> question is Breeze's, and the Qt API it implements is not itself at fault.
> The section's content and conclusion are unchanged.

`SE_ItemViewItemText` is a Qt Widgets API (`QStyle::SubElement`), not a KDE
Frameworks primitive. Every mechanism that exposes the behaviour to an
application is stock Qt:

- `QStyleFactory` — the plugin loader that turns `widgetStyle=Breeze` in
  `kdeglobals` into a loaded `breeze5.so`/`breeze6.so` — is Qt Widgets, not
  KDE Frameworks. See [`style-plugin-loading.md`](docs/style-plugin-loading.md).
- `QProxyStyle`, used by both KeePassXC (`IconSelectionCorrectedStyle`) and
  SMPlayer (`MyProxyStyle`) to layer small per-app tweaks over the active
  style, is stock Qt Widgets.
- `QCommonStyle::drawControl(CE_ItemViewItem)`, which calls
  `subElementRect(SE_ItemViewItemText)` internally, is Qt's own
  cross-platform item-view painting implementation — the same one Fusion and
  Windows inherit from.

Neither KeePassXC nor SMPlayer is a KDE application (neither links
`KF6::*`), and neither contains KDE-specific code anywhere in its item-view
paint path — see [`sequence-diagrams.md`](docs/sequence-diagrams.md),
[`keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md), and
[`smplayer-source-analysis.md`](docs/smplayer-source-analysis.md) for
source-level traces of both. GoldenDict is likewise a plain Qt Widgets
application. All three exhibit the behaviour purely because they are Qt
applications running under a KDE session with Breeze as the active
`QStyle` — the same exposure any Qt application would have, KDE-linked
or not.

The changed code is in `plasma-breeze` (`kstyle/breezestyle.cpp`) — but the
*mechanism* by which it reaches unrelated applications is Qt's platform-style
plugin system working exactly as designed. The point of this section is that
the applications are not doing anything unusual, not that any component is at
fault. See
[`se-itemviewitemtext-proof.md`](docs/se-itemviewitemtext-proof.md) for the
controlled comparison that isolates the Breeze-specific contribution to that
one function, independent of any application.

## Qt6 Scope

`breeze6.so` carries the identical code path. KeePassXC's Qt6 port (`feature/qt-feature`,
targeting 2.8.x) routes `CategoryListWidget` text rendering through `CE_ItemViewItem`
via a `QProxyStyle`, which delegates to `subElementRect(SE_ItemViewItemText)` on the
active Breeze style. The elision will reproduce on 2.8.x unless the upstream
`plasma-breeze` bug is fixed first. See [`docs/keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md).

> **Correction, 08/02/2026 — the last sentence above is wrong, and is retained
> only so the correction is visible.**
>
> That prediction was made from source reading. The Qt6 port has since been
> built and run, and **it does not reproduce the elision on unpatched Breeze.**
>
> | | screenshot |
> |---|---|
> | unpatched `6.7.3-1.fc43.1` | [`01-stock…/keepassxc-theme-classic-2.8-snapshot-fabfba2c.png`](screenshots/01-stock-6.7.3-1.fc43.1/keepassxc-theme-classic-2.8-snapshot-fabfba2c.png) |
> | patched `6.7.3-2.fc43.5` | [`05-patch4…/keepassxc-theme-classic-2.8-snapshot-fabfba2c.png`](screenshots/05-patch4-6.7.3-2.fc43.5/keepassxc-theme-classic-2.8-snapshot-fabfba2c.png) |
>
> Both show `Entry / Advanced / Icon / Auto-Type / Properties` rendered in full.
> Build: KeePassXC 2.8.0-snapshot, branch `feature/qt-feature`, commit
> `fabfba2c`, compiled from source; the launching terminal is in frame in each
> capture, recording the `plasma-breeze` package under test. The *Classic
> (Platform-native)* theme is used because that is what puts Breeze in the paint
> path — KeePassXC's own themes do not. **That branch is under active
> development, so any result from it describes the commit named above and
> nothing more.**
>
> **Why it does not reproduce** is the Qt5/Qt6 difference described in
> [Update 08/01/2026](#update-08012026), not anything specific to KeePassXC.
> Qt 5.15 clamps the item text rect to the string's natural width, so the inset
> is subtracted from a rectangle that has no slack; Qt 6 removed that clamp, so a
> label only truncates when the view is genuinely too narrow for it. Measured in
> that same sidebar layout at 96px, on unpatched Breeze:
>
> ```
> qt5  Breeze  8/8 labels elided     qt5  Fusion  6/8
> qt6  Breeze  6/8 labels elided     qt6  Fusion  6/8   <- no Breeze-specific elision
> ```
>
> On Qt5, Breeze truncates two labels that Fusion does not. On Qt6 the two styles
> agree. The Qt5 analysis in
> [`docs/keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md) is unaffected;
> only the forward-looking claim about 2.8.x is withdrawn.

---

## Workaround (No Rebuild Required)

Force a single application to use the Fusion style instead of Breeze:

```bash
# One-off launch:
QT_STYLE_OVERRIDE=Fusion keepassxc

# Persistent — edit the application's .desktop Exec= line:
Exec=env QT_STYLE_OVERRIDE=Fusion /usr/bin/keepassxc %f
```

Fusion's `subElementRect(SE_ItemViewItemText)` returns the full column width
(`diff=0` at all tested widths). The trade-off is that the application loses
Breeze rendering system-wide for that session.

---

## Patches

**Patch 4 is the proposed change. It is the only one being proposed.**

Patches 1, 2 and 3 are the experimental record, not candidates. Each was built
to test a hypothesis about the mechanism, and each was measured; what they
produced was a sharper understanding of the mechanism and the instruments used
to measure it. They are published because the results are what justify Patch 4,
and because a reader should be able to see which hypotheses were tried and what
each one cost. They are not alternatives on offer:

| | | |
|---|---|---|
| Patch 1 | remove the inset outright | Established that these two lines are the Breeze-specific contribution. Correct as far as it goes, and it discards the clearance the original commit was added to provide. |
| Patch 2 | remove only `ItemView_ItemPaddingWidth` | Tested whether a partial reduction is enough. It is not, on Qt5 — the clamp leaves it 2px short at every width. |
| Patch 3 | width guard, excluding wrapped text and `ElideNone` | Tested whether the guard needs exclusions. It does not; the exclusions truncated labels that Patch 1 renders in full, and finding that grew the test matrix by two axes. |
| **Patch 4** | **width guard, no exclusions** | **The proposed change.** |

**What these patches are trying to preserve.** The inset introduced by
`aba0f922b` reserves horizontal clearance so item text does not sit flush
against the highlight edge. That is a legitimate design choice, and a style is
entitled to reserve spacing that another style does not; Fusion's geometry is
used throughout this repository as a *control* — a same-process comparison that
holds everything but the style constant — not as a definition of what Breeze
ought to return. The argument here is narrower than "Breeze differs from
Fusion":

- Breeze reserves the clearance unconditionally, without regard to whether the
  item has width to spare.
- On Qt 5.15 that interacts with `QCommonStyle`'s natural-width clamp such that
  there is never spare width to take it from, so labels truncate even in views
  many times wider than the text.
- The truncation is therefore avoidable — the clearance and the full label are
  not actually in competition at most widths.
- Patch 4 attempts to keep the visual intent while removing the avoidable loss;
  Patch 1 removes the loss by removing the intent.

### Patch 4 — the proposed change

[`patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch`](patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch)

Takes the horizontal inset only from space the text does not need. Where the item
is wide enough to spare it, the full inset is applied exactly as before; where it
is not, the text keeps the width it needs rather than being elided by the inset
itself.

**Why there is no Qt version conditional.** The Qt5/Qt6 difference here is real
and large: Qt 5.15 clamps the item text rect to the string's natural width, so
the inset is subtracted from a rectangle with no slack at any view width, and
Qt 6 removed that clamp. Guarding a fix on the Qt major version is a reasonable
thing to want, given that.

A width guard removes the need for one, because it keys off the same quantity
the version difference expresses itself through. Where the text has room the
inset is applied in full; where it does not, it is released. Qt 6 usually has
room and Qt 5.15 does not, so the version-dependent behaviour falls out of the
measurement rather than out of a preprocessor branch — including in the cases
where Qt 6 genuinely runs out of room, which a version guard would leave
truncated.

The measured form of that claim is the `Qt6 ListMode ≥220px, vs stock —
identical` row [below](#patch-4--the-proposed-change): on the platform where the clearance
is affordable, this patch produces byte-identical output to stock. Being
inactive on Qt6 wherever there is room is the property a version guard would be
bought to obtain.

`breeze5.so` and `breeze6.so` are also compiled from one translation unit, so a
conditional here means two behaviours maintained in one file rather than a
choice between two builds.

The comparison is made against `textWidth - 2 * textMargin`, because
`viewItemDrawText()` removes `textMargin` from each side again after
`subElementRect()` returns. `textMargin` is obtained through `proxy()->pixelMetric()`,
the same source `QCommonStyle` uses, so the two values are the same by
construction. (That is a statement about where the margin comes from, not a
claim that the guard's width estimate matches Qt's final layout — see the
limitation below.)

**No case is excluded.** Patch 3 excluded word-wrapped text and `Qt::ElideNone`,
on the reasoning that `horizontalAdvance()` does not describe wrapped text and
that clipped text is not elided text. Excluding them turned out to be the worse
trade in both cases: the text still loses those pixels either way. For wrapped
text the advance is an overestimate, so the guard releases more of the inset
than strictly necessary — costing clearance that was not needed rather than a
character.

**Known limitation — what the guard is and is not.** The guard is a heuristic
backed by the tested matrix, not a proof of general correctness. It uses
`QFontMetrics::horizontalAdvance()` as its estimate of the width the string
requires, while Qt ultimately lays the string out through `QTextLayout`. Where
the advance is an *over*estimate — wrapped or multi-line text, and shaping that
narrows a run — the guard releases more of the inset than it needed to, which
costs clearance and nothing else. Where it could be an *under*estimate, the
guard would retain more of the inset than the string can spare, and the result
would be worse in that configuration than simply removing the inset. No such
case was observed anywhere in the 3888 configurations measured here, but the
matrix does not cover every text-shaping case Qt supports; see
[what was and was not tested](#what-was-and-was-not-tested). This is the
trade Patch 4 makes deliberately: it keeps the clearance the original commit
was added to provide, at the cost of depending on a width estimate that
[Patch 1](#patch-1--hypothesis-test-retained-for-the-record) — which simply
removes the inset — does not need.

Measured against the matrix in [`docs/test-plan.md`](docs/test-plan.md),
3888 configurations:

| | result |
|---|---|
| Breeze elides a label where Fusion does not | **0 of 864 configurations** |
| Breeze configurations changed vs stock | **136 of 216** in the comparable subset |
| Qt6 ListMode ≥220px, vs stock | **identical** — clearance preserved |
| Qt5 ListMode ≥220px | 7/7 labels truncated → **0/7** |
| icon mode + word wrap (KeePassXC's layout) | **Breeze ≡ Fusion** |
| `SE_ItemViewItemDecoration` rects moved | **0** |
| non-Breeze styles affected | **0** |
| dumps failing to parse | **0** |

Reading the rows together: on Qt6 the inset is retained wherever there is room
for it, and released only where the label would otherwise be truncated. On Qt5
the clamp described in the update above means there is never room to retain it,
so the guard releases it in full and the result is equivalent to Patch 1.

```
Qt6, stock:     220px diff=6   400px diff=6   150px diff=6   (elides at 150px)
Qt6, Patch 1:   220px diff=0   400px diff=0   150px diff=0   (clearance lost everywhere)
Qt6, Patch 4:   220px diff=6   400px diff=6   150px diff=0   (kept where free, released where needed)
```

Where the option carries no text, behaviour is identical to stock.

### Patch 3 — superseded, retained for the record

[`patches/0003-width-guarded-itemviewitem-text-inset.patch`](patches/0003-width-guarded-itemviewitem-text-inset.patch)

Identical to Patch 4 except that it excluded word-wrapped text and
`Qt::ElideNone` from the guard, keeping the full inset in those cases. It is kept
here because it was measured, because its failure is instructive, and because the
screenshots and corpus for it describe a real build.

Across 972 ListMode configurations it appeared correct: 68 changed against stock,
the Qt6 wide rows byte-identical, no decoration rects moved, no other style
affected. It nonetheless **elided labels that Patch 1 renders in full**, in
KeePassXC's entry-editor sidebar:

```
                   Patch 1        Patch 3        Patch 4
sidebar labels     Entry          En...          Entry
                   Advanced       Advan...       Advanced
                   Icon           I...           Icon
                   Auto-Type      Auto-...       Auto-Type
```

That sidebar is a `QListView` in `IconMode` with `wordWrap` enabled
(`CategoryListWidget.ui`), so `QAbstractItemView::viewOptions()` sets
`QStyleOptionViewItem::WrapText`, the exclusion fired, and the full inset was
kept. Measured: `Entry` received 26px of drawing area against a 32px advance.

Two things are worth taking from this rather than only the fix:

- **The measurements did not catch it.** The matrix had no view-mode or word-wrap
  axis, so no configuration in it could have. The screenshots caught it. Both
  axes have since been added, and the acceptance check was verified to *fail*
  against this build before being trusted against its successor.
- **A patch can be correct in every case you measured and still be worse than a
  simpler one.** Patch 3 was never worse than stock; it was worse than Patch 1,
  which is a different and easier failure to miss.

### Patch 1 — hypothesis test, retained for the record

[`patches/0001-remove-horizontal-text-rect-narrowing.patch`](patches/0001-remove-horizontal-text-rect-narrowing.patch)

Removes both horizontal-narrowing lines from `SE_ItemViewItemText`. Text rect width
equals the full column width, matching Fusion and Windows behavior at every tested
column width (`diff=0`). The rounded highlight rect is painted independently via its
own inset and is not affected.

```diff
--- a/kstyle/breezestyle.cpp
+++ b/kstyle/breezestyle.cpp
@@ -1062,8 +1062,6 @@ QRect Style::subElementRect(...)
         QRect rect = ParentStyleClass::subElementRect(element, option, widget);
         if (viewItem) {
             const QMargins margins = _helper->itemViewItemMargins(viewItem);
-            rect.setRight(rect.right() - margins.right() - Metrics::ItemView_ItemPaddingWidth);
-            rect.setLeft(rect.left() + margins.left() + Metrics::ItemView_ItemPaddingWidth);
             rect.moveTop(rect.top() + margins.top() - margins.bottom());
         }
```

### Patch 2 — considered, insufficient

[`patches/0002-partial-keep-margins-remove-itempadding.patch`](patches/0002-partial-keep-margins-remove-itempadding.patch)

Retains `itemViewItemMargins` (2px/side) and removes only `ItemView_ItemPaddingWidth`
(2px/side). Reduces total narrowing from 8px to 4px (`diff=4`):

```
itemWidth=150  textRect=(2,3 146x18)  textWidth=146  diff=4
itemWidth=120  textRect=(2,3 116x18)  textWidth=116  diff=4
itemWidth=100  textRect=(2,3  96x18)  textWidth= 96  diff=4
itemWidth= 80  textRect=(2,3  76x18)  textWidth= 76  diff=4
```

Tested against all three applications at default column widths — visible elision
persists in KeePassXC and SMPlayer. Presented for discussion; Patch 1 is the
recommendation.

*(Written before Patches 3 and 4 existed. Patch 4 is the proposed change; see
[Patches](#patches) above.)*

**Update 08/01/2026 — confirmed numerically.** The app-level observation above is
borne out by measurement. In a framed view Patch 2 leaves a 2px inset (margins
1px per side after the frame adjustment), and against the Qt 5.15 clamp that is
still 2px short of what the string needs. Across the full matrix, **all seven
labels of the test list remain truncated at every width from 96px to 600px** on
Qt5 — `slack` improves from −6 to −2 and never reaches zero. On Qt6, where there
is no clamp, Patch 2 is sufficient. Patch 2 is therefore not a viable fix for
Qt5, which is where the reported applications live.

---

## What Was and Was Not Tested

Added 08/02/2026, so that the measurements are not read as claiming more than
they establish. Nothing below changes a published result; it states the
boundary around them.

**Exercised and confirmed by measurement:**

| | |
|---|---|
| Toolkits | Qt 5.15.18 and Qt 6.10.3, both against the same `breezestyle.cpp` |
| Widget | `QListWidget` / `QListView` with `SelectItems` selection behaviour |
| View modes | `ListMode` and `IconMode` |
| Word wrap | on and off |
| Elide modes | `ElideRight` and `ElideNone` |
| Widths | 96px to 600px, in the steps given in [`docs/test-plan.md`](docs/test-plan.md) |
| Icons | present and absent; selected and unselected state |
| `opt.widget` | set and unset, so the framed and unframed cases are both reported |
| Styles | Breeze, Fusion, Windows, and the Kvantum variants present on the test system |
| Applications | KeePassXC 2.7.x, SMPlayer 25.6.0, GoldenDict — all Qt5, all captured per build |

**Not exercised.** These are gaps in coverage, not known failures. Most of them
apply to stock Breeze as much as to any patch here; they are listed because a
change to `SE_ItemViewItemText` is a change to *all* item views, and this
matrix does not reach all of them:

- `QTreeView` and `QTableView`.
- `SelectRows` selection behaviour. This one is the most material:
  `Helper::itemViewItemMargins()` contains a branch on `viewItemPosition` that
  is only reached under `SelectRows`, so it is code the patches interact with
  that no measurement here has run. Noted also in
  [`docs/test-plan.md`](docs/test-plan.md) §9.
- Right-to-left layouts.
- Strings containing explicit line breaks or tabs.
- Combining marks, emoji, and scripts requiring complex shaping — the cases
  where `horizontalAdvance()` is least likely to describe the final layout.
- Fonts other than the session default, and display scaling other than 1×,
  including high-DPI rounding.
- Custom delegates that reach `CE_ItemViewItem` by a path other than
  `QStyledItemDelegate`.
- Styles that override `CE_ItemViewItem` outright and so never call
  `subElementRect(SE_ItemViewItemText)` — Kvantum does this, which is why its
  rows appear in the corpus but are not comparable.

The regression itself is established independently of this list: it reproduces
in three applications, in a controlled same-process style comparison, and in a
live-view harness. The list bounds what can be said about the *patches* — Patch
4 is a candidate validated across the matrix above, not a change demonstrated
correct for every item view Breeze styles.

---

## Numeric Probe

[`probe/`](probe/) contains standalone Qt5 and Qt6 programs that measure `SE_ItemViewItemText`
width against the full item rect at four column widths across three styles.

A hardened `-v2` variant of each (using a real, populated `QListWidget` instead
of a bare `QWidget`, plus an icon+selected variant) exists as a corroborating
cross-check — not a replacement — for the original probes; see
[`docs/se-itemviewitemtext-proof.md`](docs/se-itemviewitemtext-proof.md#6-hardened-cross-check-probe-v2).
Build with `make v2`.

`-v3` and `-v4` were added 08/01/2026. **v3 is the one that reports what a real
framed view receives**: it assigns `QStyleOptionViewItem::widget` explicitly,
which `QStyleOption::initFrom()` does not do, and prints the framed and unframed
cases side by side so the v1/v2 numbers are reproduced rather than contradicted.
v4 isolates `showDecorationSelected`. Build with `make v3` / `make v4`.

### Pre-built binaries

All eight probes are committed as pre-built x86_64 binaries so they can be run
directly, without installing Qt development or private headers:

| Binary | Built | Toolchain | Reports |
|---|---|---|---|
| [`probe/qt5-style-elide-test`](probe/qt5-style-elide-test) | 2026-07-15 | Fedora 43 x86_64, Qt 5.15.18 | unframed (`opt.widget` unset) |
| [`probe/qt-style-elide-test`](probe/qt-style-elide-test) | 2026-07-15 | Fedora 43 x86_64, Qt 6.x | unframed (`opt.widget` unset) |
| [`probe/qt5-style-elide-test-v2`](probe/qt5-style-elide-test-v2) | 2026-07-29 | Fedora 43 x86_64, Qt 5.15.18 | unframed, real `QListWidget` + icon/selected |
| [`probe/qt-style-elide-test-v2`](probe/qt-style-elide-test-v2) | 2026-07-29 | Fedora 43 x86_64, Qt 6.10.3 | unframed, real `QListWidget` + icon/selected |
| [`probe/qt5-style-elide-test-v3`](probe/qt5-style-elide-test-v3) | 2026-08-01 | Fedora 43 x86_64, Qt 5.15.18 | **framed and unframed**, plus `drawn`/`slack`/`elides` |
| [`probe/qt-style-elide-test-v3`](probe/qt-style-elide-test-v3) | 2026-08-01 | Fedora 43 x86_64, Qt 6.10.3 | **framed and unframed**, plus `drawn`/`slack`/`elides` |
| [`probe/qt5-style-elide-test-v4`](probe/qt5-style-elide-test-v4) | 2026-08-01 | Fedora 43 x86_64, Qt 5.15.18 | `showDecorationSelected` isolated |
| [`probe/qt-style-elide-test-v4`](probe/qt-style-elide-test-v4) | 2026-08-01 | Fedora 43 x86_64, Qt 6.10.3 | `showDecorationSelected` isolated |

Each probe prints the Qt version and the active style name it resolved before
reporting any measurements, so you can confirm what your run actually exercised
rather than relying on the table above.

### Build

To rebuild from source:

```bash
cd probe
make              # v1 only: qt5-style-elide-test and qt-style-elide-test (Qt6)
make v2 v3 v4     # the later probes — each is a separate opt-in target
```

`make` deliberately builds only the v1 probes, because those are the binaries
cited in the bug report and the merge request and they are meant to rebuild
without pulling in anything else. The later probes corroborate and extend them
rather than replacing them, so each is its own target.

**Dependencies:**

| Program | Package (Fedora) |
|---|---|
| `qt5-style-elide-test` | `qt5-qtbase-devel`, `qt5-qtbase-private-devel` |
| `qt-style-elide-test` | `qt6-qtbase-devel`, `qt6-qtbase-private-devel` |

A KDE Breeze style plugin must be installed and the test run in a KDE session (or
with `QT_QPA_PLATFORMTHEME=kde`) for the Breeze row to reflect actual plugin values.

### Run

```bash
./run-clean.sh ./qt5-style-elide-test
```

[`run-clean.sh`](probe/run-clean.sh) clears all Qt/KDE style and theme environment variables before
launching, then echoes the cleared state and the installed `plasma-breeze` RPM
version. This ensures results are driven by `kdeglobals` and the installed plugin,
not session overrides.

### Expected Output

**Unpatched `plasma-breeze` 6.7.3:**
```
[breeze  ]  itemWidth=150  textRect=(4,3 142x18)  textWidth=142  diff=8
[Fusion  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
```

**With Patch 1:**
```
[breeze  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
[Fusion  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
```

Full probe output for all three test builds is in [`probe/results/`](probe/results/).

**Update 08/01/2026.** The output above is from probes v1/v2, which leave
`QStyleOptionViewItem::widget` unset and therefore measure the unframed case
(`diff=8`). Probes v3 and v4 set it, report the framed case (`diff=6`), and add
`drawn`, `slack` and `elides` columns so that the second `textMargin` removal in
`viewItemDrawText()` is accounted for. v3 prints both the framed and unframed
cases side by side, so the v1/v2 numbers above are reproduced rather than
contradicted. Results for all four builds — stock, Patch 1, Patch 2, Patch 3 —
are in [`probe/results/`](probe/results/).

---

## Test Bed

[`testbed/`](testbed/) contains a live-view harness that renders a real
`QListWidget` and reports, per row, the numbers behind what is on screen —
`textWidth`, `advance`, `tMargin`, `drawn`, `slack`, and whether the label
elides. The probes measure `subElementRect()` in isolation; the test bed
measures a view that is actually laid out and painted, so a disagreement
between the two is visible rather than silent.

It models SMPlayer's Preferences sidebar, with each string traced to its origin
in `preferencesdialog.{ui,cpp}`. Style, string set, list width, icons, elide
mode, view mode and word wrap are all selectable from the GUI, and the same
measurement path serves a `--dump` mode used for batch runs.

| | |
|---|---|
| [`testbed/README.md`](testbed/README.md) | What it is, how to build and drive it |
| [`testbed/sweep.sh`](testbed/sweep.sh) | Runs the full matrix — 3888 configurations — into `corpus/<rpm>/<date>-<NN>/` |
| [`testbed/compare.sh`](testbed/compare.sh) | Diffs two runs; `--fusion-check` runs the acceptance check against a single run |
| [`testbed/pack-results.sh`](testbed/pack-results.sh) | Packs a completed run for storage; `--unpack` restores it |
| [`docs/test-plan.md`](docs/test-plan.md) | The methodology: instruments, term definitions, the matrix and the reason for each axis |

The corpus for every build is stored packed. `compare.sh` refuses a packed run
rather than reporting an empty comparison as though it were a result.

---

## Screenshots

[`screenshots/`](screenshots/) is organized by build:

| Directory | Build | Patch |
|---|---|---|
| [`01-stock-6.7.3-1.fc43.1/`](screenshots/01-stock-6.7.3-1.fc43.1) | Unpatched stock | none (truncation present) |
| [`02-patch1-6.7.3-1.fc43.2/`](screenshots/02-patch1-6.7.3-1.fc43.2) | Patch 1 | remove both narrowing lines |
| [`03-patch2-6.7.3-1.fc43.3/`](screenshots/03-patch2-6.7.3-1.fc43.3) | Patch 2 | retain margins, remove padding |
| [`04-patch3-6.7.3-2.fc43.4/`](screenshots/04-patch3-6.7.3-2.fc43.4) | Patch 3 | width guard, excluding wrapped text and `ElideNone` |
| [`05-patch4-6.7.3-2.fc43.5/`](screenshots/05-patch4-6.7.3-2.fc43.5) | **Patch 4 — suggested** | width guard, no exclusions |

Every directory contains `keepassxc.png`, `smplayer-1.png`, `smplayer-2.png`, and
`goldendict.png` for that build. Beyond those:

| File | Present in | What it shows |
|---|---|---|
| `smplayer-style-fusion.png` | 03, 04, 05 | The same SMPlayer binary with Style=Fusion — `diff=0`, no truncation. Changing one dropdown is the whole variable, which is what isolates the result to the style plugin. |
| `smplayer-style-default.png` | 03, 04, 05 | The same binary at SMPlayer's default style, for comparison against the Fusion capture. |
| `keepassxc-theme-classic.png` | 01, 03, 04, 05 | KeePassXC forced to its *Classic (Platform-native)* theme, which is what puts Breeze in the paint path; KeePassXC's own themes do not. |
| `keepassxc-*-2.8-snapshot-fabfba2c.png` | 01, 05 | KeePassXC's in-development Qt6 port at commit `fabfba2c`. See the correction under [Qt6 Scope](#qt6-scope) — these are the captures showing it does *not* reproduce on unpatched Breeze. |

Captures for builds 04 and 05 were added 08/02/2026; builds 01–03 are unchanged.
Each capture has the launching terminal in frame, recording the `plasma-breeze`
package the session was actually running.

---

## Documentation

| Document | Contents |
|---|---|
| [`docs/bug-analysis.md`](docs/bug-analysis.md) | Source commit, metric values, numeric measurement, per-app test results, Patches 1 and 2, secondary `CT_ItemViewItem` finding. Written before Patches 3 and 4 existed; those are covered in [Patches](#patches) above and in its own dated update |
| [`docs/style-plugin-loading.md`](docs/style-plugin-loading.md) | How Qt5 apps load `breeze5.so` via `QStyleFactory`; why KDE-linked apps are equally affected; per-app style selection behavior |
| [`docs/keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md) | Source-level call-chain trace for KeePassXC, Qt5 release and in-development Qt6 port. Its forward-looking claim about 2.8.x has since been **withdrawn** — the Qt6 port was built and does not reproduce the elision; see [Qt6 Scope](#qt6-scope) |
| [`docs/test-plan.md`](docs/test-plan.md) | How every measurement in this repository is produced and reproduced: the two instruments, the term definitions, the matrix and the reason for each axis, the measured results per build, and the checks the tools apply to themselves |
| [`docs/smplayer-source-analysis.md`](docs/smplayer-source-analysis.md) | Source-level call-chain trace for SMPlayer's Preferences sidebar — no custom delegate at all, the most direct exposure of the three tested apps |
| [`docs/sequence-diagrams.md`](docs/sequence-diagrams.md) | Mermaid sequence diagrams for KeePassXC and SMPlayer, from `paint()` down to `Style::subElementRect(SE_ItemViewItemText)` |
| [`docs/se-itemviewitemtext-proof.md`](docs/se-itemviewitemtext-proof.md) | Controlled same-process comparison isolating the Breeze-specific contribution to `subElementRect(SE_ItemViewItemText)`, independent of any application |
| [`patches/`](patches/) | `0004` is the proposed change. `0001`–`0003` are hypothesis tests retained as the experimental record — see [Patches](#patches) |
| [`testbed/`](testbed/) | Live-view harness, sweep and comparison drivers, and the measurement corpus per build — see [`testbed/README.md`](testbed/README.md) |

---

## Test Environment

- Fedora 43 (x86_64), KDE Plasma 6.7.3, Qt 5.15.18 and Qt 6.10.3
- `plasma-breeze` builds tested:

  | Build | Contents |
  |---|---|
  | `6.7.3-1.fc43.1` | stock, unpatched |
  | `6.7.3-1.fc43.2` | Patch 1 |
  | `6.7.3-1.fc43.3` | Patch 2 |
  | `6.7.3-2.fc43.4` | Patch 3 |
  | `6.7.3-2.fc43.5` | **Patch 4 — suggested** |

  Each is a local `mock` build of the Fedora `plasma-breeze` package with the
  corresponding patch applied and nothing else changed. Builds `.4` and `.5`
  were added 08/02/2026.
- [`probe/reset-app-profile.sh`](probe/reset-app-profile.sh) resets a test application's own saved settings, so a capture reflects known geometry rather than an accumulated session — it deliberately leaves `kdeglobals` alone, since that is what selects the style under test
- All applications launched via [`probe/run-clean.sh`](probe/run-clean.sh) to document session context and exclude environment-variable interference
