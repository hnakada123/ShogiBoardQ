/// @file tst_dock_drag.cpp
/// @brief ドックのドラッグの知らせ（DockDragTracker）と、ドラッグの間の主画面の大きさのテスト
///
/// 主画面の横幅は、盤とドックの配置に必要な最小の幅に固定している。Qt は、ドックを入れた配置が
/// 今のウィンドウに収まる場所だけをドロップ先にするので、何もしなければ、切り離したドックを
/// 盤の横へドラッグで戻せず、高さの足りない主画面では下段にも入れられない。ドラッグの間は
/// ウィンドウの大きさを変えず、盤の領域を縮めてよいことにして、ドロップ先を出させる。

#include <QtTest>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMimeData>
#include <QResizeEvent>
#include <QSignalSpy>
#include <QWindow>

#include "appsettings.h"
#include "dockregistry.h"
#include "mainwindowappearancecontroller.h"
#include "sfenutils.h"
#include "shogiboard.h"
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

/// どんなドラッグも受け付ける部品（分岐ツリー・評価値グラフの QGraphicsView の代わり）
class DropTarget : public QWidget
{
public:
    explicit DropTarget(QWidget* parent) : QWidget(parent) { setAcceptDrops(true); }
    int entered = 0;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        ++entered;
        event->acceptProposedAction();
    }
};

class TestDockDrag : public QObject
{
    Q_OBJECT

private:
    using Id = DockRegistry::DockId;

    /// 盤（固定の大きさ）と、右の棋譜・下の思考のドックを持つ主画面
    struct Setup {
        ShogiBoard board;
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
            // 実際の盤の大きさにする（ドックより大きい。盤なしの ShogiView は小さい）
            board.setSfen(SfenUtils::hirateSfen());
            view = new ShogiView;
            view->setBoard(&board);
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
        /// 盤の領域が、盤の大きさに固定されているか（ドラッグの間だけ縮められる）
        bool boardAreaIsFixed() const
        {
            const QWidget* central = window.centralWidget();
            return central->minimumSize() == view->sizeHint() && central->maximumSize() == view->sizeHint()
                   && central->size() == view->sizeHint();
        }
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
        /// 主画面を、今の配置に必要な最小の高さにする
        bool shrinkToMinimumHeight()
        {
            window.resize(window.width(), window.minimumHeight());
            return QTest::qWaitFor([this] { return window.height() == window.minimumHeight() && isFitted(); });
        }
        /// 本体の外（どのドロップ先でもない所。主画面の座標）
        QPoint outside() const { return QPoint(window.width() / 2, window.height() + 300); }
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

    /// OS が描くタイトルバーの中の位置（ドックの座標。中身のすぐ上なので負の y。offscreen の枠は細い）
    static QPoint nativeTitlePos() { return QPoint(40, -1); }

    /// OS のタイトルバーでつかんだドラッグを Qt が終えるイベント（QDockWidgetPrivate::nonClientAreaMouseEvent）
    static QEvent::Type nativeTitleDragEnd()
    {
#if defined(Q_OS_MACOS) || defined(Q_OS_WASM) || defined(Q_OS_HARMONY)
        return QEvent::NonClientAreaMouseButtonRelease;
#else
        return QEvent::NonClientAreaMouseMove;
#endif
    }

    /// ウィンドウの枠のマウスのイベントを、浮動のドックへ送る
    static void sendNativeTitleEvent(QDockWidget* dock, QEvent::Type type, const QPoint& pos,
                                     Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        const bool move = type == QEvent::NonClientAreaMouseMove;
        const bool press = type == QEvent::NonClientAreaMouseButtonPress;
        QMouseEvent event(type, QPointF(pos), QPointF(dock->mapToGlobal(pos)), move ? Qt::NoButton : Qt::LeftButton,
                          press ? Qt::LeftButton : Qt::NoButton, modifiers);
        QApplication::sendEvent(dock, &event);
    }

private slots:
    /// 盤の大きさは保存した値から始まるので、既定に戻しておく（盤を拡大するテストが値を保存する）
    void init() { AppSettings::squareSize.set(AppSettings::squareSize.defaultValue()); }

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

