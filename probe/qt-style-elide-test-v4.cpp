// qt-style-elide-test-v4.cpp (Qt6)
//
// Additive companion to qt-style-elide-test-v3.cpp. Does NOT replace or modify
// the v1, v2 or v3 probes.
//
// Question this probe exists to answer:
//
//   v3 measured, under Qt 5.15, a text rect that does not grow when the item
//   grows: breeze reported textWidth=186 and Fusion textWidth=194 at itemWidth
//   220, 300 and 400 alike. A measured value that ignores its input needs an
//   explanation before any conclusion is drawn from it.
//
//   The explanation is in QCommonStylePrivate::viewItemLayout(). Qt 5.15 ends
//   that function with:
//
//     if (opt->showDecorationSelected)
//         *textRect = display;                       // all remaining width
//     else
//         *textRect = QStyle::alignedRect(opt->direction, opt->displayAlignment,
//                                         textRect->size().boundedTo(display.size()),
//                                         display); // clamped to natural text size
//
//   Qt 6 removed that conditional entirely ("the textRect takes up all
//   remaining size"), which is why v3's Qt6 run showed the text rect tracking
//   the item width while the Qt5 run did not.
//
//   QStyleOptionViewItem::showDecorationSelected defaults to false, and v3 (like
//   v1 and v2) never set it. Real applications do not set it by hand either:
//   QAbstractItemView::viewOptions() assigns it from
//   style()->styleHint(SH_ItemView_ShowDecorationSelected, ...). So the branch a
//   real Qt5 application takes is chosen by the active style, and differs
//   between styles: Qt 5.15's QFusionStyle returns 1 for that hint, while
//   QCommonStyle returns false. Breeze does not override the hint at all.
//
// This probe also assigns opt.widget, which v1 and v2 do not. initFrom() does
// not set QStyleOptionViewItem::widget, so those probes measure an unframed
// view and report an 8px inset where a real framed QListWidget receives 6px.
// See the widget-fidelity section of v3 for both numbers side by side.
//
// This probe therefore reports two things the earlier probes did not:
//   1. What each real, QStyleFactory-loaded style plugin actually answers for
//      SH_ItemView_ShowDecorationSelected — measured from the loaded .so rather
//      than inferred from source.
//   2. The same width sweep run twice, with showDecorationSelected explicitly
//      false and explicitly true, so the effect of the branch is isolated from
//      the effect of Breeze's horizontal inset.
//
// Note on the earlier results: at widths narrower than the string's natural
// width, boundedTo(display.size()) returns the display width, so both branches
// produce the same rect. The branch can only change the outcome where the item
// is wider than its text. Every width in v1 and v2 is in the former regime.
//
// Build:
//   g++ -o qt-style-elide-test-v4 qt-style-elide-test-v4.cpp $(pkg-config --cflags --libs Qt6Widgets) -fPIC
//
// Run:
//   ./qt-style-elide-test-v4

#include <QApplication>
#include <QListWidget>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <cstdio>

