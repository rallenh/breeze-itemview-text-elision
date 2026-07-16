# Bug Analysis: SE_ItemViewItemText Elision in KDE Breeze Style

## Summary

A commit to `plasma/breeze` in February 2026 introduced an unconditional
8px narrowing of the text rect in `QStyle::subElementRect(SE_ItemViewItemText)`.
This discards 4 pixels from each side of every list/tree view item's text area
regardless of column width, causing visible label elision in any Qt5 or Qt6
application using the Breeze style with narrow or medium-width columns.

Fusion and Windows styles are not affected. The defect is isolated to
`kstyle/breezestyle.cpp` in `plasma-breeze`.

---

## Source Commit

```
commit aba0f922b7b872caa3043d0cfe43eec374aba431
Author: Akseli Lahtinen
Date:   2026-02-25
Subject: Make viewItemPrimitive rounded like in QtQuick style (VDG issue #94)
```

https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431

The commit added two lines to the `SE_ItemViewItemText` case in
`Style::subElementRect()`:

```cpp
// kstyle/breezestyle.cpp — introduced by aba0f922b
case SE_ItemViewItemText: {
    auto viewItem = qstyleoption_cast<const QStyleOptionViewItem *>(option);
    QRect rect = ParentStyleClass::subElementRect(element, option, widget);
    if (viewItem) {
        const QMargins margins = _helper->itemViewItemMargins(viewItem);
        rect.setRight(rect.right() - margins.right() - Metrics::ItemView_ItemPaddingWidth);  // ← added
        rect.setLeft(rect.left() + margins.left() + Metrics::ItemView_ItemPaddingWidth);     // ← added
        rect.moveTop(rect.top() + margins.top() - margins.bottom());
    }
    return rect;
}
```

At `plasma-breeze` 6.7.3, the relevant metric values are:

| Metric                        | Value |
|-------------------------------|-------|
| `ItemView_ItemMarginLeft`     | 2 px  |
| `ItemView_ItemMarginRight`    | 2 px  |
| `ItemView_ItemPaddingWidth`   | 2 px  |
| **Total discarded per side**  | **4 px** |
| **Total discarded both sides**| **8 px** |

The same commit introduced a rounded highlight rectangle for list/tree view items.
The two added lines narrow the text rect by the same margin values used in the
highlight geometry. The rounded highlight is painted using its own inset; the
text rect narrowing results in every list/tree item losing 8px of text width
unconditionally, regardless of whether the column is wide enough to absorb
the loss.

---

## Numeric Proof

A standalone Qt5 probe ([`probe/qt5-style-elide-test.cpp`](../probe/qt5-style-elide-test.cpp)) measures
`SE_ItemViewItemText` width against the full item rect width across three
styles at four column widths. *(Pre-built x86_64 binary committed for Fedora 43, Qt 5.15.18, fully updated 2026-07-15. See [`probe/`](../probe/) to rebuild from source.)*

**Stock `plasma-breeze` 6.7.3 (unpatched):**

```
[breeze  ]  itemWidth=150  textRect=(4,3 142x18)  textWidth=142  diff=8
[breeze  ]  itemWidth=120  textRect=(4,3 112x18)  textWidth=112  diff=8
[breeze  ]  itemWidth=100  textRect=(4,3  92x18)  textWidth= 92  diff=8
[breeze  ]  itemWidth= 80  textRect=(4,3  72x18)  textWidth= 72  diff=8

[Fusion  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
[Fusion  ]  itemWidth=120  textRect=(0,3 120x18)  textWidth=120  diff=0
[Fusion  ]  itemWidth=100  textRect=(0,3 100x18)  textWidth=100  diff=0
[Fusion  ]  itemWidth= 80  textRect=(0,3  80x18)  textWidth= 80  diff=0

[Windows ]  diff=0 at all widths (identical to Fusion)
```

`diff=8` at every column width means Breeze always discards 8px, even in
80px columns where the loss is 10% of available text space.

**With Patch 1 applied (`plasma-breeze` 6.7.3-1.fc43.2):**

```
[breeze  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
[breeze  ]  itemWidth=120  textRect=(0,3 120x18)  textWidth=120  diff=0
[breeze  ]  itemWidth=100  textRect=(0,3 100x18)  textWidth=100  diff=0
[breeze  ]  itemWidth= 80  textRect=(0,3  80x18)  textWidth= 80  diff=0
```

Breeze now matches Fusion and Windows exactly.

Full probe output files are in [`probe/results/`](../probe/results/).

---

## Affected Applications (Confirmed)

All three applications tested are Qt5, using the Breeze style via the KDE
platform theme (`QT_QPA_PLATFORMTHEME=kde` → `kdeglobals` → `breeze5.so`).
None of these applications select Breeze explicitly in their source — they
inherit it from the KDE session.

### KeePassXC 2.7.x

Entry edit dialog sidebar: Entry, Advanced, Icon, Auto-Type, Properties tabs
are all visibly elided with the unpatched Breeze.

| Build          | Sidebar labels           |
|----------------|--------------------------|
| Stock `.1`     | En..., Advan..., I..., Auto-..., Prope... |
| Patch 1 `.2`   | Entry, Advanced, Icon, Auto-Type, Properties ✓ |
| Patch 2 `.3`   | En..., Advanc..., Ic..., Auto-..., Properti... |

