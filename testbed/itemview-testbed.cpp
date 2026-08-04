// itemview-testbed.cpp — interactive test bed for SE_ItemViewItemText elision
//
// Builds unmodified against Qt5 and Qt6 from this single source file.
//
// Purpose
// -------
// The probes under probe/ measure subElementRect() on a hand-built
// QStyleOptionViewItem. That is deliberate: it proves the narrowing exists with
// no application code in the path at all. But it also means the probes cannot
// answer a question that only a live view can:
//
//   Does the narrowing produce *visible* elision, in a view whose option was
//   filled in by QAbstractItemView itself rather than by us?
//
// This test bed closes that gap. The list below is constructed to match
// SMPlayer's Preferences sidebar, which is the reference case because it has no
// custom item delegate of any kind — its items render through Qt's stock
// QStyledItemDelegate. Provenance, from smplayer-25.6.0/src:
//
//   preferencesdialog.ui:38-53   QListWidget "sections", minimumSize width 150,
//                                horizontal size policy Preferred
//   preferencesdialog.cpp:57     sections->setUniformItemSizes(true)
//   preferencesdialog.cpp:58     sections->setResizeMode(QListView::Adjust)
//   preferencesdialog.cpp:60     sections->setMovement(QListView::Static)
//   preferencesdialog.cpp:143-146 ListMode, spacing 0, no maximum width
//   preferencesdialog.cpp:203-204 new QListWidgetItem(icon, text); addItem(i)
//
//   No setItemDelegate() call exists anywhere in SMPlayer's preferences dialog.
//   This file therefore sets none either. That is the point of the exercise.
//
// The style selector mirrors SMPlayer's own Preferences -> General -> Style
// dropdown, which is what makes SMPlayer a single-variable reproducer: the same
// process, the same widget, the same strings, with only the QStyle changing.
//
// What the readout shows
// ----------------------
// One line per row — not per selection. The effect is a property of the row set,
// not of whichever item happens to be current: on Qt 5.15 the clamp in
// viewItemLayout() sizes every row to its own natural width before the style's
// inset is taken, so each row is narrowed independently and every label elides.
// On Qt6 the same column is constant because each row receives the full
// remaining width. A single-row readout cannot show that difference at all —
// the numbers for any one row are identical between the two Qt versions.
//
// Every value comes from the option the *view* builds (not one we assemble),
// via QAbstractItemView::viewOptions() on Qt5 / initViewItemOption() on Qt6:
//
//   adv       fontMetrics.horizontalAdvance(text) — width of the glyphs
//   tMargin   pixelMetric(PM_FocusFrameHMargin) + 1 — the per-side padding
//             QCommonStylePrivate::viewItemLayout() reserves around item text
//   natural   adv + 2*tMargin — what QCommonStyle allocates to the text
//   textX/W   what subElementRect(SE_ItemViewItemText) actually returns
//   L, R      textX - itemX, and itemRight - textRight: the inset itself.
//             This is the quantity a width-guard patch claims to preserve —
//             clearance is a position, so width alone cannot verify it.
//   decoX/W   subElementRect(SE_ItemViewItemDecoration) — reported so a patch
//             can be shown not to disturb the sibling branch
//   drawn     textW - 2*tMargin, the width the text is actually laid out in
//   slack     drawn - adv — negative means it cannot fit
//   elides    whether QFontMetrics::elidedText() would shorten the string
//
//   showDecorationSelected is reported as the view filled it in, from
//   styleHint(SH_ItemView_ShowDecorationSelected). On Qt 5.15 that flag selects
//   between two different text-rect layouts inside viewItemLayout(); Qt 6
//   removed the branch. Breeze answers 0 to that hint and Fusion answers 1.
//
// "Dump measurements to text" writes the same table, with more columns, to
// stdout and to dumps/. Screenshots do not diff; text does — comparing a stock
// build against a patched one is the entire point of the exercise.
//
// Build:
//   make            (both, if both Qt versions are present)
//   make qt5        ./itemview-testbed-qt5
//   make qt6        ./itemview-testbed-qt6

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTextStream>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QVBoxLayout>
#include <QWidget>

// Exposes the option the view itself would paint with. Everything else in this
// file goes through public API; this subclass exists only because viewOptions()
// (Qt5) / initViewItemOption() (Qt6) is protected, and using the view's own
// option instead of a hand-built one is the entire point of the test bed.
class ViewOptionListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

    QStyleOptionViewItem optionFromView() const
    {
        QStyleOptionViewItem option;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        initViewItemOption(&option);
#else
        option = viewOptions();
#endif
        return option;
    }
};

