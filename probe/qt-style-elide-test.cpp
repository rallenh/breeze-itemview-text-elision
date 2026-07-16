// qt-style-elide-test.cpp
// Tests whether Qt widget styles elide text inside fixed-size buttons,
// and prints SE_ItemViewItemText rect measurements per style for comparison.
//
// Build:
//   g++ -o qt-style-elide-test qt-style-elide-test.cpp \
//       $(pkg-config --cflags --libs Qt6Widgets) -fPIC
//
// Run (uses current platform theme — i.e. Breeze in a KDE session):
//   ./qt-style-elide-test
//
// Run forcing a specific style:
//   ./qt-style-elide-test fusion
//   ./qt-style-elide-test breeze
//   ./qt-style-elide-test windows
//
// SE_ItemViewItemText rect probe runs at startup and prints to stdout.

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <cstdio>

// Probe SE_ItemViewItemText for a given style at a given item rect width.
// Prints the rect Breeze/Fusion/etc actually hand to the text painter.
static void probeItemViewTextRect(const QString &styleName, QStyle *style, int itemWidth)
{
    // Build a realistic QStyleOptionViewItem matching a sidebar list item
    QStyleOptionViewItem opt;
    opt.initFrom(new QWidget());           // baseline palette/font/state
    opt.rect = QRect(0, 0, itemWidth, 24); // item geometry
    opt.text = "Keyboard and mouse shortcuts";
    opt.features = QStyleOptionViewItem::HasDisplay;
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    opt.state = QStyle::State_Enabled;
    opt.decorationSize = QSize(0, 0);

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, nullptr);

    printf("[%-14s]  itemWidth=%3d  textRect=(%d,%d %dx%d)  textWidth=%3d  diff=%d\n",
           styleName.toUtf8().constData(),
           itemWidth,
           textRect.x(), textRect.y(),
           textRect.width(), textRect.height(),
           textRect.width(),
           itemWidth - textRect.width());
    fflush(stdout);
}

static void runRectProbe()
{
    QStringList styleNames = {"breeze", "Fusion", "Windows"};
    QList<int> widths = {150, 120, 100, 80};  // typical sidebar column widths

    printf("\n=== SE_ItemViewItemText rect probe ===\n");
    printf("diff = itemWidth - textRect.width()  (positive = pixels stolen from text)\n\n");
    fflush(stdout);

    for (int w : widths) {
        for (const QString &name : styleNames) {
            QStyle *s = QStyleFactory::create(name);
            if (!s) { qDebug() << "  (style not found:" << name << ")"; continue; }
            probeItemViewTextRect(name, s, w);
            delete s;
        }
        printf("\n");
    }
}

static QWidget *makeColumn(const QString &styleName)
{
    QStyle *style = nullptr;
    if (!styleName.isEmpty()) {
        style = QStyleFactory::create(styleName);
        if (!style) {
            qWarning() << "Style not found:" << styleName;
        }
    }

    QWidget *col = new QWidget;
    QVBoxLayout *vl = new QVBoxLayout(col);
    vl->setSpacing(8);

    QString label = styleName.isEmpty()
        ? QString("Platform default\n(%1)").arg(QApplication::style()->objectName())
        : styleName;
    vl->addWidget(new QLabel("<b>" + label + "</b>"));

    // --- QPushButton fixed to 96x96 (KeePassXC sidebar button size) ---
    {
        QPushButton *btn = new QPushButton("Longer Label Text");
        btn->setFixedSize(96, 96);
        if (style) btn->setStyle(style);
        vl->addWidget(btn);
        vl->addWidget(new QLabel("QPushButton 96x96"));
    }

    // --- QToolButton with text under icon ---
    {
        QToolButton *tb = new QToolButton;
        tb->setText("Longer Label Text");
        tb->setFixedSize(96, 96);
        tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
        tb->setAutoRaise(true);
        if (style) tb->setStyle(style);
        vl->addWidget(tb);
        vl->addWidget(new QLabel("QToolButton 96x96\n(TextOnly, autoRaise)"));
    }

    // --- QToolButton text under icon with word wrap OFF (common pattern) ---
    {
        QToolButton *tb = new QToolButton;
        tb->setText("Longer Label Text");
        tb->setFixedSize(96, 96);
        tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tb->setAutoRaise(true);
        if (style) tb->setStyle(style);
        vl->addWidget(tb);
        vl->addWidget(new QLabel("QToolButton 96x96\n(TextUnderIcon, autoRaise)"));
    }

    // --- Narrower 80px variant ---
    {
        QToolButton *tb = new QToolButton;
        tb->setText("Longer Label Text");
        tb->setFixedSize(80, 80);
        tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
        tb->setAutoRaise(true);
        if (style) tb->setStyle(style);
        vl->addWidget(tb);
        vl->addWidget(new QLabel("QToolButton 80x80\n(TextOnly, autoRaise)"));
    }

    vl->addStretch();
    return col;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qDebug() << "Available styles:" << QStyleFactory::keys();
    qDebug() << "Active style:" << app.style()->objectName();

    runRectProbe();

    // If a style name was passed on the command line, show only that style
    // alongside the platform default. Otherwise show platform default + Fusion.
    QStringList stylesToShow;
    stylesToShow << QString(); // always show platform default first

    if (argc > 1) {
        stylesToShow << QString::fromLocal8Bit(argv[1]);
    } else {
        // default comparison: platform default vs Fusion
        if (app.style()->objectName().toLower() != "fusion")
            stylesToShow << "Fusion";
    }

    QWidget window;
    window.setWindowTitle("Qt style elide test — 96×96 sidebar buttons");
    QHBoxLayout *hl = new QHBoxLayout(&window);
    hl->setSpacing(24);

    for (const QString &name : stylesToShow) {
        hl->addWidget(makeColumn(name));
        if (&name != &stylesToShow.last()) {
            QFrame *sep = new QFrame;
            sep->setFrameShape(QFrame::VLine);
            sep->setFrameShadow(QFrame::Sunken);
            hl->addWidget(sep);
        }
    }

    window.show();
    return app.exec();
}
