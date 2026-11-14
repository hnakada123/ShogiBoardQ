/// @file tst_dock_drag.cpp
/// @brief ドックのドラッグの知らせ（DockDragTracker）と、ドラッグの間の主画面の横幅のテスト
///
/// 主画面の横幅は、盤とドックの配置に必要な最小の幅に固定している。Qt は、ドックを入れた配置が
/// 今のウィンドウに収まる場所だけをドロップ先にするので、何もしなければ、切り離したドックを
/// 盤の横へドラッグで戻せない。

#include <QtTest>
#include <QDockWidget>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QResizeEvent>
#include <QSignalSpy>
#include <QStyle>
#include <QWindow>

#include "dockregistry.h"
#include "mainwindowappearancecontroller.h"
#include "shogiview.h"

/// ウィンドウが受けた大きさの変更の、幅の列
class WidthRecorder : public QObject
{
public:
    QList<int> widths;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Resize) widths.append(static_cast<QResizeEvent*>(event)->size().width());
        return QObject::eventFilter(watched, event);
    }
};

class TestDockDrag : public QObject
{
    Q_OBJECT

private:
    using Id = DockRegistry::DockId;

    /// 盤（固定の大きさ）と、右の棋譜・下の思考のドックを持つ主画面
    struct Setup {
        QMainWindow window;
        ShogiView* view = nullptr;
        MainWindowAppearanceController* appearance = nullptr;
        DockRegistry* registry = nullptr;
        QDockWidget* record = nullptr;
        QDockWidget* thinking = nullptr;

        Setup()
        {
            window.setDockNestingEnabled(true);
            auto* central = new QWidget;
            window.setCentralWidget(central);
            view = new ShogiView;
            MainWindowAppearanceController::Deps ad;
            ad.window = &window;
            ad.centralWidget = central;
            ad.shogiView = view;
            appearance = new MainWindowAppearanceController(ad, &window);

            registry = new DockRegistry(&window, nullptr, &window);
            DockRegistry::Contents contents{};
            auto* recordContent = new QWidget(&window);
            recordContent->setMinimumSize(kRecordWidth, 200);
            auto* thinkingContent = new QWidget(&window);
            thinkingContent->setMinimumSize(100, 100);
            contents.at(static_cast<std::size_t>(Id::Record)) = recordContent;
            contents.at(static_cast<std::size_t>(Id::Thinking)) = thinkingContent;
            registry->createAll(contents);
            record = registry->dock(Id::Record);
            thinking = registry->dock(Id::Thinking);
            // 合成ルートと同じ接続
            QObject::connect(registry, &DockRegistry::dockDragStarted, appearance,
                             &MainWindowAppearanceController::beginDockDrag);
            QObject::connect(registry, &DockRegistry::dockDragFinished, appearance,
                             &MainWindowAppearanceController::endDockDrag);
        }

        /// 表示して、横幅が配置に合うまで待つ
        bool showAndSettle()
        {
            window.resize(window.sizeHint().width(), 900);
            window.show();
            return QTest::qWaitForWindowExposed(&window) && QTest::qWaitFor([this] { return isFitted(); });
        }

        int neededWidth() const { return window.layout()->minimumSize().width(); }
        /// 横幅が、今の配置に必要な幅に固定されているか
        bool isFitted() const
        {
            return window.width() == neededWidth() && window.minimumWidth() == window.maximumWidth()
                   && window.minimumWidth() == neededWidth();
        }
        /// 主画面の中の位置（主画面の座標）へ、ドラッグ中のドックのマウスを動かす
        void moveDragTo(QDockWidget* dock, const QPoint& inWindow) const
        {
            QTest::mouseMove(dock, dock->mapFromGlobal(window.mapToGlobal(inWindow)));
        }
        void releaseDragAt(QDockWidget* dock, const QPoint& inWindow) const
        {
            QTest::mouseRelease(dock, Qt::LeftButton, Qt::NoModifier,
                                dock->mapFromGlobal(window.mapToGlobal(inWindow)));
        }
        /// 盤の段の高さ（主画面の座標。ドラッグの前に読む: ドラッグ中は Qt がドロップ先の分だけ配置をずらす）
        int boardRow() const { return window.centralWidget()->geometry().center().y(); }
        /// 盤の右隣（右のドックの場所が空のとき、Qt がドロップ先にする右端の帯）
        QPoint besideBoardRight(int row) const { return QPoint(window.width() - 20, row); }
    };

