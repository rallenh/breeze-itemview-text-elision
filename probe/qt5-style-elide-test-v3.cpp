// qt5-style-elide-test-v3.cpp (Qt5)
//
// Additive companion to qt5-style-elide-test-v2.cpp. Does NOT replace or modify
// the v1 or v2 probes; it reuses v2's option construction verbatim so that the
// widths common to both must produce identical numbers.
//
// Question this probe exists to answer:
//
//   Every width measured by v1 and v2 (150, 120, 100, 80) is narrower than the
//   test string's own natural width. That means all published measurements were
//   taken in the *constrained* regime, where the item cannot fit its text no
//   matter which style is active. The unconstrained regime — where the item has
//   room to spare — was never measured.
//
//   That gap matters because Breeze's horizontal narrowing of
//   SE_ItemViewItemText is unconditional: it is applied at every width. To
//   establish that, and to evaluate any proposed fix that restores width only
//   where it is actually needed, both regimes have to appear in the same table.
//
// Differences from v2:
//   - Widths extended upward (400, 300, 220) so the table spans both regimes.
//     v2's four widths are retained unchanged for cross-checking.
//   - Columns added per row that show where the reference width comes from,
//     rather than stating it as a bare number:
//       advance = opt.fontMetrics.horizontalAdvance(opt.text)
//                 width of the glyphs themselves, at the option's own font
//       tMargin = pixelMetric(PM_FocusFrameHMargin) + 1
//                 the per-side padding QCommonStyle reserves around item text
//                 (QCommonStylePrivate::viewItemLayout names this `textMargin`)
//       natural = advance + 2 * tMargin
//                 the width QCommonStyle allocates to a text-only item before
//                 any style-specific adjustment
//       needed  = advance
//                 restated alongside `slack` so the comparison is explicit
//       slack   = textRect.width() - needed
//                 room left over; negative means the string cannot fit the
//                 rect it was given and the delegate will elide it
//   - opt.widget is now assigned explicitly. QStyleOption::initFrom() does NOT
//     set QStyleOptionViewItem::widget — that member belongs to the view-item
//     option, not to the base class, and initFrom() never touches it. v1 and v2
//     leave it null, which means Breeze's frame detection in
//     Helper::itemViewItemMargins() fails and they measure an UNFRAMED view.
//     Every real item view has a QFrame::StyledPanel, which reduces the
//     left/right margins by 1 each. The final section of this probe reports
//     both cases side by side so the difference is documented rather than
//     silently changing the numbers relative to v1/v2.
//
//     (This correction was found on 2026-07-30 by comparing the probes against
//     a live view in testbed/itemview-testbed.cpp, which takes its option from
//     QAbstractItemView itself and so has the widget set by construction.)
//
//   - No change to which style engines are queried or how subElementRect() is
//     called.
//
// Reading the output: `slack >= 0` rows are the unconstrained regime, `slack < 0`
// rows are the constrained one. `diff` is the number of pixels the style removed
// from the text rect. Breeze reports diff=8 in both regimes; Fusion and Windows
// report diff=0 in both.
//
// Build:
//   g++ -o qt5-style-elide-test-v3 qt5-style-elide-test-v3.cpp $(pkg-config --cflags --libs Qt5Widgets) -fPIC
//
// Run:
//   ./qt5-style-elide-test-v3

#include <QApplication>
#include <QListWidget>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <cstdio>