Screenshots: [`screenshots/01-stock-6.7.3-1.fc43.1/keepassxc.png`](../screenshots/01-stock-6.7.3-1.fc43.1/keepassxc.png),
[`screenshots/02-patch1-6.7.3-1.fc43.2/keepassxc.png`](../screenshots/02-patch1-6.7.3-1.fc43.2/keepassxc.png)

### SMPlayer 25.6.0

Preferences sidebar (Style manually set to Breeze — SMPlayer defaults to Fusion):

| Build          | Sidebar labels                                      |
|----------------|-----------------------------------------------------|
| Stock `.1`     | Gen..., Dri..., Performa..., Interf..., etc.       |
| Patch 1 `.2`   | General, Drives, Performance, Interface, etc. ✓     |
| Patch 2 `.3`   | Gene..., Driv..., Performan..., Interfa..., etc.   |

"Keyboard and mouse" clips in all builds including Patch 1 — this is the
physical column width limit, not Breeze pixel discard, and is correct behavior.

The File Properties → Video codec list shows the same elision on all codec
description strings, plus a horizontal scrollbar (related to `CT_ItemViewItem`
size hint inflation — see note below).

**Fusion style control**: the same SMPlayer binary with Style=Fusion shows
zero elision. Switching styles within the running app is the clearest
isolated demonstration that the defect is in Breeze, not SMPlayer.

Screenshots: [`screenshots/01-stock-6.7.3-1.fc43.1/smplayer-1.png`](../screenshots/01-stock-6.7.3-1.fc43.1/smplayer-1.png),
[`screenshots/02-patch1-6.7.3-1.fc43.2/smplayer-1.png`](../screenshots/02-patch1-6.7.3-1.fc43.2/smplayer-1.png),
[`screenshots/03-patch2-6.7.3-1.fc43.3/smplayer-style-fusion.png`](../screenshots/03-patch2-6.7.3-1.fc43.3/smplayer-style-fusion.png)

### GoldenDict

Word list pane (inherits Breeze from KDE session automatically):

| Build          | Word list                               |
|----------------|-----------------------------------------|
| Stock `.1`     | Crab-eating macaq..., Crab-lipped spider orc... |
| Patch 1 `.2`   | Crab-eating macaques, Crab-lipped spider orchid ✓ |
| Patch 2 `.3`   | Crab-eating macaqu..., Crab-lipped spider orc... |

Screenshots: [`screenshots/01-stock-6.7.3-1.fc43.1/goldendict.png`](../screenshots/01-stock-6.7.3-1.fc43.1/goldendict.png),
[`screenshots/02-patch1-6.7.3-1.fc43.2/goldendict.png`](../screenshots/02-patch1-6.7.3-1.fc43.2/goldendict.png)

---

## Qt6 Scope

`plasma-breeze` compiles `kstyle/breezestyle.cpp` twice from the same source:
once for Qt5 (`breeze5.so`) and once for Qt6 (`breeze6.so`). Both share the
identical `SE_ItemViewItemText` code path. Qt6 applications using Breeze are
affected by the same bug.

KeePassXC 2.8.x is in active development as a Qt6 port. Unless the upstream
bug is fixed before release, 2.8.x will carry the same elision on KDE sessions.
See [`docs/keepassxc-qt6-analysis.md`](keepassxc-qt6-analysis.md) for details.

---

## Proposed Fix

### Patch 1 (recommended)

Remove the two horizontal-narrowing lines entirely. The text rect receives the
full column width, matching Fusion and Windows behavior. The rounded highlight
is painted with its own inset via a separate code path and is unaffected.

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

Full patch: [`patches/0001-remove-horizontal-text-rect-narrowing.patch`](../patches/0001-remove-horizontal-text-rect-narrowing.patch)

### Patch 2 (considered, insufficient)

Retain `itemViewItemMargins` (2px/side) but remove `ItemView_ItemPaddingWidth`
(2px/side). Reduces total discard from 8px to 4px. Tested and found to still
cause visible elision in KeePassXC and SMPlayer at default column widths.
Presented for discussion only.

Full patch: [`patches/0002-partial-keep-margins-remove-itempadding.patch`](../patches/0002-partial-keep-margins-remove-itempadding.patch)

---

## Note: CT_ItemViewItem Size Hint Inflation

A secondary finding from the `CT_ItemViewItem` probe: Breeze's item size hint
is 8px wider than Fusion at every content width (e.g., `hintSize=202` vs
Fusion's `194` for a 150px content width). This causes a horizontal scrollbar
to appear in the SMPlayer codec list even when items are elided — a paradox
that demonstrates both over-wide size hints and under-wide text rects
coexisting in the same item.

This is likely a separate issue from `SE_ItemViewItemText` narrowing and is
not addressed by either patch here.

---

## Test Environment

- Fedora 43 (x86_64), KDE Plasma 6.7.3
- Qt 5.15.18
- `plasma-breeze` versions tested: 6.7.3-1.fc43.1 (stock), 6.7.3-1.fc43.2 (Patch 1), 6.7.3-1.fc43.3 (Patch 2)
- All applications launched via [`probe/run-clean.sh`](../probe/run-clean.sh) to clear style-override
  environment variables and document session context
