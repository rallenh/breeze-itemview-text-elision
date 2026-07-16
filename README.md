# breeze-itemview-text-elision

Reproduction case, numeric probe, patches, and documentation for a label elision
bug in the KDE Breeze style plugin introduced in February 2026.

**Bug**: commit [`aba0f922b`](https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431) in `plasma/breeze` added two lines to
`Style::subElementRect(SE_ItemViewItemText)` that unconditionally discard 8px from
the width of every list and tree view item's text rect — 4px per side — regardless
of column width. Any Qt5 or Qt6 application using the Breeze style with narrow or
medium-width columns will elide labels that would otherwise fit.

**Fix**: remove the two horizontal-narrowing lines. See [`patches/`](patches/).

**KDE Bug Report**: https://bugs.kde.org/show_bug.cgi?id=523118

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
to Breeze.

## Qt6 Scope

`breeze6.so` carries the identical code path. KeePassXC's Qt6 port (`feature/qt-feature`,
targeting 2.8.x) routes `CategoryListWidget` text rendering through `CE_ItemViewItem`
via a `QProxyStyle`, which delegates to `subElementRect(SE_ItemViewItemText)` on the
active Breeze style. The elision will reproduce on 2.8.x unless the upstream
`plasma-breeze` bug is fixed first. See [`docs/keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md).

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

### Patch 1 — recommended

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

---

## Numeric Probe

`probe/` contains standalone Qt5 and Qt6 programs that measure `SE_ItemViewItemText`
width against the full item rect at four column widths across three styles.

### Pre-built binaries

[`probe/qt5-style-elide-test`](probe/qt5-style-elide-test) and [`probe/qt-style-elide-test`](probe/qt-style-elide-test) are committed as
pre-built x86_64 binaries, compiled on Fedora 43 Workstation x86_64 (fully updated
as of 2026-07-15, Qt 5.15.18 / Qt 6.x). They can be run directly without building.

### Build

To rebuild from source:

```bash
cd probe
make          # builds both qt5-style-elide-test and qt-style-elide-test (Qt6)
```

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

`run-clean.sh` clears all Qt/KDE style and theme environment variables before
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

---

## Screenshots

`screenshots/` is organized by build:

| Directory | Build | Patch |
|---|---|---|
| [`01-stock-6.7.3-1.fc43.1/`](screenshots/01-stock-6.7.3-1.fc43.1) | Unpatched stock | none (bug present) |
| [`02-patch1-6.7.3-1.fc43.2/`](screenshots/02-patch1-6.7.3-1.fc43.2) | Patch 1 | remove both narrowing lines |
| [`03-patch2-6.7.3-1.fc43.3/`](screenshots/03-patch2-6.7.3-1.fc43.3) | Patch 2 | retain margins, remove padding |

Each directory contains `keepassxc.png`, `smplayer-1.png`, `smplayer-2.png`, and
`goldendict.png`. The `03-patch2-*` directory additionally includes
`smplayer-style-fusion.png` — the same SMPlayer binary with Style=Fusion, showing
`diff=0` and no elision, which isolates the defect to the Breeze style plugin.

---

## Documentation

| Document | Contents |
|---|---|
| [`docs/bug-analysis.md`](docs/bug-analysis.md) | Source commit, metric values, numeric proof, per-app test results, both patches, secondary `CT_ItemViewItem` finding |
| [`docs/style-plugin-loading.md`](docs/style-plugin-loading.md) | How Qt5 apps load `breeze5.so` via `QStyleFactory`; why KDE-linked apps are equally affected; per-app style selection behavior |
| [`docs/keepassxc-qt6-analysis.md`](docs/keepassxc-qt6-analysis.md) | Code path analysis of KeePassXC's Qt6 port; confirms 2.8.x will be affected unless the upstream bug is fixed first |

---

## Test Environment

- Fedora 43 (x86_64), KDE Plasma 6.7.3, Qt 5.15.18
- `plasma-breeze` builds tested: 6.7.3-1.fc43.1 (stock), 6.7.3-1.fc43.2 (Patch 1), 6.7.3-1.fc43.3 (Patch 2)
- All applications launched via [`probe/run-clean.sh`](probe/run-clean.sh) to document session context and exclude environment-variable interference
