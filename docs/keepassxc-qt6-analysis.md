# KeePassXC Qt6 Port: SE_ItemViewItemText Exposure Analysis

## Summary

KeePassXC's in-development Qt6 port (`feature/qt-feature` branch, targeting 2.8.x)
will be affected by the same `SE_ItemViewItemText` text rect narrowing bug as the
current Qt5 release. The affected widget — `CategoryListWidget` — is structurally
identical between the Qt5 and Qt6 codebases. The delegate's `paint()` method routes
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

The same class exists in both the current Qt5 codebase (2.7.x) and the Qt6 port
(`feature/qt-feature`, min Qt version 6.2.4, no Qt5 fallback). No structural
changes relevant to style-system delegation were found between branches.

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

`CategoryListWidget` itself is unchanged between branches in any way that would
affect this analysis.

---

## Impact Assessment

| Question | Answer |
|---|---|
| Does the Qt6 port's paint path use `SE_ItemViewItemText`? | Yes, via `CE_ItemViewItem` → Breeze |
| Does `IconSelectionCorrectedStyle` intercept the text rect path? | No — `drawControl` is not overridden (non-Windows) |
| Does the widget compute its own text rect, bypassing the style? | No — text painting is delegated to the style via `drawControl` |
| Would fixing `plasma-breeze` also fix KeePassXC 2.8.x? | Yes — same code path, same fix |

---

## Suggestion

The `SE_ItemViewItemText` fix in [`patches/0001-remove-horizontal-text-rect-narrowing.patch`](../patches/0001-remove-horizontal-text-rect-narrowing.patch)
applies to `kstyle/breezestyle.cpp` in `plasma-breeze`. Because both `breeze5.so` and
`breeze6.so` are compiled from that source, the patch resolves the text rect narrowing
for both Qt5 and Qt6 application code simultaneously. No changes are needed in
KeePassXC itself.

If the `plasma-breeze` patch is merged before KeePassXC 2.8.x ships, users running
KDE with a fixed Breeze will not encounter label elision in either release. If
KeePassXC 2.8.x ships first against an unpatched Breeze, the entry edit dialog
sidebar will exhibit the same elision visible today in 2.7.x.