    /// ドックのドラッグの間は、ドロップを受け付ける子の部品に DragEnter を受けさせない。
    /// Wayland では Qt がドックのドラッグを OS のドラッグ＆ドロップで行い、メインウィンドウが受けた知らせから
    /// ドロップ先を決めるので、子の部品が先に受けると、その上ではドッキングできない
    void dragEnterSkipsChildrenWhileDraggingDock()
    {
        Setup s;
        auto* target = new DropTarget(s.thinking->widget());
        target->setGeometry(0, 0, 60, 60);
        QVERIFY(s.showAndSettle());
        QMimeData mime;
        mime.setText(QStringLiteral("dock"));
        const auto enter = [&mime, target] {
            QDragEnterEvent event(QPointF(5, 5), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
            return event.isAccepted();
        };
        // ふだんは子の部品が受ける
        QVERIFY(enter());
        QCOMPARE(target->entered, 1);

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QVERIFY(s.record->isFloating());
        // ドラッグの間は子の部品を飛ばし、親（メインウィンドウ）まで渡る
        enter();
        QCOMPARE(target->entered, 1);

        QTest::mouseRelease(s.record, Qt::LeftButton, Qt::NoModifier, titlePos(s.record));
        QVERIFY(enter());
        QCOMPARE(target->entered, 2);
    }

    // --- ドラッグの間の主画面の大きさ ---

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
        s.moveDragTo(s.record, s.outside());
        QTest::qWait(50);  // 配置のやり直しの依頼を処理させる
        QCOMPARE(s.window.width(), dockedWidth);
        QCOMPARE(s.thinking->width(), dockedWidth);

        s.releaseDragAt(s.record, s.outside());
        QVERIFY(s.record->isFloating());
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        // 中の配置も縮んだ幅に合っている
        QTRY_COMPARE(s.thinking->width(), s.window.width());
        QVERIFY(s.boardAreaIsFixed());
    }

    /// 浮動のドックを、盤の横へドラッグで戻せる。ドラッグの間はウィンドウの大きさを変えない
    /// （Wayland では、大きさが変わったウィンドウをコンポジターが動かし、ドロップ先がカーソルとずれる）
    void floatingDockCanBeDroppedBesideBoard()
    {
        Setup s;
        useQtTitleBar(s.record);
        QVERIFY(s.showAndSettle());
        const int dockedWidth = s.window.width();
        s.record->setFloating(true);
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        const QSize boardOnly = s.window.size();
        const int row = s.boardRow();

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        QCOMPARE(s.window.size(), boardOnly);

        // 盤の右隣（盤しかない主画面の右端）へ運ぶと、盤の領域を縮めてドロップ先を出す
        s.moveDragTo(s.record, s.besideBoardRight(row));
        QTest::qWait(50);
        QCOMPARE(s.window.size(), boardOnly);
        QVERIFY(s.window.centralWidget()->width() < s.view->sizeHint().width());
        s.releaseDragAt(s.record, s.besideBoardRight(row));

        // 離すと盤の右に入り、主画面が必要な幅へ広がる
        QTRY_VERIFY(!s.record->isFloating());
        QCOMPARE(s.window.dockWidgetArea(s.record), Qt::RightDockWidgetArea);
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.width(), dockedWidth);
        QVERIFY(s.boardAreaIsFixed());
        // 盤の右に、盤と同じ高さで並ぶ（下のドックは全幅）
        QTRY_VERIFY(s.record->geometry().left() > s.window.centralWidget()->geometry().right());
        QCOMPARE(s.thinking->width(), s.window.width());
        QCOMPARE(s.window.height(), boardOnly.height());
        QVERIFY(s.window.isAnimated());
    }

    /// OS が描くタイトルバー（Windows・macOS の浮動のドック）でつかんだドラッグも知らせる。
    /// Qt はタイトルバーを押した時点でドラッグを始めるので、押した時点で始まりを知らせる
    void dragByNativeTitleBarIsReported()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        s.record->setFloating(true);
        QTRY_VERIFY(s.isFitted());
        QSignalSpy started(s.registry, &DockRegistry::dockDragStarted);
        QSignalSpy finished(s.registry, &DockRegistry::dockDragFinished);