    static constexpr int kRecordWidth = 320;

    /// ドックのタイトルバーの中の位置（ドックの座標）
    static QPoint titlePos(const QDockWidget* dock)
    {
        return QPoint(dock->width() / 3, dock->widget()->geometry().top() / 2);
    }

    /// 浮動のときも Qt が描くタイトルバーを使うようにする。offscreen では、浮動のドックのタイトルバーを
    /// OS が描く扱いになり、マウスのイベントでは動かせない（Linux の X11・Wayland は Qt が描く）
    static void useQtTitleBar(QDockWidget* dock)
    {
        auto* bar = new QLabel(dock->windowTitle(), dock);
        bar->setMinimumHeight(24);
        dock->setTitleBarWidget(bar);
    }

private slots:
    // --- ドラッグの知らせ ---

    void dragOfDockedDockIsReported()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        QSignalSpy started(s.registry, &DockRegistry::dockDragStarted);
        QSignalSpy finished(s.registry, &DockRegistry::dockDragFinished);

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QCOMPARE(started.count(), 0);
        // 引き出す距離に届かない動きでは始まらない
        QTest::mouseMove(s.record, press + QPoint(1, 1));
        QCOMPARE(started.count(), 0);
        QVERIFY(!s.record->isFloating());

        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QCOMPARE(started.count(), 1);
        QCOMPARE(started.first().first().value<QDockWidget*>(), s.record);
        QVERIFY(s.record->isFloating());
        QCOMPARE(finished.count(), 0);

        // 動かしている間は、始まりを繰り返し知らせない
        QTest::mouseMove(s.record, titlePos(s.record) + QPoint(30, 30));
        QCOMPARE(started.count(), 1);
        QCOMPARE(finished.count(), 0);