// SMPlayer's Preferences section names, verbatim. This is the primary string
// set: SMPlayer is the application under test and the one discussed upstream,
// because its sidebar has no custom item delegate at all.
//   smplayer-25.6.0/src/prefgeneral.cpp etc., via PreferencesDialog::addSection()
static const char *kSMPlayerSections[] = {
    "General",
    "Drives (CD/DVD)",
    "Performance",
    "Subtitles",
    "Interface",
    "Keyboard and mouse shortcuts",
    "Advanced",
    nullptr,
};

// KeePassXC's Edit Entry navigation labels, verbatim from
// keepassxreboot/keepassxc, src/gui/entry/EditEntryWidget.cpp lines
// 200, 242, 272, 286, 323, 478, 484, 641 — each an addPage(tr("..."), ...) call.
//
// Included as a *length* dimension, not as an exhibit: these labels are much
// shorter than SMPlayer's, which puts them in the range where a 6px difference
// can decide whether a label fits. KeePassXC itself is deliberately out of
// scope for the upstream correspondence.
static const char *kKeePassXCNav[] = {
    "Entry",
    "Advanced",
    "Icon",
    "Auto-Type",
    "Browser Integration",
    "Properties",
    "History",
    "SSH Agent",
    nullptr,
};

// A deliberately graded set for locating the band edge by inspection: each
// string is a few pixels longer than the last.
static const char *kGradedLengths[] = {
    "Ii",
    "Icon",
    "Entry",
    "History",
    "Advanced",
    "Subtitles",
    "Auto-Type",
    "Performance",
    "Drives (CD/DVD)",
    "Browser Integration",
    nullptr,
};