        // 中身の上（枠の下の辺など）で押しても始まらない
        sendNativeTitleEvent(s.record, QEvent::NonClientAreaMouseButtonPress, QPoint(40, s.record->height() + 2));
        QCOMPARE(started.count(), 0);
        // Ctrl を押したドラッグはドッキングしない
        sendNativeTitleEvent(s.record, QEvent::NonClientAreaMouseButtonPress, nativeTitlePos(), Qt::ControlModifier);
        QCOMPARE(started.count(), 0);
        sendNativeTitleEvent(s.record, nativeTitleDragEnd(), nativeTitlePos());
        QCOMPARE(finished.count(), 0);

        sendNativeTitleEvent(s.record, QEvent::NonClientAreaMouseButtonPress, nativeTitlePos());
        QCOMPARE(started.count(), 1);
        QCOMPARE(started.first().first().value<QDockWidget*>(), s.record);
        QCOMPARE(finished.count(), 0);
        QVERIFY(!s.boardAreaIsFixed());
        sendNativeTitleEvent(s.record, nativeTitleDragEnd(), nativeTitlePos());
        QCOMPARE(finished.count(), 1);
        QTRY_VERIFY(s.boardAreaIsFixed());
        QTRY_VERIFY(s.isFitted());
    }

    /// OS が描くタイトルバーでつかんだ浮動のドックを、盤の横へ戻せる（macOS・Windows）
    void floatingDockWithNativeTitleBarCanBeDroppedBesideBoard()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        const int dockedWidth = s.window.width();
        s.record->setFloating(true);
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        // Qt は、ウィンドウの枠の上の辺（タイトルバー）で押したときだけドラッグを始める
        if (s.record->frameGeometry().top() >= s.record->geometry().top())
            QSKIP("浮動のドックに OS のタイトルバーが付かない環境");
        const QSize boardOnly = s.window.size();
        const int row = s.boardRow();

        const QPoint press = nativeTitlePos();
        sendNativeTitleEvent(s.record, QEvent::NonClientAreaMouseButtonPress, press);
        // OS がウィンドウを動かすと、Qt は押した位置から求めたマウスの位置でドロップ先を探す
        const QPoint target = s.window.mapToGlobal(s.besideBoardRight(row));
        const QPoint clientPos = target - press;
        s.record->move(s.record->pos() + clientPos - s.record->geometry().topLeft());
        QTRY_COMPARE(s.record->geometry().topLeft(), clientPos);
        QTRY_VERIFY(s.window.centralWidget()->width() < s.view->sizeHint().width());
        QCOMPARE(s.window.size(), boardOnly);
        sendNativeTitleEvent(s.record, nativeTitleDragEnd(), press);

        QTRY_VERIFY(!s.record->isFloating());
        QCOMPARE(s.window.dockWidgetArea(s.record), Qt::RightDockWidgetArea);
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.width(), dockedWidth);
        QVERIFY(s.boardAreaIsFixed());
        QTRY_VERIFY(s.record->geometry().left() > s.window.centralWidget()->geometry().right());
        QVERIFY(s.window.isAnimated());
    }

    /// 高さの足りない主画面でも、浮動のドックを下段のドックに重ねられる。
    /// 離すと、主画面が新しい配置に必要な高さへ広がる
    void floatingDockCanBeDroppedOnBottomDockOfShortWindow()
    {
        Setup s;
        useQtTitleBar(s.record);
        QVERIFY(s.showAndSettle());
        s.record->setFloating(true);
        QTRY_VERIFY(s.isFitted());
        // 今の配置にちょうどの高さにする（棋譜のドックは下段の思考のドックより高い）
        QVERIFY(s.shrinkToMinimumHeight());
        const QSize shortSize = s.window.size();
        QVERIFY(s.record->minimumSizeHint().height() > s.thinking->height());

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        // いったん本体の外へ出してから（Qt がドロップ先の分だけずらした配置を元に戻させる）、下段のドックの中央へ運ぶ
        s.moveDragTo(s.record, s.outside());
        const QPoint onThinking = s.thinking->geometry().center();
        s.moveDragTo(s.record, onThinking);
        QTest::qWait(50);
        QCOMPARE(s.window.size(), shortSize);
        s.releaseDragAt(s.record, onThinking);

        QTRY_VERIFY(!s.record->isFloating());
        QCOMPARE(s.window.dockWidgetArea(s.record), Qt::BottomDockWidgetArea);
        QCOMPARE(s.window.tabifiedDockWidgets(s.record), QList<QDockWidget*>{s.thinking});
        QTRY_VERIFY(s.window.height() > shortSize.height());
        QTRY_COMPARE(s.window.height(), s.window.minimumHeight());
        QTRY_VERIFY(s.isFitted());
        QVERIFY(s.boardAreaIsFixed());
    }

    /// 浮動のドックを動かして、どこにも入れずに離したら、主画面は元のまま（盤の領域も元の大きさ）
    void floatingDockDroppedOutsideLeavesWindowAsItWas()
    {
        Setup s;
        useQtTitleBar(s.record);
        QVERIFY(s.showAndSettle());
        s.record->setFloating(true);
        QTRY_VERIFY(s.isFitted());
        QVERIFY(s.shrinkToMinimumHeight());
        const QSize before = s.window.size();
        const int row = s.boardRow();

        const QPoint press = titlePos(s.record);
        QTest::mousePress(s.record, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(s.record, press + QPoint(40, 40));
        // ドロップ先の上を通っても、主画面の大きさは変わらない
        s.moveDragTo(s.record, s.besideBoardRight(row));
        QTest::qWait(50);
        QCOMPARE(s.window.size(), before);
        s.moveDragTo(s.record, s.outside());
        s.releaseDragAt(s.record, s.outside());

        QVERIFY(s.record->isFloating());
        QTRY_VERIFY(s.boardAreaIsFixed());
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.size(), before);
        QTRY_COMPARE(s.thinking->width(), before.width());
        QVERIFY(s.window.isAnimated());
    }

    /// 今の配置に高さが足りなくなったら（盤を大きくした等）、必要な高さにする。高さに余裕があれば変えない
    void heightFollowsLayoutOnlyWhenTooShort()
    {
        Setup s;
        QVERIFY(s.showAndSettle());
        const int tall = s.window.height();
        QVERIFY(tall > s.window.minimumHeight());
        s.view->enlargeBoard();
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.height(), tall);

        QVERIFY(s.shrinkToMinimumHeight());
        const int shortHeight = s.window.height();
        for (int i = 0; i < 5; ++i) s.view->enlargeBoard();
        QTRY_VERIFY(s.window.height() > shortHeight);
        QTRY_COMPARE(s.window.height(), s.window.minimumHeight());
        QTRY_VERIFY(s.isFitted());
    }

    /// macOS の最大化（ズーム）は、横幅を固定したまま高さだけを画面に合わせる。画面より高いウィンドウの幅を
    /// 変えただけでも最大化と報告されるので、最大化でも横幅を配置に合わせ続ける
    void widthFollowsLayoutWhileZoomedOnMac()
    {
#ifndef Q_OS_MACOS
        QSKIP("macOS の最大化（ズーム）の扱い");
#else
        Setup s;
        QVERIFY(s.showAndSettle());
        const int dockedWidth = s.window.width();
        s.window.setWindowState(Qt::WindowMaximized);
        QTRY_VERIFY(s.window.isMaximized());
        QTRY_VERIFY(s.isFitted());
        QCOMPARE(s.window.width(), dockedWidth);

        s.record->setFloating(true);
        QTRY_VERIFY(s.window.width() < dockedWidth);
        QTRY_VERIFY(s.isFitted());
        s.record->setFloating(false);
        QTRY_COMPARE(s.window.width(), dockedWidth);
        QTRY_VERIFY(s.isFitted());
        QTRY_VERIFY(s.record->geometry().right() < s.window.width());
#endif
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