static void probe(const QString &styleName, QStyle *style, QListWidget *listWidget,
                   int itemWidth, bool showDecorationSelected)
{
    QStyleOptionViewItem opt;
    opt.initFrom(listWidget);

    // QStyleOption::initFrom() does NOT set QStyleOptionViewItem::widget. Breeze
    // reads option->widget in Helper::itemViewItemMargins() to detect a
    // QFrame::StyledPanel and, when present, reduces the left/right margins by
    // 1 each (breezehelper.cpp:1777-1780). Leaving it null measures an unframed
    // view; every real item view has the frame. v1/v2 leave it null — see the
    // widget-fidelity section of v3.
    opt.widget = listWidget;

    opt.rect = QRect(0, 0, itemWidth, 24);
    opt.text = "Keyboard and mouse shortcuts";
    opt.features = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    opt.state = QStyle::State_Enabled;
    opt.index = listWidget->model()->index(0, 0);
    opt.decorationSize = QSize(0, 0);

    // The only variable this probe moves relative to v3.
    opt.showDecorationSelected = showDecorationSelected;

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, listWidget);

    // advance  — width of the glyphs themselves, at the option's own font.
    // tMargin  — QCommonStylePrivate::viewItemLayout() computes its text margin
    //            as PM_FocusFrameHMargin + 1 and reserves it on each side.
    // natural  — advance + 2*tMargin, the width QCommonStyle allocates to the
    //            text, and the value the clamped branch bounds the rect to.
    const int advance = opt.fontMetrics.horizontalAdvance(opt.text);
    const int tMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, listWidget) + 1;
    const int natural = advance + 2 * tMargin;

    // viewItemDrawText() removes tMargin from each side of whatever
    // subElementRect() returned, before eliding. Measuring against
    // textRect.width() reports "fits" for text that visibly elides.
    const int drawn = textRect.width() - 2 * tMargin;
    const int slack = drawn - advance;
    const QString shown = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, drawn);

    printf("[%-14s] showDecoSel=%-5s itemWidth=%3d  textRect=(%d,%d %dx%d)  "
           "advance=%3d  tMargin=%d  natural=%3d  drawn=%4d  slack=%4d  elides=%-3s  diff=%d\n",
           styleName.toUtf8().constData(),
           showDecorationSelected ? "true" : "false",
           itemWidth,
           textRect.x(), textRect.y(),
           textRect.width(), textRect.height(),
           advance, tMargin, natural,
           drawn, slack,
           (shown != opt.text) ? "YES" : "no",
           itemWidth - textRect.width());
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    printf("Qt version: %s\n", qVersion());
    printf("Active style: %s\n\n", app.style()->objectName().toUtf8().constData());

    QListWidget listWidget;
    listWidget.addItem(new QListWidgetItem(app.style()->standardIcon(QStyle::SP_FileIcon),
                                            "Keyboard and mouse shortcuts"));

    QStringList styleNames = {"breeze", "Fusion", "Windows"};

    // --- Part 1: what does each loaded style plugin answer for the hint? ------
    printf("=== SH_ItemView_ShowDecorationSelected, as answered by each loaded style ===\n");
    printf("This is the value QAbstractItemView::viewOptions() copies into\n");
    printf("QStyleOptionViewItem::showDecorationSelected for a real application.\n\n");
    {
        QStyleOptionViewItem opt;
        opt.initFrom(&listWidget);
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) { printf("[%-14s]  (style not found)\n", name.toUtf8().constData()); continue; }
            const int hint = s->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, &opt, &listWidget);
            printf("[%-14s] SH_ItemView_ShowDecorationSelected = %d  (%s)\n",
                   name.toUtf8().constData(), hint, hint ? "text rect takes all remaining width"
                                                         : "text rect clamped to natural text size");
            delete s;
        }
    }
    printf("\n");

    // --- Part 2: the width sweep, with the branch forced both ways ------------
    printf("=== SE_ItemViewItemText rect probe v4 (Qt6, showDecorationSelected isolated) ===\n");
    printf("advance = fontMetrics.horizontalAdvance(text)  width of the glyphs themselves\n");
    printf("tMargin = pixelMetric(PM_FocusFrameHMargin) + 1  per-side padding QCommonStyle reserves\n");
    printf("natural = advance + 2*tMargin                 what the clamped branch bounds the rect to\n");
    printf("drawn   = textWidth - 2*tMargin               viewItemDrawText() removes tMargin from\n");
    printf("                                              each side again before eliding\n");
    printf("slack   = drawn - advance                     negative = cannot fit; delegate elides\n");
    printf("elides  = elidedText(text, ElideRight, drawn) != text\n");
    printf("diff    = itemWidth - textWidth               regime-dependent; see v3's note\n\n");

    QList<int> widths = {400, 300, 220, 150, 120, 100, 80};

    for (int w : widths) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) { printf("[%-14s]  (style not found)\n", name.toUtf8().constData()); continue; }
            probe(name, s, &listWidget, w, /*showDecorationSelected=*/false);
            probe(name, s, &listWidget, w, /*showDecorationSelected=*/true);
            delete s;
        }
        printf("\n");
    }

    printf("Expected, if the reading of viewItemLayout() above is correct:\n");
    printf("  - showDecoSel=false rows reproduce v3 exactly.\n");
    printf("  - showDecoSel=true rows show textRect tracking itemWidth.\n");
    printf("  - The breeze-minus-Fusion difference in textWidth stays 8 in both,\n");
    printf("    since Breeze's inset is applied after this branch and independently\n");
    printf("    of it.\n");

    return 0;
}