class TestBed : public QWidget
{
public:
    TestBed()
    {
        auto *outer = new QHBoxLayout(this);

        // --- the view under test ------------------------------------------
        // Constructed to match SMPlayer's "sections" list. No item delegate is
        // installed, matching SMPlayer.
        _list = new ViewOptionListWidget;
        _list->setUniformItemSizes(true);                  // preferencesdialog.cpp:57
        _list->setResizeMode(QListView::Adjust);           // preferencesdialog.cpp:58
        _list->setMovement(QListView::Static);             // preferencesdialog.cpp:60

        // These are the settings SMPlayer's sidebar actually runs with.
        // PreferencesDialog::setIconMode() (preferencesdialog.cpp:134-151) would
        // change setUniformItemSizes to false for its ListMode branch, but it is
        // never called anywhere in the source — it exists only as a Q_PROPERTY
        // declaration (preferencesdialog.h:53,81). That branch is therefore dead
        // at runtime and the constructor's line-57 value stands.
        //
        // This matters, and not subtly: with uniformItemSizes(false) the rows
        // take the widest item's size hint rather than the viewport width, a
        // horizontal scrollbar appears, and the item is no longer constrained by
        // the view at all — at which point nothing elides and the "List width"
        // control stops controlling anything. SMPlayer's sidebar visibly does
        // elide, which is only possible with viewport-constrained items.
        _list->setMinimumWidth(0);                         // overridden by the width control

        outer->addWidget(_list);

        // --- controls -------------------------------------------------------
        auto *side = new QVBoxLayout;
        outer->addLayout(side, 1);

        auto *controls = new QGroupBox(QStringLiteral("Controls"));
        auto *form = new QFormLayout(controls);

        // Mirrors SMPlayer's Preferences -> General -> Style dropdown.
        _styleBox = new QComboBox;
        _styleBox->addItems(QStyleFactory::keys());
        form->addRow(QStringLiteral("Style"), _styleBox);

        _strings = new QComboBox;
        _strings->addItem(QStringLiteral("SMPlayer sections"), QVariant::fromValue(0));
        _strings->addItem(QStringLiteral("KeePassXC Edit Entry nav"), QVariant::fromValue(1));
        _strings->addItem(QStringLiteral("Graded lengths"), QVariant::fromValue(2));
        form->addRow(QStringLiteral("Strings"), _strings);

        // SMPlayer's sidebar has minimumSize width 150 (preferencesdialog.ui:47-51).
        // Sweeping this is how the wide-item case gets exercised.
        _width = new QSpinBox;
        _width->setRange(60, 800);
        _width->setValue(150);
        _width->setSuffix(QStringLiteral(" px"));
        form->addRow(QStringLiteral("List width"), _width);

        _showIcons = new QCheckBox(QStringLiteral("Show icons"));
        _showIcons->setChecked(true);
        form->addRow(_showIcons);

        _elide = new QComboBox;
        _elide->addItem(QStringLiteral("ElideRight"), int(Qt::ElideRight));
        _elide->addItem(QStringLiteral("ElideMiddle"), int(Qt::ElideMiddle));
        _elide->addItem(QStringLiteral("ElideLeft"), int(Qt::ElideLeft));
        _elide->addItem(QStringLiteral("ElideNone"), int(Qt::ElideNone));
        form->addRow(QStringLiteral("Elide mode"), _elide);

        // View mode and word wrap were added after a patch that behaved
        // correctly across 972 ListMode configurations was found to elide labels
        // in KeePassXC's entry-editor sidebar, which is a QListView in IconMode
        // with wordWrap enabled (CategoryListWidget.ui). Neither axis existed in
        // the matrix, so nothing measured could have caught it.
        //
        // IconMode puts the decoration above the text (decorationPosition Top),
        // a different branch of QCommonStylePrivate::viewItemLayout(). wordWrap
        // sets QStyleOptionViewItem::WrapText, which QAbstractItemView::
        // viewOptions() copies into every item's option.
        _viewMode = new QComboBox;
        _viewMode->addItem(QStringLiteral("ListMode (icon left)"), int(QListView::ListMode));
        _viewMode->addItem(QStringLiteral("IconMode (icon above)"), int(QListView::IconMode));
        form->addRow(QStringLiteral("View mode"), _viewMode);

        _wrap = new QCheckBox(QStringLiteral("Word wrap (sets WrapText)"));
        _wrap->setChecked(false);
        form->addRow(_wrap);

        auto *dumpBtn = new QPushButton(QStringLiteral("Dump measurements to text"));
        form->addRow(dumpBtn);
        _dumpPath = new QLabel;
        _dumpPath->setWordWrap(true);
        form->addRow(_dumpPath);

        side->addWidget(controls);

        // --- readout --------------------------------------------------------
        auto *readoutBox = new QGroupBox(QStringLiteral("Measured, for the selected row"));
        auto *readoutLayout = new QVBoxLayout(readoutBox);
        _readout = new QLabel;
        _readout->setTextInteractionFlags(Qt::TextSelectableByMouse);
        _readout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        readoutLayout->addWidget(_readout);

        // Static, and wrapped. Kept out of the readout's own HTML: that label
        // has word wrap off so the measurement table cannot reflow, and a long
        // unwrapped paragraph inside it forces the whole window several
        // thousand pixels wide.
        auto *footnote = new QLabel(QStringLiteral(
            "* assumes the style lets QCommonStyle paint the item, which Breeze does "
            "(it never overrides CE_ItemViewItem). Styles that paint item text "
            "themselves — Kvantum, for one — skip viewItemDrawText(), so these "
            "three rows do not apply to them. Where these rows and the rendered list "
            "disagree, the list is right."));
        footnote->setWordWrap(true);
        QFont footFont = footnote->font();
        footFont.setPointSizeF(footFont.pointSizeF() * 0.9);
        footnote->setFont(footFont);
        readoutLayout->addSpacing(8);
        readoutLayout->addWidget(footnote);

        side->addWidget(readoutBox);
        side->addStretch(1);

        // Pin the panels, not just the label inside them. With only _readout
        // fixed, the group box was still free to stretch, so the wrapped
        // footnote reflowed wider than the label and ran off the window edge,
        // taking the last table row with it.
        controls->setFixedWidth(740);
        readoutBox->setFixedWidth(740);

        // No width cap here. The window only ever ran away because the footnote
        // was unwrapped rich text inside _readout; now that it wraps, the panel
        // sizes to the measurement table, which must not be clipped — a clipped
        // readout silently drops rows off the bottom.
        // Fixed width, not minimum. The readout's content changes size as the
        // measurements change — "shown as" alternates between an elided and a
        // full string, numbers gain digits — and a content-driven width makes
        // the whole box resize and jump every time a value updates. Pinning the
        // width means only the height can respond, and the table (width='100%')
        // fills it regardless of what is in the cells.
        _readout->setFixedWidth(700);
        _readout->setWordWrap(false);   // the table must not reflow

        // Never let the layout compress the table below its sizeHint. Without
        // this the final row is shaved through the middle of its glyphs when the
        // window is short — the readout silently dropping content is exactly the
        // failure this tool exists to detect.
        _readout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

        connect(_styleBox, &QComboBox::currentTextChanged, this, &TestBed::applyStyle);
        connect(_width, QOverload<int>::of(&QSpinBox::valueChanged), this, &TestBed::applyWidth);
        connect(_showIcons, &QCheckBox::toggled, this, &TestBed::applyIcons);
        connect(_elide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestBed::applyElide);
        connect(_strings, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestBed::applyStrings);
        connect(_list, &QListWidget::currentRowChanged, this, &TestBed::refresh);
        connect(_viewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestBed::applyViewMode);
        connect(_wrap, &QCheckBox::toggled, this, &TestBed::applyWrap);
        connect(dumpBtn, &QPushButton::clicked, this, &TestBed::dump);

        applyStrings(0);   // populates the list

        // Start on whatever style the session actually resolved to, so the
        // first thing shown is the real-desktop configuration.
        const int idx = _styleBox->findText(qApp->style()->objectName(), Qt::MatchFixedString);
        if (idx >= 0) {
            _styleBox->setCurrentIndex(idx);
        }
        applyWidth(_width->value());
        applyElide(_elide->currentIndex());
        refresh();

        setWindowTitle(QStringLiteral("SE_ItemViewItemText test bed — Qt %1").arg(qVersion()));
    }

private:
    void applyStyle(const QString &name)
    {
        if (QStyle *s = QStyleFactory::create(name)) {
            // Applied to the view only, so the surrounding controls stay put and
            // the list is the sole thing that changes — the same single-variable
            // setup SMPlayer's own style dropdown provides.
            _list->setStyle(s);
            _list->viewport()->update();
            _ownedStyle.reset(s);
        }
        refresh();
    }

