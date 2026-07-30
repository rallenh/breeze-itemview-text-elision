# Proof: The Defect Is in Style::subElementRect(SE_ItemViewItemText)

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

## What this document establishes

That the label elision reported in bug 523118 is caused by
`plasma-breeze`'s implementation of one specific, named Qt API —
`QStyle::subElementRect(QStyle::SE_ItemViewItemText, ...)` — and by nothing
else: not application code, not KDE Frameworks, not a font metrics
difference, not a KDE-only widget.

The method does not rely on any application. It calls the exact function
Qt's own item-view painting code calls, directly, and compares its return
value against the two other styles shipped in the same Qt installation.

---

## 1. The claim under test

`QStyle::subElementRect(SE_ItemViewItemText, option, widget)` is the API
every standard Qt item view (`QListWidget`, `QListView`, `QTreeView`, and
any delegate built on `QStyledItemDelegate`/`QCommonStyle::drawControl`)
calls to ask the active style: *given this item's full rect, how much of it
is available for text?* The claim is that Breeze's answer to this question
is wrong — it always returns a rect 8px narrower than the item, regardless
of whether the item has room to spare.

## 2. The exact code under test

`kstyle/breezestyle.cpp`, introduced by commit
[`aba0f922b`](https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431)
(2026-02-25):