        QTest::mouseRelease(s.record, Qt::LeftButton, Qt::NoModifier, titlePos(s.record));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(started.count(), 1);
    }

    void clickOnTitleIsNotADrag()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        QSignalSpy started(s.registry, &DockRegistry::dockDragStarted);
        QSignalSpy finished(s.registry, &DockRegistry::dockDragFinished);
        QTest::mouseClick(s.record, Qt::LeftButton, Qt::NoModifier, titlePos(s.record));
        QCOMPARE(started.count(), 0);
        QCOMPARE(finished.count(), 0);
    }

    void dragsThatCannotDockAreNotReported_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("ctrl") << QStringLiteral("ctrl");      // Ctrl を押したドラッグはドッキングしない
        QTest::newRow("locked") << QStringLiteral("locked");  // ドックを固定している
        QTest::newRow("content") << QStringLiteral("content");  // 中身の上で押した
    }

    void dragsThatCannotDockAreNotReported()
    {
        QFETCH(QString, kind);
        Setup s;
        QVERIFY(s.showAndSettle());
        QSignalSpy started(s.registry, &DockRegistry::dockDragStarted);

        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        QPoint press = titlePos(s.record);
        if (kind == QLatin1String("ctrl")) modifiers = Qt::ControlModifier;
        if (kind == QLatin1String("locked")) {
            s.record->setFeatures(QDockWidget::DockWidgetClosable);
            s.record->setAllowedAreas(Qt::NoDockWidgetArea);
        }
        if (kind == QLatin1String("content")) press = s.record->widget()->geometry().center();

        QTest::mousePress(s.record, Qt::LeftButton, modifiers, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QTest::mouseMove(s.record, s.record->mapFromGlobal(s.window.mapToGlobal(QPoint(60, 60))));
        QTest::mouseRelease(s.record, Qt::LeftButton, modifiers, QPoint(10, 10));
        QCOMPARE(started.count(), 0);
        // 横幅の固定は続いている
        QTRY_VERIFY(s.isFitted());
    }

    // --- ドラッグの間の横幅 ---

    /// 収まっているドックを引き出す間は横幅を変えず、離してから縮める
    /// （Qt はドラッグの間のウィンドウの大きさの変更を配置に反映しない。途中で縮めると中身が切れる）
    void widthIsKeptWhileDraggingDockOut()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        const int dockedWidth = s.window.width();

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QVERIFY(s.record->isFloating());
        // 本体の下（どのドロップ先でもない所）へ運ぶ
        const QPoint outside(s.window.width() / 2, s.window.height() + 300);
        s.moveDragTo(s.record, outside);
        QTest::qWait(50);  // 配置のやり直しの依頼を処理させる
        QCOMPARE(s.window.width(), dockedWidth);
        QCOMPARE(s.thinking->width(), dockedWidth);

        s.releaseDragAt(s.record, outside);
        QVERIFY(s.record->isFloating());
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        // 中の配置も縮んだ幅に合っている
        QTRY_COMPARE(s.thinking->width(), s.window.width());
    }

    /// 浮動のドックを、盤の横へドラッグで戻せる
    void floatingDockCanBeDroppedBesideBoard()
    {
        Setup s;
        useQtTitleBar(s.record);
        QVERIFY(s.showAndSettle());
        const int dockedWidth = s.window.width();
        s.record->setFloating(true);
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        const int boardWidth = s.window.width();
        const int row = s.boardRow();

        // つかむと、ドックを入れられるだけ広がる
        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QCOMPARE(s.window.width(), boardWidth);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        const int separator = s.window.style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent, nullptr, &s.window);
        QVERIFY2(s.window.width() >= boardWidth + separator + kRecordWidth, qPrintable(QString::number(s.window.width())));
        const int widened = s.window.width();

        // 盤の右隣へ運んで離す
        s.moveDragTo(s.record, s.besideBoardRight(row));
        QTest::qWait(50);
        QCOMPARE(s.window.width(), widened);
        s.releaseDragAt(s.record, s.besideBoardRight(row));

        QTRY_VERIFY(!s.record->isFloating());
        QCOMPARE(s.window.dockWidgetArea(s.record), Qt::RightDockWidgetArea);
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.width(), dockedWidth);
        // 盤の右に、盤と同じ高さで並ぶ（下のドックは全幅）
        QTRY_COMPARE(s.record->geometry().left() > s.window.centralWidget()->geometry().right(), true);
        QCOMPARE(s.thinking->width(), s.window.width());
        QVERIFY(s.window.isAnimated());
    }

    /// 浮動のドックを動かして、どこにも入れずに離したら、元の横幅に戻る
    void floatingDockDroppedOutsideRestoresWidth()
    {
        Setup s;
        useQtTitleBar(s.record);
        QVERIFY(s.showAndSettle());
        s.record->setFloating(true);
        QTRY_VERIFY(s.isFitted());
        const int boardWidth = s.window.width();

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QVERIFY(s.window.width() > boardWidth);
        const QPoint outside(s.window.width() / 2, s.window.height() + 300);
        s.moveDragTo(s.record, outside);
        s.releaseDragAt(s.record, outside);

        QVERIFY(s.record->isFloating());
        QTRY_COMPARE(s.window.width(), boardWidth);
        QTRY_VERIFY(s.isFitted());
        QTRY_COMPARE(s.thinking->width(), boardWidth);
        QVERIFY(s.window.isAnimated());
    }

    /// 固定した横幅と違う幅にされたら（Wayland のコンポジターが古い大きさで戻す）、合わせ直す
    void widthImposedFromOutsideIsCorrected()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        const int fitted = s.window.width();
        QWindow* handle = s.window.windowHandle();
        QVERIFY(handle);
        WidthRecorder recorder;
        s.window.installEventFilter(&recorder);
        handle->resize(fitted + 60, s.window.height());
        // 外からの幅がいったん届いてから、固定した幅に戻る
        QTRY_VERIFY(recorder.widths.contains(fitted + 60));
        QTRY_COMPARE(s.window.width(), fitted);
        QVERIFY(s.isFitted());
    }
};

QTEST_MAIN(TestDockDrag)
#include "tst_dock_drag.moc"
