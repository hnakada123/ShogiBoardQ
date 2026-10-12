#include <QtTest>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionViewItem>

#include "applicationstyle.h"

/// アプリ全体のスタイル（Fusion を包み、ドックのボタンのアイコンを使い回す）が、
/// 見た目を変えずにアイコンを使い回すこと。
class TestApplicationStyle : public QObject
{
    Q_OBJECT

    static QList<QStyle::StandardPixmap> cachedPixmaps()
    {
        return {QStyle::SP_TitleBarNormalButton, QStyle::SP_TitleBarMinButton, QStyle::SP_TitleBarCloseButton,
                QStyle::SP_DockWidgetCloseButton};
    }

    /// ドック2つ（1つは切り離す）を持つウィンドウを描いた絵
    static QList<QImage> renderDocks()
    {
        QMainWindow window;
        window.setCentralWidget(new QLabel(QStringLiteral("盤")));
        QList<QDockWidget*> docks;
        for (int i = 0; i < 2; ++i) {
            auto* dock = new QDockWidget(QStringLiteral("思考 %1").arg(i), &window);
            // アプリのドックと同じタイトルの指定
            dock->setStyleSheet(QStringLiteral("QDockWidget::title { padding: 6px 8px; background-color: #edf0f3; }"));
            dock->setWidget(new QLabel(QStringLiteral("中身 %1").arg(i)));
            window.addDockWidget(Qt::BottomDockWidgetArea, dock);
            docks.append(dock);
        }
        window.resize(640, 420);
        window.show();
        if (!QTest::qWaitForWindowExposed(&window)) return {};
        docks.at(1)->setFloating(true);
        docks.at(1)->resize(300, 160);
        for (int i = 0; i < 5; ++i) QCoreApplication::processEvents();
        return {window.grab().toImage(), docks.at(1)->grab().toImage()};
    }

private slots:
    void wrapsFusion()
    {
        std::unique_ptr<QStyle> style(ApplicationStyle::create());
        auto* proxy = qobject_cast<QProxyStyle*>(style.get());
        QVERIFY(proxy != nullptr);
        QVERIFY(proxy->baseStyle()->inherits("QFusionStyle"));
    }

    // 2回目からは同じアイコンを返し、その絵は Fusion が毎回作るものと同じ
    void titleBarIconsAreReusedAndUnchanged()
    {
        std::unique_ptr<QStyle> style(ApplicationStyle::create());
        std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
        for (const QStyle::StandardPixmap pixmap : cachedPixmaps()) {
            const QIcon first = style->standardIcon(pixmap);
            const QIcon second = style->standardIcon(pixmap);
            QVERIFY(!first.isNull());
            QCOMPARE(second.cacheKey(), first.cacheKey());
            const QIcon reference = fusion->standardIcon(pixmap);
            QCOMPARE(first.availableSizes(), reference.availableSizes());
            for (const int size : {10, 12, 16, 20, 24, 32, 48, 64}) {
                for (const qreal ratio : {1.0, 1.25, 1.5, 2.0}) {
                    QCOMPARE(first.pixmap(QSize(size, size), ratio).toImage(),
                             reference.pixmap(QSize(size, size), ratio).toImage());
                }
            }
        }
        // ほかのアイコンは Fusion に任せる
        for (const QStyle::StandardPixmap pixmap : {QStyle::SP_DialogOkButton, QStyle::SP_ArrowUp, QStyle::SP_FileIcon}) {
            QCOMPARE(style->standardIcon(pixmap).pixmap(QSize(16, 16)).toImage(),
                     fusion->standardIcon(pixmap).pixmap(QSize(16, 16)).toImage());
        }
    }

    // 表や一覧の項目の各部（チェック・絵・文字・焦点枠）の位置が、Fusion がそのまま求めたものと同じ
    void itemViewRectsMatchFusion()
    {
        std::unique_ptr<QStyle> style(ApplicationStyle::create());
        std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::red);
        const QIcon icon(pixmap);
        QFont large;
        large.setPointSize(large.pointSize() + 8);
        const QStringList texts = {QString(), QStringLiteral("a"), QStringLiteral("▲７六歩(77)"),
                                   QStringLiteral("△同　歩(24)▲２三歩打△同　金(32)").repeated(6),
                                   QStringLiteral("一行目\n二行目"), QStringLiteral("😀 絵文字"),
                                   QStringLiteral("שלום עולם")};
        const QList<QStyle::SubElement> elements = {QStyle::SE_ItemViewItemCheckIndicator,
                                                    QStyle::SE_ItemViewItemDecoration,
                                                    QStyle::SE_ItemViewItemText,
                                                    QStyle::SE_ItemViewItemFocusRect};
        // 解像度の違う描画先（Fusion はチェックの大きさを書体の解像度で拡大する）
        QImage highDpi(10, 10, QImage::Format_ARGB32);
        highDpi.setDotsPerMeterX(5669);  // 144dpi
        highDpi.setDotsPerMeterY(5669);
        int compared = 0;
        // 2回目は覚えた位置を返すので、それも Fusion と同じであること。位置をずらしても同じ
        for (int pass = 0; pass < 2; ++pass)
        for (const QString& text : texts) {
            for (const auto position : {QStyleOptionViewItem::Left, QStyleOptionViewItem::Right,
                                        QStyleOptionViewItem::Top, QStyleOptionViewItem::Bottom}) {
                for (int variant = 0; variant < 32; ++variant) {
                    QStyleOptionViewItem option;
                    option.rect = variant & 1 ? QRect(10, 20, 300, 60) : QRect(0, 0, 40, 24);
                    option.rect.translate(pass * 37, pass * 11);
                    option.text = text;
                    option.features = QStyleOptionViewItem::HasDisplay;
                    if (variant & 2) {
                        option.features |= QStyleOptionViewItem::HasDecoration;
                        option.icon = icon;
                        option.decorationSize = QSize(16, 16);
                    }
                    if (variant & 4) option.features |= QStyleOptionViewItem::HasCheckIndicator;
                    if (variant & 8) option.features |= QStyleOptionViewItem::WrapText;
                    option.direction = variant & 1 ? Qt::RightToLeft : Qt::LeftToRight;
                    option.font = variant & 2 ? large : QFont();
                    option.fontMetrics = variant & 16 ? QFontMetrics(option.font, &highDpi)
                                                      : QFontMetrics(option.font);
                    option.decorationPosition = position;
                    option.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
                    option.decorationAlignment = Qt::AlignCenter;
                    for (const QStyle::SubElement element : elements) {
                        QVERIFY2(style->subElementRect(element, &option, nullptr)
                                     == fusion->subElementRect(element, &option, nullptr),
                                 qPrintable(QStringLiteral("pass=%5 text=%1 position=%2 variant=%3 element=%4")
                                                .arg(text).arg(int(position)).arg(variant).arg(int(element)).arg(pass)));
                        ++compared;
                    }
                }
            }
        }
        QVERIFY(compared > 4000);
    }

    // ドックのタイトル（閉じる・切り離すボタンを含む）の絵が、Fusion のままのときと同じ
    void dockTitleBarsLookTheSame()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        const QList<QImage> expected = renderDocks();
        QApplication::setStyle(ApplicationStyle::create());
        const QList<QImage> actual = renderDocks();
        QCOMPARE(actual.size(), 2);
        QCOMPARE(expected.size(), 2);
        QVERIFY2(actual.at(0) == expected.at(0), "docked");
        QVERIFY2(actual.at(1) == expected.at(1), "floating");
    }
};

QTEST_MAIN(TestApplicationStyle)
#include "tst_application_style.moc"
