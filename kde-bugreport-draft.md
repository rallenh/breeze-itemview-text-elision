# KDE bugs.kde.org — Ticket Draft

> **This is a record of text prepared for bugs.kde.org, not a living document.**
> It is kept as written so that what was filed can be compared against what was
> later learned. Two figures in it have since been refined — see
> [Update 08/01/2026](#update-08012026) at the end. Correcting the ticket itself
> requires a follow-up comment on the bug; editing this file would not do that.

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

---

## Update 08/01/2026

Two figures in the text above were refined by later measurement. Neither changes
the substance of the report — the narrowing is real, unconditional with respect
to item width, absent from Fusion and Windows under identical conditions, and
removed by the proposed change.

**"8px total" is the unframed case; a framed item view loses 6px.**
`Helper::itemViewItemMargins()` reduces its margins from 2px to 1px per side once
the view's `QFrame::StyledPanel` is detected, so the inset is 3px per side. Every
view in every application named in the report is framed. The original probes
measured the unframed case because `QStyleOption::initFrom()` does not populate
`QStyleOptionViewItem::widget`.

**A live view does show 8px less text than Fusion**, but 2px of that is Breeze's
`PM_DefaultFrameWidth` returning 2 where Fusion returns 1 for a scroll area in a
spaced layout — a frame-thickness difference, not text layout, and not addressed
by any proposed patch.

Additionally, `QCommonStylePrivate::viewItemDrawText()` removes
`PM_FocusFrameHMargin + 1` from each side again after `subElementRect()` returns.
On Qt 5.15 this compounds with a clamp in `viewItemLayout()` that Qt 6 removed,
leaving every label short at *any* view width rather than only in narrow ones.

**Action still required:** the ticket carries the 8px figure. A follow-up comment
stating the 6px/2px decomposition should be posted so the bug record matches the
evidence. That has not been done by editing this file.

---

## Update 08/02/2026 — the proposed change is Patch 4, not Patch 1

The "Proposed fix" section above names Patch 1, and its closing line — *"The full
removal (Patch 1 above) is the recommendation"* — no longer describes what is
being proposed upstream.

Patches 1, 2 and 3 were hypothesis tests. Each was built and measured to
establish a fact about the mechanism, and together they produced the test
harness and the understanding that Patch 4 rests on. They are published as the
experimental record, not as alternatives on offer.

**The proposed change is
[`patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch`](patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch).**
It takes the horizontal inset only from space the text does not need: where the
item is wide enough to spare it, the clearance `aba0f922b` added is applied
exactly as before; where it is not, the text keeps the width it needs. That
preserves the intent of the original commit instead of reverting it, which
Patch 1 does not.

A follow-up ticket comment should therefore carry both corrections together —
the 6px/2px decomposition *and* the change of proposed fix — rather than the
figure alone.
