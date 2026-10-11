#include <QtTest>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QProxyStyle>
#include <QStyleFactory>

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
