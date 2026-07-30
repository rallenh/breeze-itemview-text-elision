// qt-style-elide-test-v2.cpp (Qt6)
//
// Hardened companion to qt-style-elide-test.cpp. Does NOT replace or modify
// the original probe — it exists to close a methodological question raised
// during review: does subElementRect(SE_ItemViewItemText) behave differently
// when given a real QListWidget (what every tested application actually
// uses) instead of a bare QWidget?
//
// Breeze's Helper::itemViewItemMargins() branches on:
//   qobject_cast<const QFrame *>(option->widget)             (frame-shape check)
//   qobject_cast<const QAbstractItemView *>(option->widget)  (selection-behavior check)
// A bare QWidget fails both casts; a real QListWidget passes both. This
// probe checks whether that difference changes the measured diff.
//
// Differences from the original probe:
//   - opt.widget is a real, populated QListWidget (not `new QWidget()`, and
//     not leaked — one instance, reused for every measurement).
//   - The widget pointer passed to subElementRect() matches opt.widget
//     (the original probe passed nullptr for that argument).
//   - opt.index is a real QModelIndex from the list widget's model, not a
//     default-constructed (invalid) one.
//   - A second variant repeats every measurement with a real icon
//     (opt.decorationSize = 22x22, matching typical sidebar icon sizes) and
//     State_Selected set, to directly demonstrate — rather than assert from
//     reading the source — that neither gates the horizontal narrowing.
//
// Build:
//   g++ -o qt-style-elide-test-v2 qt-style-elide-test-v2.cpp $(pkg-config --cflags --libs Qt6Widgets) -fPIC
//
// Run:
//   ./qt-style-elide-test-v2

#include <QApplication>
#include <QListWidget>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <cstdio>

static void probe(const QString &styleName, QStyle *style, QListWidget *listWidget,
                   int itemWidth, bool withIconAndSelection)
{
    QStyleOptionViewItem opt;
    opt.initFrom(listWidget);                 // real QListWidget, not a bare QWidget
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

    printf("[%-14s] %-16s itemWidth=%3d  textRect=(%d,%d %dx%d)  textWidth=%3d  diff=%d\n",
           styleName.toUtf8().constData(),
           withIconAndSelection ? "icon+selected" : "baseline",
           itemWidth,
           textRect.x(), textRect.y(),
           textRect.width(), textRect.height(),
           textRect.width(),
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

    printf("=== SE_ItemViewItemText rect probe v2 (Qt6, real QListWidget) ===\n");
    printf("diff = itemWidth - textRect.width()  (positive = pixels stolen from text)\n\n");

    QStringList styleNames = {"breeze", "Fusion", "Windows"};
    QList<int> widths = {150, 120, 100, 80};

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

    printf("Cross-check: these numbers should match probe/qt-style-elide-test's\n");
    printf("baseline output exactly. Any divergence means opt.widget fidelity\n");
    printf("(bare QWidget vs real QListWidget) matters for this Breeze build,\n");
    printf("and the original probe's numbers should be treated as approximate.\n");

    return 0;
}
