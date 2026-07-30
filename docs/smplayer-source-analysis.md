# SMPlayer 25.6.0: SE_ItemViewItemText Exposure Analysis

*Part of the evidence set for KDE bug [523118](https://bugs.kde.org/show_bug.cgi?id=523118) — see the [repository overview](../README.md) for the full set.*

## Summary

SMPlayer's Preferences sidebar (`PreferencesDialog::sections`) is a plain
`QListWidget` populated with stock `QListWidgetItem`s. No custom item delegate
is ever installed on it — `setItemDelegate()` is never called for `sections`
anywhere in the codebase. Painting therefore goes through Qt's own default
`QStyledItemDelegate`, which calls `drawControl(CE_ItemViewItem)` on the
active application style. SMPlayer wraps that active style in a thin
`QProxyStyle` (`MyProxyStyle`) that overrides only `styleHint()` — `drawControl`
and `subElementRect` are never touched, so both fall straight through to
whatever style is currently based in. When the user sets
`Preferences → General → Style → Breeze`, that base style becomes `breeze5.so`,
and `subElementRect(SE_ItemViewItemText)` is called unmediated.

This makes SMPlayer's exposure to the bug *more direct* than KeePassXC's —
KeePassXC at least has a custom delegate in between; SMPlayer's sidebar uses
Qt's stock item-view painting path with nothing in front of it at all.

Source examined: `smplayer-25.6.0-2.fc43.src.rpm` — the exact build used for
this repo's screenshots and probe runs.

Citations below link to upstream SMPlayer at tag [`v25.6.0`](https://github.com/smplayer-dev/smplayer/tree/v25.6.0)
so they can be followed without downloading a source RPM. That substitution is
safe here: every file cited in this document is **byte-identical** between the
Fedora tarball and upstream `v25.6.0` (verified by `diff`), and Fedora's two
patches touch only `src/smplayer.pro` and `smplayer.desktop`, neither of which
is cited or relevant to item-view painting. Line numbers therefore match in
both.

---

## The Widget Under Examination

`PreferencesDialog::sections` — the Preferences dialog's left-hand category
list (General / Drives / Performance / Subtitles / Interface /
Keyboard and mouse / Playlist / TV and radio / Updates / Network / Advanced). Declared in
[`src/preferencesdialog.ui`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/preferencesdialog.ui):

```xml
<widget class="QListWidget" name="sections" >
 <property name="sizePolicy" >
  <sizepolicy>
   <hsizetype>3</hsizetype>   <!-- MinimumExpanding -->
   <vsizetype>7</vsizetype>   <!-- Expanding -->
   <horstretch>0</horstretch>
   <verstretch>0</verstretch>
  </sizepolicy>
 </property>
 <property name="minimumSize" >
  <size>
   <width>150</width>
   <height>0</height>
  </size>
 </property>
</widget>
```

The sidebar therefore has a **150px floor** and a horizontally
`MinimumExpanding` policy: it will not shrink below 150px, and grows only as
the layout allows. That is the pixel budget the shipped widget actually works
within, and it is why the probe measures 150px alongside narrower widths — the
150px column is not an arbitrary choice, it is this widget's declared minimum.
`sections->setMovement(QListView::Static)` is set in the constructor, but
nothing there touches item painting.

Items are added via `PreferencesDialog::addSection()`
([`src/preferencesdialog.cpp:202-206`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/preferencesdialog.cpp#L202-L206)):

```cpp
void PreferencesDialog::addSection(PrefWidget *w) {
	QListWidgetItem *i = new QListWidgetItem( w->sectionIcon(), w->sectionName() );
	sections->addItem( i );
	pages->addWidget(w);
}
```

Plain `QListWidgetItem` construction — icon + text, nothing else. No
per-item painting logic, no delegate assignment.

**Confirmed via grep**: `setItemDelegate` appears four times in SMPlayer's
source (`bookmarkdialog.cpp`, `playlist.cpp`, `favoriteeditor.cpp`,
`actionseditor.cpp` [commented out]) — all on unrelated `QTableWidget`/list
widgets for bookmarks, the playlist, and favorites. None reference `sections`.
The Preferences sidebar renders with Qt's built-in `QStyledItemDelegate`.

---

## How the Active Style Is Chosen

SMPlayer wraps `QApplication::setStyle()` in its own `QProxyStyle` subclass,
`MyProxyStyle`, defined in
[`src/myapplication.cpp:36-45`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/myapplication.cpp#L36-L45):

```cpp
class MyProxyStyle : public QProxyStyle {
  public:
    int styleHint(StyleHint hint, const QStyleOption *option = 0,
                  const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle:: SH_ItemView_ActivateItemOnSingleClick)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};
```

The **only** override is `styleHint()`, and only to suppress single-click
activation. `drawControl()` and `subElementRect()` are not overridden, so
`QProxyStyle`'s defaults apply: both forward unmodified to `baseStyle()`.

At startup ([`src/myapplication.cpp:65-66`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/myapplication.cpp#L65-L66)):

```cpp
#ifdef OS_UNIX_NOT_MAC
	proxy_style = new MyProxyStyle;
	setStyle(proxy_style);
#endif
```

`proxy_style` becomes `QApplication::style()` for the whole process. Its
base style is set/reset via `MyApplication::changeStyle()`
([`src/myapplication.cpp:71-74`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/myapplication.cpp#L71-L74)):

```cpp
void MyApplication::changeStyle(const QString & style_name) {
	QStyle * style = QStyleFactory::create(style_name);
	if (style) proxy_style->setBaseStyle(style);
}
```

`changeStyle()` is invoked from `BaseGui::applyStyles()`
([`src/basegui.cpp:5894-5911`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/basegui.cpp#L5894-L5911)), driven directly by the user's
Preferences → General → Style selection (`pref->style`):

```cpp
#if STYLE_SWITCHING
	QString style = pref->style;
	if (style.isEmpty()) style = default_style;
	if (!style.isEmpty()) {
		#ifdef OS_UNIX_NOT_MAC
		MyApplication * app = static_cast<MyApplication *>(qApp);
		app->changeStyle(style);
		#else
		qApp->setStyle(style);
		#endif
	}
#endif
```

The style names offered in the combo box come straight from Qt itself
([`src/prefinterface.cpp:57`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/prefinterface.cpp#L57)): `style_combo->addItems(QStyleFactory::keys())`
— this is why "Breeze" appears as a selectable option at all: it's whatever
`QStyleFactory` finds installed (`breeze5.so`), not something SMPlayer knows
about specifically.

**Default style is Fusion**, confirmed in
[`src/preferences.cpp:418`](https://github.com/smplayer-dev/smplayer/blob/v25.6.0/src/preferences.cpp#L418): `style = "Fusion";`. This is why
SMPlayer does not exhibit the bug out of the box — it actively opts out of
inheriting the KDE session style at startup, unlike KeePassXC and GoldenDict.

---

## The Call Chain for CE_ItemViewItem

```
QListWidget::paintEvent
  → QStyledItemDelegate::paint(painter, option, index)     ← Qt's stock delegate, unmodified
      → style()->drawControl(CE_ItemViewItem, &opt, painter, widget)
          → MyProxyStyle::drawControl                       ← not overridden
          → QProxyStyle::drawControl(CE_ItemViewItem)        ← forwards to baseStyle()
          → baseStyle()->drawControl(CE_ItemViewItem)        ← Breeze, once user selects it
          → QCommonStyle::drawControl(CE_ItemViewItem)       ← via Breeze's ParentStyleClass
          → Style::subElementRect(SE_ItemViewItemText)       ← THE BUG IS HERE
```

No application code sits between `QStyledItemDelegate::paint()` and Breeze's
`subElementRect()` except a proxy that doesn't touch either call. This is the
shortest, least-mediated path of any of the three applications tested in
this repo.

---

## Correlating Source With the Empirical Test Results

| Finding | Source basis |
|---|---|
| Sidebar column has a declared 150px minimum width | `preferencesdialog.ui` — `minimumSize` width 150, `MinimumExpanding` horizontal policy; the 150px probe column is this widget's floor, not an arbitrary figure |
| No custom delegate — stock `QStyledItemDelegate` | `setItemDelegate` never called on `sections`; grep-confirmed |
| Style-switching is a real `QStyleFactory::create()` + rebase, not cosmetic | `MyApplication::changeStyle()`, invoked from `BaseGui::applyStyles()` |
| Default is Fusion, not inherited from KDE session | `preferences.cpp:418`, confirmed against README's stated default behavior |
| Breeze selection routes to real `breeze5.so`/`breeze6.so` | `QStyleFactory::keys()` populates the combo — same plugin registry KDE's platform theme uses |

Every behavioral observation in [`bug-analysis.md`](bug-analysis.md) and
[`probe/MY-TEST-NOTES.md`](../probe/MY-TEST-NOTES.md) (elides on Breeze, clean on
Fusion, fixed by Patch 1, still broken on Patch 2) is now explained at the
source level: the sidebar has no code of its own that
could account for the difference — the only thing that changes between
"Fusion" and "Breeze" in these tests is which style's `subElementRect`
implementation gets called, because nothing in SMPlayer's path intercepts it.

---

## Suggestion

Same conclusion as [`keepassxc-qt6-analysis.md`](keepassxc-qt6-analysis.md): no
change is needed in SMPlayer. The [`patches/0001-remove-horizontal-text-rect-narrowing.patch`](../patches/0001-remove-horizontal-text-rect-narrowing.patch)
fix to `plasma-breeze`'s `kstyle/breezestyle.cpp` resolves the defect at its
source; both `breeze5.so` and `breeze6.so` are compiled from that file, so
the fix covers SMPlayer's Qt5 build without any SMPlayer-side action.