    void applyWidth(int w)
    {
        _list->setFixedWidth(w);

        // QListView caches its content width and does not recompute it when the
        // viewport shrinks, so without this the item rect keeps a stale width
        // from a previous, wider layout — reporting e.g. 640 for a 150px list,
        // with a spurious horizontal scrollbar. Every number here is a geometry
        // measurement, so a stale geometry is a wrong answer, not a cosmetic
        // defect.
        _list->doItemsLayout();
        _list->horizontalScrollBar()->setValue(0);

        refresh();
    }

    void applyStrings(int)
    {
        const char **set = kSMPlayerSections;
        switch (_strings->currentData().toInt()) {
        case 1: set = kKeePassXCNav; break;
        case 2: set = kGradedLengths; break;
        default: break;
        }

        _list->clear();
        for (const char **p = set; *p; ++p) {
            // preferencesdialog.cpp:203-204 — icon + text, stock item type,
            // no delegate installed.
            _list->addItem(new QListWidgetItem(
                _showIcons->isChecked() ? _list->style()->standardIcon(QStyle::SP_FileIcon) : QIcon(),
                QString::fromUtf8(*p)));
        }
        // Select the longest row: it is the one that shows the effect soonest.
        int longest = 0;
        for (int i = 1; i < _list->count(); ++i) {
            if (_list->item(i)->text().size() > _list->item(longest)->text().size()) {
                longest = i;
            }
        }
        _list->setCurrentRow(longest);
        refresh();
    }

    void applyIcons(bool on)
    {
        for (int i = 0; i < _list->count(); ++i) {
            _list->item(i)->setIcon(on ? _list->style()->standardIcon(QStyle::SP_FileIcon) : QIcon());
        }
        refresh();
    }

    void applyElide(int)
    {
        _list->setTextElideMode(Qt::TextElideMode(_elide->currentData().toInt()));
        refresh();
    }

    void applyViewMode(int)
    {
        _list->setViewMode(QListView::ViewMode(_viewMode->currentData().toInt()));
        // setViewMode resets movement and resize mode, so restore SMPlayer's.
        _list->setMovement(QListView::Static);
        _list->setResizeMode(QListView::Adjust);
        _list->doItemsLayout();
        refresh();
    }

    void applyWrap(bool on)
    {
        _list->setWordWrap(on);
        _list->doItemsLayout();
        refresh();
    }

    // One row's worth of measurement. Every number the tool reports comes from
    // here, so the on-screen table and the text dump can never disagree.
    struct RowMeasure {
        int row = -1;
        QString text;
        QRect itemRect;
        QRect textRect;
        QRect decoRect;
        int advance = 0;
        int tMargin = 0;
        int natural = 0;
        int leftInset = 0;      // textRect.left  - itemRect.left
        int rightInset = 0;     // itemRect.right - textRect.right
        int drawn = 0;
        int slack = 0;
        bool elides = false;
        QString shown;
        bool showDecoSel = false;
        int hint = 0;
    };

