// qt5-style-elide-test.cpp
// Probes CT_ItemViewItem size inflation in Qt5 styles.
// Measures how much each style adds to the item size hint beyond content size,
// which indirectly starves the text rect and causes elision in narrow columns.
//
// Build:
//   g++ -o qt5-style-elide-test qt5-style-elide-test.cpp \
//       $(pkg-config --cflags --libs Qt5Widgets) -fPIC
//
// Run:
//   ./qt5-style-elide-test

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QWidget>
#include <cstdio>

// Probe CT_ItemViewItem: ask each style for the size hint given a fixed
// content size, and report how much it inflated the width vs. content.
static void probeSizeFromContents(const QString &styleName, QStyle *style, int contentWidth)
{
    QStyleOptionViewItem opt;
    opt.initFrom(new QWidget());
    opt.rect = QRect(0, 0, contentWidth, 20);
    opt.text = "Keyboard and mouse shortcuts";
    opt.features = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    opt.state = QStyle::State_Enabled;
    opt.decorationSize = QSize(0, 0);

    QSize contentSize(contentWidth, 20);
    QSize hintSize = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, contentSize, nullptr);

    int widthAdded  = hintSize.width()  - contentSize.width();
    int heightAdded = hintSize.height() - contentSize.height();

    printf("[%-14s]  contentWidth=%3d  hintSize=%dx%d  widthAdded=%d  heightAdded=%d\n",
           styleName.toUtf8().constData(),
           contentWidth,
           hintSize.width(), hintSize.height(),
           widthAdded,
           heightAdded);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    printf("Qt version: %s\n", qVersion());
    printf("Available styles: ");
    for (const QString &s : QStyleFactory::keys())
        printf("%s ", s.toUtf8().constData());
    printf("\n");
    printf("Active style: %s\n", app.style()->objectName().toUtf8().constData());

    printf("\n=== CT_ItemViewItem size inflation probe (Qt5) ===\n");
    printf("widthAdded = hintSize.width - contentWidth  (positive = pixels added beyond content)\n");
    printf("These extra pixels are NOT available for text, causing elision in narrow columns.\n\n");

    QStringList styleNames = {"breeze", "Fusion", "Windows"};
    QList<int> widths = {150, 120, 100, 80};

    for (int w : widths) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) {
                printf("[%-14s]  (style not found)\n", name.toUtf8().constData());
                continue;
            }
            probeSizeFromContents(name, s, w);
            delete s;
        }
        printf("\n");
    }

    // Also probe SE_ItemViewItemText for comparison with Qt6 probe
    printf("=== SE_ItemViewItemText rect probe (Qt5, for comparison with Qt6) ===\n");
    printf("diff = itemWidth - textRect.width()  (positive = pixels stolen from text rect)\n\n");

    for (int w : widths) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) continue;

            QStyleOptionViewItem opt;
            opt.initFrom(new QWidget());
            opt.rect = QRect(0, 0, w, 24);
            opt.text = "Keyboard and mouse shortcuts";
            opt.features = QStyleOptionViewItem::HasDisplay;
            opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
            opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
            opt.state = QStyle::State_Enabled;
            opt.decorationSize = QSize(0, 0);

            QRect textRect = s->subElementRect(QStyle::SE_ItemViewItemText, &opt, nullptr);

            printf("[%-14s]  itemWidth=%3d  textRect=(%d,%d %dx%d)  textWidth=%3d  diff=%d\n",
                   name.toUtf8().constData(),
                   w,
                   textRect.x(), textRect.y(),
                   textRect.width(), textRect.height(),
                   textRect.width(),
                   w - textRect.width());
            fflush(stdout);
            delete s;
        }
        printf("\n");
    }

    return 0;  // no event loop needed — probe only, no window
}
