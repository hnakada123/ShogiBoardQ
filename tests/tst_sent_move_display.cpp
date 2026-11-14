/// @file tst_sent_move_display.cpp
/// @brief 通信対局・P2P対局で、送って確認を待っている手の表示のテスト
///
/// 通信対局・P2P対局は、相手・サーバーの確認を受けてから盤を動かす。その間、つまんだ駒を移動元へ戻して
/// 見せると、確認が届いたときに駒が移動元から移動先へ跳んで見える。確認待ちの間も駒を移動先に出し、
/// 盤（ShogiBoard）は確認を受けるまで変えないことを確かめる。

#include <QtTest>
#include <QImage>
#include <QTemporaryDir>

#include <memory>
#include <optional>

#include "appactivitystate.h"
#include "boardinteractioncontroller.h"
#include "boardmovecontroller.h"
#include "branchtreeeditcontroller.h"
#include "gameboard.h"
#include "gamechannel.h"
#include "gamechannels.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "operationpolicy.h"
#include "positioneditsession.h"
#include "shogiboard.h"
#include "shogiposition.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"

namespace {

/// 両方の駒台に持駒がある局面（先手番。角打ち・歩の突き・飛車で歩を取って成る手がある）
const QString kStartSfen =
    QStringLiteral("ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2P1P1P2/PPSP1P2P/2G1G2R1/LN2K2NL b B2Sbp 41");

/// 盤で指した手を相手へ送り、確認が届くまで盤に指さない窓口（通信対局・P2P対局と同じ）
class PendingChannel final : public GameChannel
{
public:
    enum class Mode {
        Pending,           ///< 送って確認を待つ
        Reject,            ///< 受け付けない
        ConfirmAtOnce,     ///< 送った呼び出しの中で確認が届く（盤に指してから「送った」を返す）
    };

    explicit PendingChannel(GameBoard* board) : m_board(board) {}

    MoveResult playMove(const UsiMove& move) override
    {
        if (mode == Mode::Reject || sent || !m_board->isLegal(move)) return MoveResult::Rejected;
        sent = move;
        if (mode == Mode::ConfirmAtOnce) confirm();
        return MoveResult::Sent;
    }
    void resign() override {}
    void breakOff() override {}
    void moveNow() override {}
    bool isInProgress() const override { return true; }
    bool isHumanToMove() const override { return !sent; }
    bool canResign() const override { return !sent; }
    Turn sideToMove() const override { return m_board->board()->currentPlayer(); }
    bool acceptsDeclaration() const override { return false; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override { return std::nullopt; }

    /// 相手の確認が届いた: 送った手を盤に指す
    bool confirm()
    {
        if (!sent) return false;
        const UsiMove move = *std::exchange(sent, std::nullopt);
        return m_board->applyMove(move, {}).has_value();
    }

    Mode mode = Mode::Pending;
    std::optional<UsiMove> sent;   ///< 確認を待っている手

private:
    GameBoard* m_board;
};

/// 主画面と同じ組み立て: 盤・盤の操作・盤の手の行き先（BoardMoveController）と、P2P対局中の窓口
struct Harness {
    KifuBranchTree tree;
    AppActivityState activity;
    GameBoard gameBoard;
    ShogiView view;
    QObject owner;   ///< 動作状態の持ち主
    ActivityLease lease;
    PendingChannel* channel = nullptr;
    std::unique_ptr<KifuNavigationState> state;
    std::unique_ptr<KifuNavigationController> nav;
    std::unique_ptr<LiveGameSession> live;
    std::unique_ptr<BoardInteractionController> interaction;
    std::unique_ptr<GameChannels> channels;
    std::unique_ptr<OperationPolicy> policy;
    std::unique_ptr<BranchTreeEditController> branchEdit;
    std::unique_ptr<PositionEditSession> positionEdit;
    std::unique_ptr<BoardMoveController> route;