```cpp
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

The two marked lines subtract `margins.right() + ItemView_ItemPaddingWidth`
from the right edge and add the mirrored quantity to the left edge —
4px per side, 8px total, with **no width check, no conditional, no minimum
column-width guard**. Every caller of this function, for every item, at
every width, loses 8px.

## 3. Method: call the API directly, bypass every application

[`probe/qt-style-elide-test.cpp`](../probe/qt-style-elide-test.cpp) (Qt6) and
[`probe/qt5-style-elide-test.cpp`](../probe/qt5-style-elide-test.cpp) (Qt5)
both contain `probeItemViewTextRect()`, which constructs a
`QStyleOptionViewItem` by hand and calls `subElementRect` on it — no
`QListWidget`, no delegate, no application under test at all:

```cpp
static void probeItemViewTextRect(const QString &styleName, QStyle *style, int itemWidth)
{
    QStyleOptionViewItem opt;
    opt.initFrom(new QWidget());
    opt.rect = QRect(0, 0, itemWidth, 24);
    opt.text = "Keyboard and mouse shortcuts";
    opt.features = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    opt.state = QStyle::State_Enabled;
    opt.decorationSize = QSize(0, 0);

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, nullptr);
    // ... print itemWidth, textRect, diff = itemWidth - textRect.width()
}
```

This is the same function signature, called the same way, that every
application's `drawControl(CE_ItemViewItem)` call resolves to internally
(see [`sequence-diagrams.md`](sequence-diagrams.md) for the two traced
examples). The probe isolates the single function call from all the
application scaffolding around it.

`style` is obtained via `QStyleFactory::create(styleName)` — the identical
plugin-loading mechanism (`QStyleFactory` → `dlopen()`) that
`QApplication::style()` uses when a KDE session sets `widgetStyle=Breeze` in
`kdeglobals`. This is not a reimplementation or approximation of Breeze;
it is the real, installed `breeze5.so`/`breeze6.so`, loaded and called exactly
as any Qt application would load and call it.

## 4. Controlled comparison

Three styles, four widths, one call each — the only independent variable is
which style answers the call:

**Stock `plasma-breeze` 6.7.3-1.fc43.1 (unpatched):**

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

**With Patch 1 applied (`plasma-breeze` 6.7.3-1.fc43.2)** — only the two
marked lines in §2 removed, nothing else touched:

```
[breeze  ]  itemWidth=150  textRect=(0,3 150x18)  textWidth=150  diff=0
[breeze  ]  itemWidth=120  textRect=(0,3 120x18)  textWidth=120  diff=0
[breeze  ]  itemWidth=100  textRect=(0,3 100x18)  textWidth=100  diff=0
[breeze  ]  itemWidth= 80  textRect=(0,3  80x18)  textWidth= 80  diff=0
```

Removing exactly those two lines, and nothing else, brings Breeze's answer
into exact agreement with Fusion and Windows at every tested width. Full
output for all three tested builds (`.1` stock, `.2` Patch 1, `.3` Patch 2):
[`probe/results/`](../probe/results/).

## 5. Isolating cause from correlation

- **The independent variable is isolated.** `opt.text`, `opt.rect`, and every
  other field are held constant across all three styles and all four widths.
  Only `style` changes. A single-variable comparison against two unaffected
  control styles (Fusion, Windows) that share the same Qt version and the
  same `QStyleOptionViewItem` input rules out font metrics, Qt version, or
  test-harness bugs as the cause.
- **Cause and fix are the same two lines.** The diff between the "bug" build
  and the "fixed" build is exactly the two lines quoted in §2 — no other
  source change. `diff` goes from 8 to 0 at every width for that change
  alone, and returns to 8 when the two lines are restored — the cause is
  established by intervention, not by observation alone.
- **No application is involved.** The probe never constructs a
  `QListWidget`, `QStyledItemDelegate`, or any KeePassXC/SMPlayer/GoldenDict
  code. Whatever KeePassXC's or SMPlayer's delegate does with the *result*
  of this call (see [`sequence-diagrams.md`](sequence-diagrams.md)) is a
  separate, corroborating layer — it explains why the effect is visible to
  users, not what causes it.
- **The measurements and the application screenshots agree.** `diff=8` on
  stock, `diff=0` on Patch 1, `diff=4` on Patch 2 predicted — and matched —
  pass/fail behavior in KeePassXC, SMPlayer, and GoldenDict at every
  tested build (`probe/MY-TEST-NOTES.md` results table). The direct API
  measurement and the end-to-end visual symptom move together across three
  independent builds.

## 6. Hardened cross-check (probe v2)

The original probe passes a bare `QWidget` as `opt.widget` and `nullptr` as
`subElementRect`'s third argument. Breeze's `Helper::itemViewItemMargins()`
(`kstyle/breezehelper.cpp`) branches on `qobject_cast<QFrame*>(option->widget)`
and `qobject_cast<QAbstractItemView*>(option->widget)` — a bare `QWidget`
fails both casts; every real app uses a `QListWidget`, which passes both.
This raised a legitimate question: does the original probe's `diff=8`
actually match what real apps see?

[`probe/qt-style-elide-test-v2.cpp`](../probe/qt-style-elide-test-v2.cpp) and
[`probe/qt5-style-elide-test-v2.cpp`](../probe/qt5-style-elide-test-v2.cpp)
answer this directly: same measurement, but against a real, populated
`QListWidget` (not a bare `QWidget`), with the widget pointer passed
consistently to `subElementRect`, and a second variant adding a real icon
plus `State_Selected` to rule out decoration/selection-state dependence.
Measured against the same installed `plasma-breeze-6.7.3-1.fc43.1`
(unpatched stock):

```
[breeze        ] baseline         itemWidth=150  diff=8
[breeze        ] icon+selected    itemWidth=150  diff=36   (Fusion/Windows: 28 — same 8px constant, additive)
```

Identical to the original probe's `diff=8` at every width, with or without
an icon or selection. The widget-fidelity question is closed: it doesn't
change the result for this build. Probe v2 is a corroborating companion,
not a replacement — the original probe's committed binaries and numbers
remain the cited evidence in the bug report and MR discussion.

## 7. Scope statement

This document establishes the defect's location and mechanism. It does not
establish which applications are affected in practice — that depends on
whether a given application's paint path reaches this subelement at all. The
KeePassXC and SMPlayer [sequence diagrams](sequence-diagrams.md), and the
source-level traces behind them
([KeePassXC](keepassxc-qt6-analysis.md), [SMPlayer](smplayer-source-analysis.md)),
settle that question for those two applications. GoldenDict is covered by
screenshot evidence only, with no source trace.
