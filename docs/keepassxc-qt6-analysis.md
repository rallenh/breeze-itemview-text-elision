# KeePassXC Qt6 Port: SE_ItemViewItemText Exposure Analysis

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

> ## Read this first — the forward-looking conclusion is withdrawn
>
> **Withdrawn 08/02/2026.** This document predicts, from source reading, that
> KeePassXC's Qt6 port will show the same label truncation as 2.7.x once it
> ships. **That prediction was tested and it did not hold.** The port was built
> from branch `feature/qt-feature` at commit `fabfba2c` and run against
> unpatched `plasma-breeze-6.7.3-1.fc43.1`: every sidebar label renders in full.
>
> The call-path analysis below is unaffected and was confirmed — the Qt6 port
> does reach `subElementRect(SE_ItemViewItemText)` through Breeze, exactly as
> traced here. What did not follow is the *visible outcome*, because the
> prediction assumed Qt 6 lays item text out the way Qt 5.15 does. It does not:
> Qt 5.15 clamps the item text rect to the string's natural width, leaving no
> slack for the inset to come out of, and Qt 6 removed that clamp. Measured in
> the same 96px sidebar layout on unpatched Breeze:
>
> ```
> qt5  Breeze  8/8 labels truncated     qt5  Fusion  6/8
> qt6  Breeze  6/8 labels truncated     qt6  Fusion  6/8   <- no Breeze-specific difference
> ```
>
> Sharing a call path is necessary for exposure, not sufficient for a visible
> symptom. That is the correction, and it is the reason this document's
> "Impact Assessment" and "Suggestion" sections carry inline corrections below.
>
> That branch is under active development, so this result describes the commit
> named above and nothing else. Screenshots and the full statement of the
> retraction are in the repository overview under
> [Qt6 Scope](../README.md#qt6-scope).

> **Partially superseded — see [Update 08/01/2026](#update-08012026) at the end
> of this document.** The 8px figure below describes an *unframed* item view.
> Framed views — which is every view in every application examined here — lose
> **6px**. The finding is otherwise unchanged, and the original text is kept as
> written.

> **Verification note**: the call chain documented below was verified against
> `CategoryListWidget.cpp` as shipped in the pristine
> `keepassxc-2.7.12-src.tar.xz` tarball — the same source compiled into
> `keepassxc-2.7.12-1.fc43.x86_64`, the unpatched build that produced this
> repo's screenshots. It matches exactly: `opt.icon` is cleared and redrawn
> manually, while `opt.text` is left untouched and painted entirely inside
> `drawControl(CE_ItemViewItem)`.
>
> **If you re-derive this analysis, read from unmodified upstream source.**
> An app-side change to KeePassXC's own text painting — blanking `opt.text`
> and drawing the label directly with `QPainter::drawText()` — was tried
> early in this investigation and decided against, on the grounds that the
> regression is in the style and the fix belongs there. Nothing in this repository
> depends on that attempt, and the screenshots here were taken against the
> unpatched release build. The point of the caution is only that a modified
> `CategoryListWidget.cpp` will not reproduce the call chain described below.

## Summary

KeePassXC's in-development Qt6 port (`feature/qt-feature` branch, targeting 2.8.x)
will be affected by the same `SE_ItemViewItemText` text rect narrowing bug as the
current Qt5 release. The affected widget — `CategoryListWidget` — is **byte-identical**
between the mainline and Qt6-port branches, which anyone can confirm in one command:

```
$ git diff origin/develop origin/feature/qt-feature -- src/gui/CategoryListWidget.cpp
$            # no output — the file is unchanged between the two branches
```

The delegate's `paint()` method routes
text rendering through the style system, landing on `breeze6.so`'s
`subElementRect(SE_ItemViewItemText)` on a KDE session. Unless the bug in
`plasma-breeze` is fixed before the 2.8.x release, users will see the same elision
in the entry edit dialog sidebar.

---

## The Widget Under Examination

`CategoryListWidget` (in `src/gui/CategoryListWidget.cpp`) is the sidebar
navigation widget used in multi-page dialogs throughout KeePassXC — most visibly in
the entry edit dialog, where it renders the Entry / Advanced / Icon / Auto-Type /
Properties tabs.

It is implemented as a `QWidget` housing a `QListWidget` with a custom item
delegate, `CategoryListWidgetDelegate` (defined inline in the same file), which
inherits `QStyledItemDelegate`.

The same class exists in both the mainline codebase and the Qt6 port
(`feature/qt-feature`, min Qt version 6.2.4, no Qt5 fallback). The file is not
merely similar between the two branches — it is unchanged, per the `git diff`
above. Whatever differences the Qt6 port introduces elsewhere, this widget's
delegation to the style system is not among them.

---

## Paint Path in CategoryListWidgetDelegate

The delegate overrides `QStyledItemDelegate::paint()`:

```cpp
void CategoryListWidgetDelegate::paint(QPainter* painter,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();

    QIcon icon = opt.icon;
    QSize iconSize = opt.icon.actualSize(QSize(ICON_SIZE, ICON_SIZE));
    opt.icon = QIcon();                               // ← icon removed from option
    opt.decorationPosition = QStyleOptionViewItem::Top;

    QScopedPointer<QStyle> style(new IconSelectionCorrectedStyle());
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);  // ← text goes here

    // ... icon painted manually via QPainter::drawPixmap(), bypassing QStyle
    painter->drawPixmap(left, opt.rect.top() + paddingTop, icon.pixmap(iconSize, mode));
    painter->restore();
}
```

Two things to note:

1. **The icon is drawn manually.** `opt.icon` is cleared before `drawControl`,
   and the icon is re-applied via `QPainter::drawPixmap()` at a manually computed
   position. This half of the rendering is entirely outside the style system and is
   not affected by the bug.

2. **The text is drawn through the style system.** `style->drawControl(CE_ItemViewItem, ...)`
   is the call that paints the category label. `CE_ItemViewItem` is responsible for
   rendering the item's text, and the Qt implementation computes the text rect
   using `subElementRect(SE_ItemViewItemText)` — the exact code path where the bug lives.

---

## How IconSelectionCorrectedStyle Routes to Breeze

`IconSelectionCorrectedStyle` is a `QProxyStyle` declared locally in
`CategoryListWidget.cpp`:

```cpp
class IconSelectionCorrectedStyle : public QProxyStyle
{
    void drawPrimitive(PrimitiveElement element, ...) const override { /* custom highlight */ }
    // drawControl: NOT overridden on non-Windows
};
```

`QProxyStyle` with no base-style argument uses `QApplication::style()` as its
delegate. On a KDE session, `QApplication::style()` is the Breeze plugin loaded
via `QStyleFactory`. For Qt5 this is `breeze5.so`; for Qt6 this will be `breeze6.so`.

The call chain for `CE_ItemViewItem` on non-Windows:

```
IconSelectionCorrectedStyle::drawControl(CE_ItemViewItem)
  → (not overridden)
  → QProxyStyle::drawControl(CE_ItemViewItem)
  → QApplication::style()->drawControl(CE_ItemViewItem)   ← Breeze
  → QCommonStyle::drawControl(CE_ItemViewItem)             ← via Breeze's ParentStyleClass
  → Style::subElementRect(SE_ItemViewItemText)             ← THE BUG IS HERE
```

Breeze's `subElementRect(SE_ItemViewItemText)` discards 4px from each side of the
text rect (2px from `itemViewItemMargins` + 2px from `ItemView_ItemPaddingWidth`),
unconditionally.

---

## Width Budget and Why Elision Follows

`CategoryListWidget` sizes itself to fit its labels. `CategoryListWidgetDelegate::minWidth()`
measures each label with `QFontMetrics::boundingRect()` and adds 10px padding:

```cpp
int CategoryListWidgetDelegate::minWidth() const
{
    int maxWidth = 0;
    for (int i = 0; i < c; ++i) {
        QFontMetrics fm(m_listWidget->font());
        QRect fontRect = fm.boundingRect(QRect(0,0,0,0),
                                          Qt::TextWordWrap | Qt::ElideNone,
                                          m_listWidget->item(i)->text());
        if (fontRect.width() > maxWidth)
            maxWidth = fontRect.width();
    }
    return maxWidth + 10;  // ← 10px padding, not 18px
}
```

The widget is therefore sized to give labels `maxWidth + 10` pixels, with no
allowance for the 8px Breeze text rect narrowing. When Breeze discards 8px from
that budget, labels that fill close to the column width will elide.

This is the same mechanism observed in the Qt5 2.7.x build under `plasma-breeze`
6.7.3 (stock): the "Properties" and "Advanced" tabs elide because their label width
is within 8px of the column's text rect after the narrowing is applied.

---

## Verified Scope in Qt6 Port

The `feature/qt-feature` branch `CMakeLists.txt` confirms:

```cmake
find_package(Qt6 6.2.4 REQUIRED COMPONENTS ...)
```

No Qt5 fallback. The build produces Qt6 binaries exclusively. These binaries will
load `breeze6.so` (not `breeze5.so`) on a KDE session. `breeze6.so` is compiled
from the same `kstyle/breezestyle.cpp` as `breeze5.so` and carries the identical
`SE_ItemViewItemText` narrowing.

`CategoryListWidget` itself is unchanged between branches — literally, not just
in effect (see the `git diff` in the Summary).

---

## Impact Assessment

| Question | Answer |
|---|---|
| Does the Qt6 port's paint path use `SE_ItemViewItemText`? | Yes, via `CE_ItemViewItem` → Breeze |
| Does `IconSelectionCorrectedStyle` intercept the text rect path? | No — `drawControl` is not overridden (non-Windows) |
| Does the widget compute its own text rect, bypassing the style? | No — text painting is delegated to the style via `drawControl` |
| Would fixing `plasma-breeze` also fix KeePassXC 2.8.x? | Yes — same code path, same fix |

> **Correction, 08/02/2026.** The first three rows were confirmed by building
> the port; they describe the call path and they hold. The fourth row is
> correct only in the narrow sense that a Breeze fix covers this code path —
> **it should not be read as saying 2.8.x currently truncates and needs
> fixing.** As built at `fabfba2c`, it does not. See the withdrawal at the top
> of this document.

---

## Suggestion

The `SE_ItemViewItemText` fix in [`patches/0001-remove-horizontal-text-rect-narrowing.patch`](../patches/0001-remove-horizontal-text-rect-narrowing.patch)
applies to `kstyle/breezestyle.cpp` in `plasma-breeze`. Because both `breeze5.so` and
`breeze6.so` are compiled from that source, the patch resolves the text rect narrowing
for both Qt5 and Qt6 application code simultaneously. No changes are needed in
KeePassXC itself.

> **Note, 08/02/2026 — the patch named above is not the proposed change.**
> Patch 1 was a hypothesis test. The proposed change is
> [`patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch`](../patches/0004-width-guarded-itemviewitem-text-inset-no-exclusions.patch),
> which resolves the same truncation while keeping the clearance `aba0f922b`
> was added to provide. The point that survives unchanged is the one about
> shared compilation: both `breeze5.so` and `breeze6.so` are built from the
> same source, so any fix there covers both.

If the `plasma-breeze` patch is merged before KeePassXC 2.8.x ships, users running
KDE with a fixed Breeze will not encounter label elision in either release. If
KeePassXC 2.8.x ships first against an unpatched Breeze, the entry edit dialog
sidebar will exhibit the same elision visible today in 2.7.x.

> **Correction, 08/02/2026 — the final sentence above is withdrawn.** The
> paragraph is retained so the correction is visible. The Qt6 port was built
> (`feature/qt-feature`, `fabfba2c`) and its sidebar renders in full on
> unpatched Breeze; on Qt 6 the two styles agree. The reasoning was a
> source-level inference that assumed Qt 6 lays item text out as Qt 5.15 does,
> and Qt 6 removed the natural-width clamp that makes the inset bite at any
> width. See the withdrawal at the top of this document.
>
> The rest of the paragraph stands: 2.7.x is affected today, and a Breeze fix
> resolves it without any change to KeePassXC.
>
> **This document should not be cited as evidence that KeePassXC 2.8.x is
> affected.** The application demonstrating the regression in this repository
> is SMPlayer, whose Preferences sidebar has no custom item delegate at all and
> whose style dropdown makes the style the only variable — see
> [`smplayer-source-analysis.md`](smplayer-source-analysis.md).


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

**Scope note.** This document analyses KeePassXC's Qt6 branch at source level and
concludes the exposure is structurally identical. That remains a source-level
inference: the Qt6 port was not built or run for these measurements, and no
claim about observed behaviour on 2.8.x is made here.