    explicit Harness(bool flipped = false)
    {
        tree.setRootSfen(kStartSfen);
        state = std::make_unique<KifuNavigationState>(&tree);
        nav = std::make_unique<KifuNavigationController>(state.get());
        live = std::make_unique<LiveGameSession>(&tree, state.get());

        gameBoard.newGame(kStartSfen);
        view.setBoard(gameBoard.board());
        view.setFlipMode(flipped);
        view.setFixedSize(view.sizeHint());
        // 成り・不成の両方が合法なら成る
        interaction = std::make_unique<BoardInteractionController>(&view, gameBoard.board(), [] { return true; });

        channels = std::make_unique<GameChannels>(&activity, nullptr, nullptr);
        auto pending = std::make_unique<PendingChannel>(&gameBoard);
        channel = pending.get();
        channels->setP2pChannel(std::move(pending));

        OperationPolicy::Deps policyDeps;
        policyDeps.activity = &activity;
        policyDeps.channels = channels.get();
        policyDeps.liveSession = live.get();
        policy = std::make_unique<OperationPolicy>(policyDeps);

        BranchTreeEditController::Deps branchDeps;
        branchDeps.tree = &tree;
        branchDeps.navState = state.get();
        branchDeps.navController = nav.get();
        branchDeps.policy = policy.get();
        branchEdit = std::make_unique<BranchTreeEditController>(branchDeps);

        PositionEditSession::Deps editDeps;
        editDeps.view = &view;
        editDeps.board = interaction.get();
        editDeps.navState = state.get();
        editDeps.activity = &activity;
        positionEdit = std::make_unique<PositionEditSession>(editDeps);

        BoardMoveController::Deps routeDeps;
        routeDeps.gameBoard = &gameBoard;
        routeDeps.board = interaction.get();
        routeDeps.activity = &activity;
        routeDeps.positionEdit = positionEdit.get();
        routeDeps.branchEdit = branchEdit.get();
        routeDeps.operationPolicy = policy.get();
        routeDeps.channels = channels.get();
        route = std::make_unique<BoardMoveController>(routeDeps);

        // 直前の手（相手の△8五歩）を示した対局中の盤から始める
        interaction->showMoveHighlights(QPoint(8, 4), QPoint(8, 5));
        lease = activity.begin(AppActivityState::Activity::DuringP2pGame, &owner);
        view.show();
    }

    ShogiBoard* board() const { return gameBoard.board(); }
    QList<QPoint> highlights(BoardHighlightLayer layer) const { return view.highlighting()->highlights(layer); }
    QImage image() { return view.grab().toImage(); }

    /// 盤のクリック2回（移動元・移動先）で手を指す
    void click(const QPoint& from, const QPoint& to)
    {
        interaction->onLeftClick(from);
        QCoreApplication::processEvents();
        interaction->onLeftClick(to);
        QCoreApplication::processEvents();
    }
};

/// position の盤に、直前の手 from → to を示した絵（確認待ちの表示と比べる見本）
QImage referenceImage(const Position& position, const QPoint& from, const QPoint& to, bool flipped)
{
    ShogiBoard board;
    board.setPosition(position);
    ShogiView view;
    view.setBoard(&board);
    view.setFlipMode(flipped);
    view.setFixedSize(view.sizeHint());
    view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveFrom, {from});
    view.highlighting()->setHighlights(BoardHighlightLayer::LastMoveTo, {to});
    return view.grab().toImage();
}

} // namespace