    RowMeasure measureRow(int row) const
    {
        RowMeasure m;
        QListWidgetItem *item = _list->item(row);
        if (!item) {
            return m;
        }
        QStyle *style = _list->style();

        // The option as the view itself builds it — including
        // showDecorationSelected, which the view copies from the style hint.
        QStyleOptionViewItem opt = _list->optionFromView();
        opt.rect = _list->visualItemRect(item);
        opt.text = item->text();
        opt.features |= QStyleOptionViewItem::HasDisplay;
        if (!item->icon().isNull()) {
            opt.features |= QStyleOptionViewItem::HasDecoration;
            opt.icon = item->icon();
        }
        opt.index = _list->model()->index(row, 0);
        opt.state |= QStyle::State_Enabled;
        // Only the genuinely current row carries State_Selected. Measuring every
        // row as if selected would misreport the non-selected ones.
        if (_list->currentRow() == row) {
            opt.state |= QStyle::State_Selected;
        }

        m.row = row;
        m.text = opt.text;
        m.itemRect = opt.rect;
        m.textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, _list);
        m.decoRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, _list);
        m.advance = opt.fontMetrics.horizontalAdvance(opt.text);
        m.tMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, _list) + 1;
        m.natural = m.advance + 2 * m.tMargin;

        // The quantity Patch 3 is actually about. The inset is what provides
        // clearance between the glyphs and the rounded selection highlight, so a
        // guard that claims to preserve it has to be checked on position, not
        // only on width.
        m.leftInset = m.textRect.left() - m.itemRect.left();
        m.rightInset = m.itemRect.right() - m.textRect.right();

        m.drawn = m.textRect.width() - 2 * m.tMargin;
        m.slack = m.drawn - m.advance;

        const auto mode = Qt::TextElideMode(_elide->currentData().toInt());
        m.shown = opt.fontMetrics.elidedText(opt.text, mode, m.drawn);
        m.elides = (m.shown != opt.text);

        m.showDecoSel = opt.showDecorationSelected;
        m.hint = style->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, &opt, _list);
        return m;
    }

    void refresh()
    {
        if (_list->count() == 0) {
            _readout->setText(QStringLiteral("(empty)"));
            return;
        }

        // One line per row. The per-row view is the point: on Qt5 the clamp sizes
        // every row to its own natural width before the inset is taken, so
        // `textW` varies down the column and every label elides. On Qt6 the same
        // column is constant, because each row receives the full remaining
        // width. A single-row readout cannot show that difference at all — the
        // numbers for any one row are identical between the two Qt versions.
        QString rows;
        const int current = _list->currentRow();
        for (int i = 0; i < _list->count(); ++i) {
            const RowMeasure m = measureRow(i);
            rows += QStringLiteral(
                        "<tr>"
                        "<td>%1%2</td><td>%3</td><td align='right'>%4</td>"
                        "<td align='right'>%5</td><td align='right'>%6</td>"
                        "<td align='right'>%7</td><td align='right'>%8</td>"
                        "<td align='right'>%9</td><td align='right'><b>%10</b></td>"
                        "<td><b>%11</b></td>"
                        "</tr>")
                        .arg(i)
                        .arg(i == current ? QStringLiteral("&nbsp;&#9656;") : QString())
                        .arg(elideForDisplay(m.text).toHtmlEscaped())
                        .arg(m.advance)
                        .arg(m.textRect.x())
                        .arg(m.textRect.width())
                        .arg(m.leftInset)
                        .arg(m.rightInset)
                        .arg(m.drawn)
                        .arg(m.slack)
                        .arg(m.elides ? QStringLiteral("YES") : QStringLiteral("no"));
        }

        const RowMeasure ref = measureRow(current >= 0 ? current : 0);

        _readout->setText(
            QStringLiteral(
                "<table width='100%' cellspacing='2' cellpadding='3'>"
                "<tr><td colspan='10'>item %1x%2 &nbsp; tMargin %3 &nbsp; "
                "showDecorationSelected <b>%4</b> (hint %5)</td></tr>"
                "<tr><td colspan='10'>&nbsp;</td></tr>"
                "<tr>"
                "<td><u>#</u></td><td><u>text</u></td><td align='right'><u>adv</u></td>"
                "<td align='right'><u>textX</u></td><td align='right'><u>textW</u></td>"
                "<td align='right'><u>L</u></td><td align='right'><u>R</u></td>"
                "<td align='right'><u>drawn</u>*</td><td align='right'><u>slack</u>*</td>"
                "<td><u>elides</u>*</td>"
                "</tr>"
                "%6"
                "<tr><td colspan='10'>&nbsp;</td></tr>"
                "</table>")
                .arg(ref.itemRect.width())
                .arg(ref.itemRect.height())
                .arg(ref.tMargin)
                .arg(ref.showDecoSel ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(ref.hint)
                .arg(rows));
    }

    static QString elideForDisplay(const QString &s)
    {
        return s.size() <= 22 ? s : s.left(21) + QChar(0x2026);
    }

public:
    // --- CLI configuration ------------------------------------------------
    // Same code paths the GUI controls use, so a scripted run and a hand-driven
    // run cannot diverge.
    bool setStyleByName(const QString &name)
    {
        const int i = _styleBox->findText(name, Qt::MatchFixedString);
        if (i < 0) {
            return false;
        }
        _styleBox->setCurrentIndex(i);
        return true;
    }
    void setListWidth(int w) { _width->setValue(w); }
    void setIconsOn(bool on) { _showIcons->setChecked(on); }
    bool setStringSet(const QString &key)
    {
        const QString k = key.toLower();
        int i = -1;
        if (k == QLatin1String("smplayer")) i = 0;
        else if (k == QLatin1String("keepassxc")) i = 1;
        else if (k == QLatin1String("graded")) i = 2;
        if (i < 0) {
            return false;
        }
        _strings->setCurrentIndex(i);
        return true;
    }
    bool setElideMode(const QString &key)
    {
        const QString k = key.toLower();
        int i = -1;
        if (k == QLatin1String("right")) i = 0;
        else if (k == QLatin1String("middle")) i = 1;
        else if (k == QLatin1String("left")) i = 2;
        else if (k == QLatin1String("none")) i = 3;
        if (i < 0) {
            return false;
        }
        _elide->setCurrentIndex(i);
        return true;
    }
    bool setViewMode(const QString &key)
    {
        const QString k = key.toLower();
        int i = -1;
        if (k == QLatin1String("list")) i = 0;
        else if (k == QLatin1String("icon")) i = 1;
        if (i < 0) {
            return false;
        }
        _viewMode->setCurrentIndex(i);
        return true;
    }
    void setWrap(bool on) { _wrap->setChecked(on); }
    QString dumpText() { return buildDump(); }

private:
    // Plain text, for diffing one build against another. Screenshots do not
    // diff; text does. Printed to stdout as well as written to a file, because
    // the terminal is captured in every screenshot anyway.
    void dump()
    {
        const QString out = buildDump();

        // stdout, so it lands in the terminal that is captured with every shot
        fputs(out.toUtf8().constData(), stdout);
        fflush(stdout);

        const QString name = QStringLiteral("%1/qt%2-%3-%4-icons%5-%6-%7-wrap%8-%9px.txt")
                                 .arg(sessionDumpDir())
                                 .arg(QString(qVersion()).section(QLatin1Char('.'), 0, 0))
                                 .arg(_list->style()->objectName())
                                 .arg(_strings->currentText().section(QLatin1Char(' '), 0, 0).toLower())
                                 .arg(_showIcons->isChecked() ? QStringLiteral("on") : QStringLiteral("off"))
                                 .arg(_elide->currentText().mid(5).toLower())
                                 .arg(_viewMode->currentIndex() == 0 ? QStringLiteral("list") : QStringLiteral("icon"))
                                 .arg(_wrap->isChecked() ? QStringLiteral("on") : QStringLiteral("off"))
                                 .arg(_width->value());
        QFile f(name);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(out.toUtf8());
            f.close();
            _dumpPath->setText(QStringLiteral("wrote %1").arg(name));
        } else {
            _dumpPath->setText(QStringLiteral("could not write %1").arg(name));
        }
    }

    QString buildDump()
    {
        QString out;
        QTextStream ts(&out);

        ts << "=== SE_ItemViewItemText test bed dump ===\n"
           << "when        : " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
           << "Qt          : " << qVersion() << "\n"
           << "style       : " << _list->style()->objectName() << "\n"
           << "breeze rpm  : " << breezeVersion() << "\n"
           << "strings     : " << _strings->currentText() << "\n"
           << "list width  : " << _width->value() << "\n"
           << "icons       : " << (_showIcons->isChecked() ? "on" : "off") << "\n"
           << "elide mode  : " << _elide->currentText() << "\n"
           << "view mode   : " << _viewMode->currentText() << "\n"
           << "word wrap   : " << (_wrap->isChecked() ? "on" : "off") << "\n";

        const RowMeasure ref = measureRow(_list->currentRow() >= 0 ? _list->currentRow() : 0);
        ts << "item rect   : " << ref.itemRect.width() << "x" << ref.itemRect.height() << "\n"
           << "tMargin     : " << ref.tMargin << "\n"
           << "showDecoSel : " << (ref.showDecoSel ? "true" : "false")
           << "  (styleHint " << ref.hint << ")\n\n";

        ts << "--- column glossary ------------------------------------------------\n"
              "  #        row index within the list\n"
              "  text     the item's label, verbatim\n"
              "  adv      fontMetrics.horizontalAdvance(text): width of the glyphs\n"
              "           themselves, at the option's own font. Style-independent.\n"
              "  natural  adv + 2*tMargin: the width QCommonStyle allocates to a\n"
              "           text-only item before any style-specific adjustment.\n"
              "  textX    x of subElementRect(SE_ItemViewItemText), in item coords\n"
              "  textW    width of that rect: what the style hands to the painter\n"
              "  L        textX - itemRect.left  (left inset)\n"
              "  R        itemRect.right - textRect.right  (right inset)\n"
              "           L and R are the inset a width-guard patch must preserve.\n"
              "           With icons on, L also contains the decoration, so read the\n"
              "           inset from an --icons off run; in stock-vs-patched diffs the\n"
              "           decoration contribution cancels and it does not matter.\n"
              "  decoX/W  subElementRect(SE_ItemViewItemDecoration): reported so that\n"
              "           a patch can be shown not to disturb the sibling branch.\n"
              "  drawn    textW - 2*tMargin. QCommonStylePrivate::viewItemDrawText()\n"
              "           removes tMargin from each side of whatever subElementRect()\n"
              "           returned, immediately before eliding, so this, not textW,\n"
              "           is the width the string is actually laid out in.\n"
              "  slack    drawn - adv. Negative means the string cannot fit and the\n"
              "           delegate will elide it.\n"
              "  elides   whether QFontMetrics::elidedText() shortens the string at\n"
              "           that width, i.e. whether a user sees an ellipsis.\n"
              "--------------------------------------------------------------------\n\n";

        ts << Qt::left << qSetFieldWidth(4) << "#"
           << qSetFieldWidth(31) << "text"
           << Qt::right << qSetFieldWidth(8) << "adv" << qSetFieldWidth(9) << "natural"
           << qSetFieldWidth(8) << "textX" << qSetFieldWidth(8) << "textW"
           << qSetFieldWidth(6) << "L" << qSetFieldWidth(6) << "R"
           << qSetFieldWidth(8) << "decoX" << qSetFieldWidth(8) << "decoW"
           << qSetFieldWidth(8) << "drawn" << qSetFieldWidth(8) << "slack"
           << qSetFieldWidth(9) << "elides" << qSetFieldWidth(0) << "\n";

        for (int i = 0; i < _list->count(); ++i) {
            const RowMeasure m = measureRow(i);
            ts << Qt::left << qSetFieldWidth(4) << i
               << qSetFieldWidth(31) << m.text
               << Qt::right << qSetFieldWidth(8) << m.advance << qSetFieldWidth(9) << m.natural
               << qSetFieldWidth(8) << m.textRect.x() << qSetFieldWidth(8) << m.textRect.width()
               << qSetFieldWidth(6) << m.leftInset << qSetFieldWidth(6) << m.rightInset
               << qSetFieldWidth(8) << m.decoRect.x() << qSetFieldWidth(8) << m.decoRect.width()
               << qSetFieldWidth(8) << m.drawn << qSetFieldWidth(8) << m.slack
               << qSetFieldWidth(9) << (m.elides ? "YES" : "no") << qSetFieldWidth(0) << "\n";
        }
        ts << "\nNote on drawn / slack / elides: these assume the style lets QCommonStyle\n"
              "paint the item. Styles that override CE_ItemViewItem and paint item text\n"
              "themselves (Kvantum, for one) never reach viewItemDrawText(), so for\n"
              "those three columns trust the rendered list over this table. Breeze does\n"
              "not override CE_ItemViewItem anywhere in kstyle/, so they apply to it.\n";
        return out;
    }

    // Cached: this shells out, and a scripted sweep calls it once per dump.
    static QString breezeVersion()
    {
        static QString cached;
        if (!cached.isEmpty()) {
            return cached;
        }
        QProcess p;
        p.start(QStringLiteral("rpm"), {QStringLiteral("-q"), QStringLiteral("plasma-breeze")});
        if (!p.waitForFinished(3000)) {
            cached = QStringLiteral("unknown-breeze-version");
            return cached;
        }
        const QString s = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        cached = s.isEmpty() ? QStringLiteral("unknown-breeze-version") : s;
        return cached;
    }

    // corpus/<full rpm package line>/<MMDDYYYY>-<NN>/
    //
    // Keyed on the installed plasma-breeze package first, because that is the
    // variable under test: a dump is only meaningful with respect to the Breeze
    // build that produced it. The dated, sequenced subdirectory keeps repeat
    // runs on the same day from overwriting each other, and keeps a run's files
    // together so two runs can be diffed directory-against-directory.
    static QString sessionDumpDir()
    {
        static QString dir;
        if (!dir.isEmpty()) {
            return dir;
        }
        const QString base = QStringLiteral("corpus/%1").arg(breezeVersion());
        const QString date = QDateTime::currentDateTime().toString(QStringLiteral("MMddyyyy"));
        int n = 1;
        QString candidate;
        do {
            candidate = QStringLiteral("%1/%2-%3")
                            .arg(base, date, QString::number(n).rightJustified(2, QLatin1Char('0')));
            ++n;
        } while (QDir(candidate).exists() && n < 100);
        QDir().mkpath(candidate);
        dir = candidate;
        return dir;
    }

    ViewOptionListWidget *_list = nullptr;
    QComboBox *_styleBox = nullptr;
    QSpinBox *_width = nullptr;
    QCheckBox *_showIcons = nullptr;
    QComboBox *_elide = nullptr;
    QComboBox *_strings = nullptr;
    QComboBox *_viewMode = nullptr;
    QCheckBox *_wrap = nullptr;
    QLabel *_readout = nullptr;
    QLabel *_dumpPath = nullptr;
    QScopedPointer<QStyle> _ownedStyle;
};