static void probe(const QString &styleName, QStyle *style, QListWidget *listWidget,
                   int itemWidth, bool withIconAndSelection, bool setOptWidget = true)
{
    QStyleOptionViewItem opt;
    opt.initFrom(listWidget);                 // real QListWidget, not a bare QWidget

    // QStyleOption::initFrom() does NOT set QStyleOptionViewItem::widget — that
    // member is specific to QStyleOptionViewItem and must be assigned directly.
    // Breeze reads option->widget in Helper::itemViewItemMargins() to decide
    // whether the view has a QFrame::StyledPanel, which changes the margins.
    // Leaving it null therefore measures an unframed view. v1 and v2 leave it
    // null; every real item view has a frame. See the widget-fidelity section
    // in main() for the size of the difference.
    if (setOptWidget) {
        opt.widget = listWidget;
    }

    opt.rect = QRect(0, 0, itemWidth, 24);
    opt.text = "Keyboard and mouse shortcuts";
    opt.features = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    opt.state = QStyle::State_Enabled;
    opt.index = listWidget->model()->index(0, 0);   // real, valid index (row 0)

    if (withIconAndSelection) {
        opt.decorationSize = QSize(22, 22);
        opt.icon = listWidget->item(0)->icon();
        opt.features |= QStyleOptionViewItem::HasDecoration;
        opt.state |= QStyle::State_Selected;
    } else {
        opt.decorationSize = QSize(0, 0);
    }

    // widget pointer passed to subElementRect matches opt.widget — real apps
    // always pass opt.widget here (see docs/sequence-diagrams.md)
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, listWidget);

    // Measured with the same QFontMetrics the style itself sees, so `advance` is
    // exactly the quantity a width-aware style fix would have available to it.
    const int advance = opt.fontMetrics.horizontalAdvance(opt.text);

    // QCommonStylePrivate::viewItemLayout() computes its text margin as
    // PM_FocusFrameHMargin + 1, and reserves it on each side of the text.
    const int tMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, listWidget) + 1;
    const int natural = advance + 2 * tMargin;

    // QCommonStylePrivate::viewItemDrawText() removes tMargin from each side of
    // whatever subElementRect() returned, immediately before eliding:
    //     QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0);
    // Comparing the string against textRect.width() therefore reports "fits" for
    // text that visibly elides. `drawn` is the width the string is actually laid
    // out in, and is what `slack` must be measured against.
    const int drawn = textRect.width() - 2 * tMargin;
    const int slack = drawn - advance;
    const QString shown = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, drawn);
    const bool elides = (shown != opt.text);

    printf("[%-14s] %-17s itemWidth=%3d  textRect=(%d,%d %dx%d)  textWidth=%3d  "
           "advance=%3d  tMargin=%d  natural=%3d  drawn=%4d  slack=%4d  elides=%-3s  diff=%d\n",
           styleName.toUtf8().constData(),
           setOptWidget ? (withIconAndSelection ? "icon+selected" : "baseline")
                        : (withIconAndSelection ? "icon+sel/noWidget" : "baseline/noWidget"),
           itemWidth,
           textRect.x(), textRect.y(),
           textRect.width(), textRect.height(),
           textRect.width(),
           advance, tMargin, natural,
           drawn, slack,
           elides ? "YES" : "no",
           itemWidth - textRect.width());
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    printf("Qt version: %s\n", qVersion());
    printf("Active style: %s\n\n", app.style()->objectName().toUtf8().constData());

    // One real QListWidget, populated with one real item — reused for every
    // measurement below rather than constructed/leaked per call.
    QListWidget listWidget;
    listWidget.addItem(new QListWidgetItem(app.style()->standardIcon(QStyle::SP_FileIcon),
                                            "Keyboard and mouse shortcuts"));

    printf("=== SE_ItemViewItemText rect probe v3 (Qt5, slack-aware width sweep) ===\n");
    printf("advance = fontMetrics.horizontalAdvance(text)  width of the glyphs themselves\n");
    printf("tMargin = pixelMetric(PM_FocusFrameHMargin) + 1  per-side padding QCommonStyle reserves\n");
    printf("natural = advance + 2*tMargin                 what QCommonStyle allocates to the text\n");
    printf("drawn   = textWidth - 2*tMargin               viewItemDrawText() removes tMargin from\n");
    printf("                                              each side again before eliding, so this,\n");
    printf("                                              not textWidth, is the layout width\n");
    printf("slack   = drawn - advance                     negative = cannot fit; delegate elides\n");
    printf("elides  = elidedText(text, ElideRight, drawn) != text\n");
    printf("diff    = itemWidth - textWidth               NOTE: regime-dependent. Where the Qt5\n");
    printf("                                              clamp caps textWidth at `natural`, diff\n");
    printf("                                              grows with itemWidth and does NOT mean\n");
    printf("                                              'pixels taken by the style'. The sound\n");
    printf("                                              cross-style measure is breeze vs Fusion\n");
    printf("                                              textWidth at the same itemWidth.\n\n");

    QStringList styleNames = {"breeze", "Fusion", "Windows"};
    // 400/300/220 are new in v3 and span the unconstrained regime.
    // 150/120/100/80 are v2's widths, unchanged, and must reproduce v2's numbers.
    QList<int> widths = {400, 300, 220, 150, 120, 100, 80};

    for (int w : widths) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) { printf("[%-14s]  (style not found)\n", name.toUtf8().constData()); continue; }
            probe(name, s, &listWidget, w, /*withIconAndSelection=*/false);
            probe(name, s, &listWidget, w, /*withIconAndSelection=*/true);
            delete s;
        }
        printf("\n");
    }

    // --- widget fidelity ----------------------------------------------------
    printf("=== opt.widget fidelity: what leaving QStyleOptionViewItem::widget null costs ===\n");
    printf("QStyleOption::initFrom() does not assign QStyleOptionViewItem::widget. Breeze reads\n");
    printf("option->widget in Helper::itemViewItemMargins() to detect a QFrame::StyledPanel and,\n");
    printf("when it finds one, reduces the left/right margins by 1 each (breezehelper.cpp:1777-1780,\n");
    printf("\"Breeze frame has one extra white pixel\"). Every real item view has that frame.\n\n");
    printf("v1 and v2 leave opt.widget null and therefore measure the UNFRAMED case. The rows\n");
    printf("above set it, and so measure what a real framed QListWidget receives. This section\n");
    printf("shows both, so the difference between this probe and v1/v2 is documented rather than\n");
    printf("looking like an inconsistency.\n\n");

    for (int w : {400, 150, 80}) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) { printf("[%-14s]  (style not found)\n", name.toUtf8().constData()); continue; }
            probe(name, s, &listWidget, w, /*withIconAndSelection=*/false, /*setOptWidget=*/false);
            probe(name, s, &listWidget, w, /*withIconAndSelection=*/false, /*setOptWidget=*/true);
            delete s;
        }
        printf("\n");
    }

    printf("Cross-check: the */noWidget rows at 150 and 80 must match probe/qt5-style-elide-test-v2's\n");
    printf("output exactly, since that construction is what v2 uses. The rows with opt.widget set\n");
    printf("are new measurements and are the ones that describe a real view.\n");

    return 0;
}