class TestSentMoveDisplay : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void sentMoveStaysAtDestinationUntilConfirmed_data()
    {
        QTest::addColumn<QPoint>("from");
        QTest::addColumn<QPoint>("to");
        QTest::addColumn<QString>("usi");
        QTest::addColumn<bool>("flipped");
        // 駒台の角（筋10・段6）を打つ: 駒台の枚数が減る
        QTest::newRow("drop") << QPoint(10, 6) << QPoint(5, 5) << QStringLiteral("B*5e") << false;
        QTest::newRow("push") << QPoint(7, 6) << QPoint(7, 5) << QStringLiteral("7f7e") << false;
        // 飛車で歩を取って成る: 取った駒が消え、駒台の枚数が増え、成った駒になる
        QTest::newRow("capture and promote") << QPoint(2, 8) << QPoint(2, 3) << QStringLiteral("2h2c+") << false;
        QTest::newRow("flipped board") << QPoint(2, 8) << QPoint(2, 3) << QStringLiteral("2h2c+") << true;
    }

    void sentMoveStaysAtDestinationUntilConfirmed()
    {
        QFETCH(QPoint, from);
        QFETCH(QPoint, to);
        QFETCH(QString, usi);
        QFETCH(bool, flipped);

        Harness h(flipped);
        QVERIFY(QTest::qWaitForWindowExposed(&h.view));
        const Position before = h.board()->position();
        Position after = before;
        QVERIFY(after.apply(UsiMove::fromString(usi)));
        const QImage beforeImage = h.image();

        h.click(from, to);

        // 相手へ送り、確認を待つ。盤（局面の値）は確認を受けるまで動かさない
        QVERIFY(h.channel->sent.has_value());
        QCOMPARE(h.channel->sent->toString(), usi);
        QVERIFY(h.board()->position() == before);
        QVERIFY(!h.route->acceptsInput());

        // 表示は指した後の局面で、直前の手として示す（つまんだ駒を移動元へ戻さない）
        QVERIFY(h.view.hasPendingPosition());
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveFrom), QList<QPoint>{from});
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveTo), QList<QPoint>{to});
        QVERIFY(h.highlights(BoardHighlightLayer::Selection).isEmpty());
        QVERIFY(h.highlights(BoardHighlightLayer::LegalDestinations).isEmpty());
        // 駒・駒台・直前の手・手番の印のどれも、その手を指した後の盤と同じ絵になる
        const QImage pendingImage = h.image();
        QVERIFY(pendingImage != beforeImage);
        QVERIFY(pendingImage == referenceImage(after, from, to, flipped));
        // 手番の印も指した後の手番になる（指した側に付いたままの絵とは違う）
        Position moverToMove = after;
        moverToMove.setSideToMove(before.sideToMove());
        QVERIFY(pendingImage != referenceImage(moverToMove, from, to, flipped));

        // 確認が届く: 盤が動き、表示は何も変わらない
        QVERIFY(h.channel->confirm());
        QCoreApplication::processEvents();
        QVERIFY(h.board()->position() == after);
        QVERIFY(!h.view.hasPendingPosition());
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveFrom), QList<QPoint>{from});
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveTo), QList<QPoint>{to});
        QVERIFY(h.image() == pendingImage);
    }

    /// 確認が届かないまま対局が終わった（切断・時間切れなど）: 駒と直前の手の表示を元に戻す
    void sentMoveReturnsWhenGameEndsWithoutConfirmation()
    {
        Harness h;
        QVERIFY(QTest::qWaitForWindowExposed(&h.view));
        const Position before = h.board()->position();
        const QImage beforeImage = h.image();

        h.click(QPoint(2, 8), QPoint(2, 3));
        QVERIFY(h.view.hasPendingPosition());
        QVERIFY(h.image() != beforeImage);

        // 対局の終わりで動作状態が待機中に戻る
        h.lease.reset();
        QCoreApplication::processEvents();
        QVERIFY(!h.view.hasPendingPosition());
        QVERIFY(h.board()->position() == before);
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveFrom), QList<QPoint>{QPoint(8, 4)});
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveTo), QList<QPoint>{QPoint(8, 5)});
        QVERIFY(h.image() == beforeImage);
    }

    /// 受け付けられなかった手は、今までどおり駒を移動元へ戻す
    void rejectedMoveReturnsToSource()
    {
        Harness h;
        QVERIFY(QTest::qWaitForWindowExposed(&h.view));
        h.channel->mode = PendingChannel::Mode::Reject;
        const QImage beforeImage = h.image();

        h.click(QPoint(7, 6), QPoint(7, 5));
        QVERIFY(!h.channel->sent.has_value());
        QVERIFY(!h.view.hasPendingPosition());
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveTo), QList<QPoint>{QPoint(8, 5)});
        QVERIFY(h.image() == beforeImage);
    }

    /// 送った呼び出しの中で確認が届いて盤が進んでいたら、同じ手を重ねて表示しない
    void moveConfirmedAtOnceIsNotShownAgain()
    {
        Harness h;
        QVERIFY(QTest::qWaitForWindowExposed(&h.view));
        h.channel->mode = PendingChannel::Mode::ConfirmAtOnce;
        Position after = h.board()->position();
        QVERIFY(after.apply(UsiMove::fromString(u"7f7e")));

        h.click(QPoint(7, 6), QPoint(7, 5));
        QVERIFY(!h.view.hasPendingPosition());
        QVERIFY(h.board()->position() == after);
        // 表示は盤の局面そのもの
        const QImage shown = h.image();
        h.view.repaint();
        QVERIFY(shown == h.image());
        QVERIFY(h.highlights(BoardHighlightLayer::Selection).isEmpty());
    }

    /// 確認待ちの表示が残っていても、駒をつまむときは盤の局面に戻してからつまむ
    void pickingAPieceDropsTheSentMoveDisplay()
    {
        Harness h;
        QVERIFY(QTest::qWaitForWindowExposed(&h.view));
        const QImage beforeImage = h.image();
        h.click(QPoint(7, 6), QPoint(7, 5));
        QVERIFY(h.view.hasPendingPosition());

        // 確認を待たずに手を受け付ける状態になった（窓口が確認待ちをやめた）
        h.channel->sent.reset();
        h.interaction->onLeftClick(QPoint(7, 6));
        QVERIFY(!h.view.hasPendingPosition());
        QCOMPARE(h.highlights(BoardHighlightLayer::Selection), QList<QPoint>{QPoint(7, 6)});
        QCOMPARE(h.highlights(BoardHighlightLayer::LastMoveTo), QList<QPoint>{QPoint(8, 5)});
        // つまむのをやめれば、指す前の表示に戻る
        h.interaction->onLeftClick(QPoint(7, 6));
        QCoreApplication::processEvents();
        QVERIFY(h.image() == beforeImage);
    }
};

QTEST_MAIN(TestSentMoveDisplay)
#include "tst_sent_move_display.moc"