static void usage()
{
    fputs(
        "itemview-testbed — SE_ItemViewItemText measurement harness\n"
        "\n"
        "  (no arguments)        launch the interactive GUI\n"
        "\n"
        "  --dump                measure once, print the table to stdout, exit\n"
        "    --style NAME        style to load        (default: session style)\n"
        "    --width N           list width in px     (default: 150)\n"
        "    --strings SET       smplayer|keepassxc|graded  (default: smplayer)\n"
        "    --icons on|off      show item icons      (default: on)\n"
        "    --elide MODE        right|middle|left|none    (default: right)\n"
        "    --viewmode MODE     list|icon            (default: list)\n"
        "    --wrap on|off       word wrap / WrapText (default: off)\n"
        "  --list-styles         print available style names, exit\n"
        "\n"
        "In --dump mode the widget is laid out but never mapped to the screen\n"
        "(Qt::WA_DontShowOnScreen), so a scripted sweep produces identical\n"
        "geometry to the GUI without windows appearing. Example:\n"
        "\n"
        "  for w in 96 150 220 300 400; do\n"
        "    for s in Breeze Fusion; do\n"
        "      ./itemview-testbed-qt5 --dump --style $s --width $w\n"
        "    done\n"
        "  done > qt5-stock.txt\n"
        "\n",
        stderr);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--help")) || args.contains(QStringLiteral("-h"))) {
        usage();
        return 0;
    }
    if (args.contains(QStringLiteral("--list-styles"))) {
        for (const QString &k : QStyleFactory::keys()) {
            fprintf(stdout, "%s\n", qPrintable(k));
        }
        return 0;
    }

    if (args.contains(QStringLiteral("--dump"))) {
        auto value = [&args](const QString &flag, const QString &fallback) {
            const int i = args.indexOf(flag);
            return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : fallback;
        };

        TestBed bed;

        // Lay the widget out without ever mapping it to the screen, so a scripted
        // sweep yields the same geometry as the GUI with no windows flashing past.
        bed.setAttribute(Qt::WA_DontShowOnScreen, true);
        bed.resize(1250, 780);
        bed.show();

        const QString styleName = value(QStringLiteral("--style"), QString());
        if (!styleName.isEmpty() && !bed.setStyleByName(styleName)) {
            fprintf(stderr, "unknown style '%s' (try --list-styles)\n", qPrintable(styleName));
            return 2;
        }
        const QString set = value(QStringLiteral("--strings"), QStringLiteral("smplayer"));
        if (!bed.setStringSet(set)) {
            fprintf(stderr, "unknown string set '%s'\n", qPrintable(set));
            return 2;
        }
        const QString elide = value(QStringLiteral("--elide"), QStringLiteral("right"));
        if (!bed.setElideMode(elide)) {
            fprintf(stderr, "unknown elide mode '%s'\n", qPrintable(elide));
            return 2;
        }
        const QString vm = value(QStringLiteral("--viewmode"), QStringLiteral("list"));
        if (!bed.setViewMode(vm)) {
            fprintf(stderr, "unknown view mode '%s' (list|icon)\n", qPrintable(vm));
            return 2;
        }
        bed.setWrap(value(QStringLiteral("--wrap"), QStringLiteral("off")) == QLatin1String("on"));
        bed.setIconsOn(value(QStringLiteral("--icons"), QStringLiteral("on")) != QLatin1String("off"));
        bed.setListWidth(value(QStringLiteral("--width"), QStringLiteral("150")).toInt());

        // Let the layout settle before measuring; every value here is geometry.
        QApplication::processEvents();

        const QByteArray text = bed.dumpText().toUtf8();
        fwrite(text.constData(), 1, text.size(), stdout);
        fflush(stdout);
        return 0;
    }

    TestBed bed;
    // Tall enough that every readout row is visible in a screenshot even when
    // long values wrap to two lines. A clipped row is a missing measurement.
    bed.resize(1250, 780);
    bed.show();
    return app.exec();
}
