# Isolation Experiment: Style::subElementRect(SE_ItemViewItemText)

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

> **Partially superseded — see [Update 08/01/2026](#update-08012026) at the end
> of this document.** The 8px figure below describes an *unframed* item view.
> Framed views — which is every view in every application examined here — lose
> **6px**. The finding is otherwise unchanged, and the original text is kept as
> written.

> **Title and scope narrowed 08/02/2026.** This document was previously titled
> "Proof: The Defect Is in `Style::subElementRect(SE_ItemViewItemText)`," and
> the section below claimed the elision was caused by that function "and by
> nothing else." The measurements are unchanged and still support what they
> were run to show, but that phrasing claimed more than a single-variable
> comparison can establish, and later work in this repository identified
> further contributing mechanisms. The superseded wording is preserved
> verbatim in the [Update 08/02/2026](#update-08022026) at the end.

## What this document establishes

*Rewritten 08/02/2026 — see the note above.*

That the **Breeze-specific** difference in item-view text width is produced by
`plasma-breeze`'s implementation of one specific, named Qt API —
`QStyle::subElementRect(QStyle::SE_ItemViewItemText, ...)`. Holding everything
else constant — same process, same Qt build, same font, same widget, same
option, same item rect — and changing only the active `QStyle` changes the
rectangle that comes back. That rules out the alternative explanations which
were live when this was written: application code, KDE Frameworks, a
font-metrics difference, and a KDE-only widget.

**What it does not establish.** It does not establish that this subelement is
the only mechanism contributing to the visible symptom. Three others were
identified by later measurement and are documented in the
[repository overview](../README.md#update-08012026):

- Qt 5.15's `viewItemLayout()` clamp, which is what turns a fixed inset into
  truncation at *any* view width rather than only at narrow ones.
- `QCommonStylePrivate::viewItemDrawText()`, which removes
  `PM_FocusFrameHMargin + 1` from each side again after this function has
  returned.
- A 2px viewport difference from `PM_DefaultFrameWidth`. This one is also
  Breeze, but it is not this subelement.

The first two are shared substrate: Fusion and Windows are laid out and painted
through the same Qt code and do not truncate. They explain the *magnitude* of
the effect rather than its origin. The third is why a live view shows an 8px
difference against Fusion while only 6px of it belongs here — quoting the total
as though it were the inset overstates what `SE_ItemViewItemText` does.

The method does not rely on any application. It calls the exact function
Qt's own item-view painting code calls, directly, and compares its return
value against the two other styles shipped in the same Qt installation.

---

## 1. The claim under test

`QStyle::subElementRect(SE_ItemViewItemText, option, widget)` is the API
every standard Qt item view (`QListWidget`, `QListView`, `QTreeView`, and
any delegate built on `QStyledItemDelegate`/`QCommonStyle::drawControl`)
calls to ask the active style: *given this item's full rect, how much of it
is available for text?* The claim under test is that Breeze's answer differs
from the other styles' — it returns a rect 8px narrower than the item,
unconditionally, whether or not the item has room to spare.

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

> **Correction, 08/02/2026 — "the widget-fidelity question is closed" was
> premature. v2 did not close it;
> [probe v3](#6a-widget-fidelity-actually-closed-probe-v3) did.** The paragraph
> above is retained so the correction is visible.
>
> v2 does pass a real, populated `QListWidget` as `subElementRect()`'s third
> argument, and that part of the claim holds. But it builds its option with
> `opt.initFrom(listWidget)`, and **`QStyleOption::initFrom()` does not assign
> `QStyleOptionViewItem::widget`** — that member belongs to the derived option
> class and must be set directly. v2 leaves it null.
>
> This matters because `Helper::itemViewItemMargins()` reads `option->widget`,
> not the widget passed alongside it, to detect the view's
> `QFrame::StyledPanel`. With the member null the frame check cannot fire, so
> v2 measures an **unframed** view — which is exactly why it reports `diff=8`
> and a real framed application view loses 6px. The two numbers were never in
> conflict; they are different cases, and v2's own header comment described its
> option as more faithful than it was.
>
> v2's measurements stand as published. What changes is what they are evidence
> *of*: the unframed case, correctly measured. Neither v1 nor v2 has been
> modified.

## 6a. Widget fidelity, actually closed (probe v3)

*Added 08/02/2026. Section numbering is left alone so existing links keep
working.*

[`probe/qt5-style-elide-test-v3.cpp`](../probe/qt5-style-elide-test-v3.cpp) and
[`probe/qt-style-elide-test-v3.cpp`](../probe/qt-style-elide-test-v3.cpp) assign
the member directly:

```cpp
opt.initFrom(listWidget);   // does NOT set QStyleOptionViewItem::widget
if (setOptWidget) {
    opt.widget = listWidget;
}
```

The `setOptWidget` parameter exists so both cases can be measured in one run
rather than argued about. Against the same unpatched
`plasma-breeze-6.7.3-1.fc43.1`:

```
[breeze ] baseline/noWidget itemWidth=150  textWidth=142  drawn=136  slack= -52  elides=YES  diff=8
[breeze ] baseline          itemWidth=150  textWidth=144  drawn=138  slack= -50  elides=YES  diff=6
[Fusion ] baseline/noWidget itemWidth=150  textWidth=150  drawn=144  slack= -44  elides=YES  diff=0
[Fusion ] baseline          itemWidth=150  textWidth=150  drawn=144  slack= -44  elides=YES  diff=0
```

Three things follow, and they are the reason v3 rather than v2 closes this:

1. **Setting the member changes the Breeze result and only the Breeze result.**
   `diff` moves 8 → 6 for Breeze; Fusion and Windows are unmoved, because
   neither reads `option->widget` to adjust item margins.
2. **The framed number, 6px, is what a real application view receives.** Every
   item view in every application examined here is inside a
   `QFrame::StyledPanel`.
3. **v1 and v2 are reproduced, not contradicted.** Their `diff=8` is the
   `*/noWidget` row above.

Full output for every build is in [`probe/results/`](../probe/results/).

## 7. Scope statement

This document establishes where the Breeze-specific contribution originates and
by what mechanism. It does not establish which applications are affected in
practice — that depends on whether a given application's paint path reaches this
subelement at all. The KeePassXC and SMPlayer
[sequence diagrams](sequence-diagrams.md), and the source-level traces behind
them ([KeePassXC](keepassxc-qt6-analysis.md),
[SMPlayer](smplayer-source-analysis.md)), settle that question for those two
applications. GoldenDict is covered by screenshot evidence only, with no source
trace.

It also does not establish that any particular change to this function is
correct for every item view Breeze styles. The coverage boundary around the
patches is stated in the repository overview under
[What Was and Was Not Tested](../README.md#what-was-and-was-not-tested).


---

## Update 08/01/2026

A second round of measurement, described in [`test-plan.md`](test-plan.md),
refined the size of the effect and identified two mechanisms this document
predates. Nothing here is retracted; the numbers are decomposed.

**The narrowing is 6px in a framed view, not 8px.**
`Helper::itemViewItemMargins()` reduces its left and right margins from 2px to
1px once it detects the view's `QFrame::StyledPanel`, making the inset
`1 + ItemView_ItemPaddingWidth` = 3px per side. The original probes measured the
unframed case because `QStyleOption::initFrom()` does not populate
`QStyleOptionViewItem::widget`, so Breeze's frame check never fired. Probe v3
reports both cases side by side, reproducing the 8px figures below as its
`*/noWidget` rows.

**A live view shows 8px less text than Fusion, but only 6px of it is this code
path.** The remaining 2px is Breeze's `PM_DefaultFrameWidth` returning
`Metrics::Frame_FrameWidth` (2) for a scroll area in a spaced multi-item layout
where Fusion returns 1 — a deliberate difference in frame thickness, unrelated
to text layout.

**Qt removes the padding a second time.**
`QCommonStylePrivate::viewItemDrawText()` removes `PM_FocusFrameHMargin + 1` from
each side of whatever `subElementRect()` returned, immediately before eliding.
The width the text is laid out in is `textWidth - 2 * textMargin`. On Qt 5.15,
where `viewItemLayout()` clamps the rect to the string's natural width whenever
`showDecorationSelected` is false — which is what Breeze's style hint reports —
this leaves every label short by exactly `2 * textMargin` at *any* view width.

See the [repository overview](../README.md) for the current suggested patch.

---

## Update 08/02/2026

Two changes to this document, neither of which alters a measurement.

**1. The title and the opening claim were narrowed.** This document was titled
"Proof: The Defect Is in `Style::subElementRect(SE_ItemViewItemText)`," and
"What this document establishes" read, verbatim:

> That the label elision reported in bug 523118 is caused by
> `plasma-breeze`'s implementation of one specific, named Qt API —
> `QStyle::subElementRect(QStyle::SE_ItemViewItemText, ...)` — and by nothing
> else: not application code, not KDE Frameworks, not a font metrics
> difference, not a KDE-only widget.

Against the four alternatives it names, that holds and the measurements show it.
"By nothing else" reaches further than that, though — it reads as excluding any
other contributing mechanism, and by the time of the
[Update 08/01/2026](#update-08012026) this repository had itself identified
three: Qt 5.15's layout clamp, the second `textMargin` removal in
`viewItemDrawText()`, and a 2px `PM_DefaultFrameWidth` difference that is
Breeze's but is not this subelement. The last of those is the sharper problem,
because it means the 8px total quoted here was never entirely attributable to
the code under test.

What the experiment does is hold every variable but the active `QStyle` fixed
and observe the returned rectangle change. That is a controlled isolation of the
Breeze-specific contribution, which is what the document is retitled to claim.

**2. Section 6's closing sentence was corrected in place.** "The
widget-fidelity question is closed" was premature — see the correction under
[section 6](#6-hardened-cross-check-probe-v2) and the measurement in
[section 6a](#6a-widget-fidelity-actually-closed-probe-v3). v2 leaves
`QStyleOptionViewItem::widget` null and therefore measures an unframed view;
v3 sets it and reports both.

No probe, patch, or result file was modified. `probe/qt5-style-elide-test-v2.cpp`
and its Qt6 counterpart still carry the header comment that overstated their
option's fidelity; they are published artifacts with published results and are
left as they are. This document is where the correction lives.
