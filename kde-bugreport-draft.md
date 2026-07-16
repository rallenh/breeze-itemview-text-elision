# KDE bugs.kde.org — Ticket Draft

## Filing Details

- **Product**: Breeze
- **Component**: style
- **Version**: 6.7.3
- **Severity**: major
- **Platform**: Linux
- **OS**: All (affects breeze5.so and breeze6.so)
- **Keywords**: regression

---

## Summary (ticket title)

```
SE_ItemViewItemText: aba0f922b unconditionally narrows text rect by 8px, causing label elision in list/tree views
```

---

## Description

### Source commit

```
commit aba0f922b7b872caa3043d0cfe43eec374aba431
Author: Akseli Lahtinen
Date:   2026-02-25
Subject: Make viewItemPrimitive rounded like in QtQuick style (VDG issue #94)
```

https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431

This report does not propose reverting the rounded highlight geometry introduced by
that commit. The behavior described here is limited to two lines added to
`SE_ItemViewItemText` as part of the same change.

Those two lines appear in the `SE_ItemViewItemText` case in
`Style::subElementRect()` (`kstyle/breezestyle.cpp`):

```cpp
case SE_ItemViewItemText: {
    auto viewItem = qstyleoption_cast<const QStyleOptionViewItem *>(option);
    QRect rect = ParentStyleClass::subElementRect(element, option, widget);
    if (viewItem) {
        const QMargins margins = _helper->itemViewItemMargins(viewItem);
        rect.setRight(rect.right() - margins.right() - Metrics::ItemView_ItemPaddingWidth);  // ← aba0f922b
        rect.setLeft(rect.left() + margins.left() + Metrics::ItemView_ItemPaddingWidth);     // ← aba0f922b
        rect.moveTop(rect.top() + margins.top() - margins.bottom());
    }
    return rect;
}
```

At 6.7.3 metric values (`ItemView_ItemMarginLeft/Right = 2px`,
`ItemView_ItemPaddingWidth = 2px`), these two lines discard 4px from each side
of the text rect — 8px total — unconditionally, at every column width.

### Analysis

The rounded highlight rect introduced by aba0f922b applies its own inset during
painting. The two `SE_ItemViewItemText` lines above apply an additional inset to
the text rect — 8px total at 6.7.3 metric values — beyond whatever space Qt's base
class has already reserved for check indicators and decorations
(`SE_ItemViewItemCheckIndicator`, `SE_ItemViewItemDecoration`). The result is that
the text rect is 8px narrower than the column width the application measured when
sizing its labels, causing visible elision.

The `moveTop` adjustment on the line below (vertical centering within the asymmetric
rounded highlight geometry) is not part of the proposed change.

### Numeric proof

A standalone probe measures `SE_ItemViewItemText` width against the full item rect
across multiple column widths and three styles:

**Unpatched 6.7.3 (Breeze):**

```
itemWidth=150  textRect=(4,3 142x18)  textWidth=142  diff=8
itemWidth=120  textRect=(4,3 112x18)  textWidth=112  diff=8
itemWidth=100  textRect=(4,3  92x18)  textWidth= 92  diff=8
itemWidth= 80  textRect=(4,3  72x18)  textWidth= 72  diff=8
```

**Fusion (same Qt, same session, unmodified):**

```
itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
itemWidth=120  textRect=(0,3 120x18)  textWidth=120  diff=0
itemWidth=100  textRect=(0,3 100x18)  textWidth=100  diff=0
itemWidth= 80  textRect=(0,3  80x18)  textWidth= 80  diff=0
```

Breeze gives `diff=8` at every column width, including 80px where the loss is
10% of available text space. Fusion and Windows both give `diff=0`.

### Affected applications (confirmed)

All three are Qt5 applications that inherit Breeze via `QT_QPA_PLATFORMTHEME=kde`
→ `kdeglobals` → `breeze5.so`. None select Breeze explicitly in their own code.

- **KeePassXC 2.7.x** — entry edit dialog sidebar (Entry/Advanced/Icon/Auto-Type/Properties
  tabs): all five labels elide under stock 6.7.3
- **GoldenDict** — word list pane: multi-word entries ("Crab-eating macaque",
  "Crab-lipped spider orchid") truncate at default pane width
- **SMPlayer** — Preferences sidebar when Style is set to Breeze: General/Drives/
  Performance/Interface and others all elide

**Strongest isolation**: switching SMPlayer between Style=Fusion and Style=Breeze in
the same running process reproduces and eliminates the elision without restarting,
confirming the behavior is in the Breeze style plugin rather than the application.

### Qt6 scope

`kstyle/breezestyle.cpp` is compiled twice from the same source — once as `breeze5.so`
(Qt5) and once as `breeze6.so` (Qt6). Both carry the identical `SE_ItemViewItemText`
code. Qt6 applications using Breeze on a KDE session are equally affected.

### Proposed fix

Remove the two horizontal-narrowing lines. The `moveTop` adjustment is unchanged.

```diff
--- a/kstyle/breezestyle.cpp
+++ b/kstyle/breezestyle.cpp
@@ -1062,8 +1062,6 @@ QRect Style::subElementRect(SubElement element, ...)
         QRect rect = ParentStyleClass::subElementRect(element, option, widget);
         if (viewItem) {
             const QMargins margins = _helper->itemViewItemMargins(viewItem);
-            rect.setRight(rect.right() - margins.right() - Metrics::ItemView_ItemPaddingWidth);
-            rect.setLeft(rect.left() + margins.left() + Metrics::ItemView_ItemPaddingWidth);
             rect.moveTop(rect.top() + margins.top() - margins.bottom());
         }
```

After applying this patch, Breeze `SE_ItemViewItemText` gives `diff=0` at all tested
column widths — matching Fusion and Windows exactly. The rounded highlight geometry
is unaffected.

A partial alternative (retain `itemViewItemMargins`, remove only `ItemView_ItemPaddingWidth`)
was also tested. It reduces the per-side discard from 4px to 2px (diff=4 total), but
at default column widths in KeePassXC and SMPlayer this still causes visible elision.
The full removal (Patch 1 above) is the recommendation.

### Full evidence

Probe source, patches, screenshots, and analysis documents:
https://github.com/rallenh/breeze-itemview-text-elision
