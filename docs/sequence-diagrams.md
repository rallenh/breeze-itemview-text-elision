# Sequence Diagrams: Reaching Style::subElementRect(SE_ItemViewItemText)

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

Both diagrams below trace a real paint call, from the item view's paint
event down to the single function where the bug lives
(`Style::subElementRect(SE_ItemViewItemText)` in `kstyle/breezestyle.cpp`,
introduced by commit [`aba0f922b`](https://invent.kde.org/plasma/breeze/-/commit/aba0f922b7b872caa3043d0cfe43eec374aba431)).
Source basis: [`keepassxc-qt6-analysis.md`](keepassxc-qt6-analysis.md) for
KeePassXC (pristine upstream `keepassxc-2.7.12-1.fc43`, verified against
the actual installed/screenshotted build) and
[`smplayer-source-analysis.md`](smplayer-source-analysis.md) for SMPlayer
(`smplayer-25.6.0-2.fc43`).

Both chains terminate at the identical Qt/Breeze call — the divergence is
entirely in what each application puts *in front of* it, which is the point:
neither app decides how wide the text rect is. Breeze does.

---

## KeePassXC — CategoryListWidgetDelegate (Entry Edit dialog sidebar)

```mermaid
sequenceDiagram
    participant QLW as QListWidget<br/>(categoryList)
    participant Del as CategoryListWidgetDelegate<br/>(QStyledItemDelegate)
    participant Proxy as IconSelectionCorrectedStyle<br/>(QProxyStyle)
    participant Breeze as Style<br/>(breeze5.so / breeze6.so)
    participant Common as QCommonStyle<br/>(Breeze's ParentStyleClass)

    QLW->>Del: paint(painter, option, index)
    Del->>Del: initStyleOption(&opt, index)<br/>opt.text = "Properties" (kept)
    Del->>Del: opt.icon = QIcon() (cleared —<br/>drawn manually later)
    Del->>Proxy: new IconSelectionCorrectedStyle()<br/>drawControl(CE_ItemViewItem, &opt, painter)
    Proxy->>Proxy: drawPrimitive(PE_PanelItemViewItem)<br/>— custom selection highlight, handled locally
    Proxy->>Proxy: drawControl(CE_ItemViewItem) not overridden
    Proxy->>Breeze: QProxyStyle::drawControl() forwards to<br/>QApplication::style() → Breeze
    Breeze->>Common: drawControl(CE_ItemViewItem)<br/>via ParentStyleClass
    Common->>Breeze: subElementRect(SE_ItemViewItemText, opt)
    Note over Breeze: BUG: rect.setRight(-2px margin -2px padding)<br/>rect.setLeft(+2px margin +2px padding)<br/>→ 8px stolen from text width, unconditionally
    Breeze-->>Common: narrowed textRect
    Common->>Common: drawItemText(painter, textRect,<br/>..., opt.text)
    Note over Common: Qt elides opt.text to fit the<br/>narrowed rect → "Properties" → "Prope..."
```

KeePassXC's delegate clears `opt.icon` and paints the icon itself afterward
(`painter->drawPixmap(...)`), but **does not** clear `opt.text` — the label
is left in `opt` and painted entirely inside `drawControl(CE_ItemViewItem)`,
which is the standard Qt path. `IconSelectionCorrectedStyle` only overrides
`drawPrimitive()` (for the rounded selection highlight) and, on Windows only,
`drawControl()` for a focus-color hack — on Linux, `drawControl()` is
untouched and falls straight through to Breeze.

---

## SMPlayer — Preferences sidebar (`sections` QListWidget)

```mermaid
sequenceDiagram
    participant QLW as QListWidget<br/>(sections)
    participant Del as QStyledItemDelegate<br/>(Qt's stock delegate — no override)
    participant Proxy as MyProxyStyle<br/>(QProxyStyle)
    participant Breeze as Style<br/>(breeze5.so, after user selects<br/>Preferences → General → Style → Breeze)
    participant Common as QCommonStyle<br/>(Breeze's ParentStyleClass)

    QLW->>Del: paint(painter, option, index)
    Note over Del: no custom paint() —<br/>SMPlayer never calls setItemDelegate()<br/>on "sections"
    Del->>Proxy: style()->drawControl(CE_ItemViewItem, &opt, painter)
    Proxy->>Proxy: styleHint() override present,<br/>irrelevant to this call
    Proxy->>Breeze: drawControl() not overridden —<br/>QProxyStyle forwards to baseStyle()
    Breeze->>Common: drawControl(CE_ItemViewItem)<br/>via ParentStyleClass
    Common->>Breeze: subElementRect(SE_ItemViewItemText, opt)
    Note over Breeze: same bug, same two lines,<br/>same kstyle/breezestyle.cpp
    Breeze-->>Common: narrowed textRect
    Common->>Common: drawItemText(painter, textRect,<br/>..., opt.text)
    Note over Common: "Advanced" → "Advan..."
```

SMPlayer's path is shorter: no custom delegate exists for `sections` at all,
so Qt's own `QStyledItemDelegate` handles `paint()` unmodified. `MyProxyStyle`
overrides only `styleHint()`, so `drawControl` and the `subElementRect` call
it triggers reach Breeze completely unmediated — SMPlayer's application code
never gets a chance to influence the text rect one way or the other.

---

## Why the Convergence Point Matters

Both diagrams reach the exact same function call:
`Style::subElementRect(SE_ItemViewItemText, viewItemOption, widget)`.
Neither application:

- computes its own text-clipping rect for this widget,
- calls `QFontMetrics::elidedText()` itself,
- or overrides `subElementRect` anywhere in its own proxy/delegate classes.

The third argument is the item view itself, not `nullptr`: KeePassXC passes
`opt.widget` into `drawControl(CE_ItemViewItem, …)`, SMPlayer's stock
`QStyledItemDelegate` passes the view it is painting, and `QCommonStyle`
forwards that pointer through to `subElementRect`. This detail matters because
Breeze's `Helper::itemViewItemMargins()` branches on
`qobject_cast<const QFrame *>(option->widget)` and
`qobject_cast<const QAbstractItemView *>(option->widget)` — so the margins it
returns can depend on what kind of widget is supplied. The `-v2` probes
(`make v2`) exist to close exactly that gap: they measure the same call against
a real, populated `QListWidget` with the widget pointer supplied consistently,
and report the same `diff=8` as the original probes. See
[`se-itemviewitemtext-proof.md`](se-itemviewitemtext-proof.md#6-hardened-cross-check-probe-v2).

The only variable between "labels fit" and "labels elide" in both apps is
which concrete `QStyle` instance answers that one call — confirmed directly
by the Fusion-vs-Breeze control comparison in
[`bug-analysis.md`](bug-analysis.md) and proven independent of any
application in [`se-itemviewitemtext-proof.md`](se-itemviewitemtext-proof.md).
