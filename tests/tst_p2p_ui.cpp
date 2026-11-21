/// @file tst_p2p_ui.cpp
/// @brief P2P対局の画面の試験（ウィンドウ・条件確認画面・プレイヤー鍵のダイアログを作り、表示と操作の可否を確かめる）
///
/// SBQ_P2P_UI_SHOTS にフォルダを指定すると、各画面の画像を保存する（見た目の確認用）。

#include <QtTest>
#include <QPlainTextEdit>
#include <QMimeData>
#include <QClipboard>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QWindow>

#include "appactivitystate.h"
#include "engineprofilestore.h"
#include "engineslots.h"
#include "gameboard.h"
#include "mainwindowappearancecontroller.h"
#include "networksettings.h"
#include "notificationpresenter.h"
#include "p2pconfirmdialog.h"
#include "p2pconnectionpanel.h"
#include "p2pfriendpresence.h"
#include "p2pgamecoordinator.h"
#include "p2pgameuicontroller.h"
#include "p2phistorypanel.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2pinvitepanel.h"
#include "p2pknownpeers.h"
#include "p2plobbyservice.h"
#include "p2ppeerspanel.h"
#include "p2pplayerprofile.h"
#include "p2pplayerslot.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "p2pwindow.h"
#include "p2pwindowtext.h"
#include "shogiclock.h"
#include "shogiview.h"

using namespace P2p;

namespace {

void saveShot(QWidget* widget, const QString& name)
{
    const QString dir = qEnvironmentVariable("SBQ_P2P_UI_SHOTS");
    if (dir.isEmpty()) return;
    QDir().mkpath(dir);
    widget->grab().save(QDir(dir).filePath(name + QStringLiteral(".png")));
}

/// 2つの司令塔をつなぐ窓口（フレームは次のイベントループで渡す）
class LoopTransport : public P2pTransport
{
public:
    QByteArray peerId;
    P2pGameCoordinator* other = nullptr;
    QByteArray localPeerId() const override { return peerId; }
    QByteArray networkId() const override { return P2p::networkId("sbq-local-1"); }
    bool sendFrame(quint64 connection, const QByteArray& frame) override
    {
        P2pGameCoordinator* target = other;
        QMetaObject::invokeMethod(target, [target, connection, frame]() { target->onFrame(connection, frame); },
                                  Qt::QueuedConnection);
        return true;
    }
    void closeConnection(quint64) override {}
    void setGamePeer(const QString&) override {}
    void reconnectGamePeer(const QString&) override {}
};

/// 対局する1人（盤・時計・司令塔と、主画面の盤とその向きを変える部品・P2P対局の画面）
struct Seat {
    explicit Seat(const QByteArray& peerId)
        : game(P2pGameCoordinator::Deps{&board, &clock, &activity})
        , view(new ShogiView)
    {
        transport.peerId = peerId;
        auto* central = new QWidget;
        window.setCentralWidget(central);
        MainWindowAppearanceController::Deps ad;
        ad.window = &window;
        ad.centralWidget = central;
        ad.shogiView = view;
        appearance = new MainWindowAppearanceController(ad, &window);
        P2pGameUiController::Deps ud;
        ud.game = &game;
        ud.activity = &activity;
        ud.parentWidget = &window;
        ud.shogiView = view;
        ud.appearance = appearance;
        ui = new P2pGameUiController(ud, &window);
    }

    GameBoard board;
    ShogiClock clock;
    AppActivityState activity;
    LoopTransport transport;
    SigningKey key = SigningKey::generate();
    P2pGameCoordinator game;
    QMainWindow window;
    ShogiView* view = nullptr;
    MainWindowAppearanceController* appearance = nullptr;
    P2pGameUiController* ui = nullptr;
};

P2pGameCoordinator::NegotiationRequest request(quint8 role, const QByteArray& opponentPeerId, const QString& name)
{
    P2pGameCoordinator::NegotiationRequest r;
    r.connection = 1;
    r.opponentPeerId = QString::fromLatin1(opponentPeerId.toHex());
    r.opponentPeerIdBytes = opponentPeerId;
    r.role = role;
    r.myDisplayName = name;
    r.hostTerms = TimeControlTerms{600'000, 30'000};
    return r;
}

/// 2人をつないで、条件確認から対局の開始まで進める（対局はジャーナルに書いてから始めるので、保存先が要る）
bool startGame(Seat& applicant, Seat& host, P2pRecordStore* applicantStore, P2pKnownPeers* applicantKnown,
               P2pRecordStore* hostStore, P2pKnownPeers* hostKnown)
{
    applicant.transport.other = &host.game;
    host.transport.other = &applicant.game;
    applicant.game.setEnvironment(&applicant.transport, applicantStore, applicantKnown, &applicant.key);
    host.game.setEnvironment(&host.transport, hostStore, hostKnown, &host.key);
    if (!host.game.startNegotiation(request(kRoleHost, applicant.transport.peerId, QStringLiteral("募集する人"))))
        return false;
    if (!applicant.game.startNegotiation(request(kRoleApplicant, host.transport.peerId, QStringLiteral("申し込む人"))))
        return false;
    if (!QTest::qWaitFor([&]() { return applicant.game.negotiation()
                                         && applicant.game.negotiation()->stage() == Negotiation::Stage::Confirming
                                         && host.game.negotiation()
                                         && host.game.negotiation()->stage() == Negotiation::Stage::Confirming; }))
        return false;
    host.game.confirmStart();
    applicant.game.confirmStart();
    return QTest::qWaitFor([&]() {
        return applicant.game.phase() == P2pGameCoordinator::Phase::Playing
               && host.game.phase() == P2pGameCoordinator::Phase::Playing;
    });
}

} // namespace

class TestP2pUi : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::initialize());
        QVERIFY(m_dir.isValid());
    }

    // 「対局中」の状態の文は1手ごとに替わる。文が替わっても、タブの部品（QTabWidget）は描き直さない
    // （文の入れ替えが配置のやり直しを親へ伝え、QTabWidget はそれを受けるたびに全体を描き直していた）
    void gameStatusChangeDoesNotRepaintTabs()
    {
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("tabs")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        P2pWindow window(&lobby, &game);
        // ウィンドウは1秒ごとに、状態の文を対局の状態から書き直す。この試験は対局を始めずに文をラベルへ直接
        // 書くので、途中で書き直されると（「対局していません。」の1行になる）、描いた回数とラベルの高さが変わる。
        // 文を書く前に止める（止めないと、ウィンドウを作ってから約1秒後に終わるこの試験は、時々落ちる）
        const QList<QTimer*> statusTimers = window.findChildren<QTimer*>(Qt::FindDirectChildrenOnly);
        QVERIFY(!statusTimers.isEmpty());
        for (QTimer* timer : statusTimers) timer->stop();
        window.resize(820, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("p2pTabs"));
        auto* status = window.findChild<QLabel*>(QStringLiteral("p2pGameStatus"));
        QVERIFY(tabs && status);
        tabs->setCurrentIndex(2);
        QCOMPARE(tabs->tabText(2), QStringLiteral("対局中"));
        QVERIFY(status->isVisible());
        const QString head = QStringLiteral("相手: 「テスト」\nあなたは先手\n経路: 直接\n");
        const QString tail = QStringLiteral("\n切断時間の残り: 180 秒");
        status->setText(head + QStringLiteral("あなたの手番です") + tail);
        for (int i = 0; i < 5; ++i) QCoreApplication::processEvents();

        struct PaintSpy : QObject {
            int paints = 0;
            QRegion region;
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if (event->type() == QEvent::Paint) {
                    ++paints;
                    region += static_cast<QPaintEvent*>(event)->region();
                }
                return QObject::eventFilter(watched, event);
            }
        } tabPaints, tabBarPaints, statusPaints;
        tabs->installEventFilter(&tabPaints);
        tabs->tabBar()->installEventFilter(&tabBarPaints);
        status->installEventFilter(&statusPaints);
        for (const QString& line : {QStringLiteral("着手の確認を待っています…"), QStringLiteral("相手の手番です"),
                                    QStringLiteral("あなたの手番です"), QStringLiteral("相手の手番です")}) {
            status->setText(head + line + tail);
            for (int i = 0; i < 5; ++i) QCoreApplication::processEvents();
        }
        QCOMPARE(statusPaints.paints, 4);
        // タブの部品が描くのは、文のラベルの下だけ（ラベルは不透明でないので、下の親もその範囲を描く）
        const QRect statusRect(status->mapTo(tabs, QPoint(0, 0)), status->size());
        QVERIFY2((tabPaints.region - statusRect).isEmpty(), "the tab widget repainted outside the status label");
        QCOMPARE(tabBarPaints.paints, 0);
        // 行が増えて大きさの条件が変わるときは、配置をやり直す（文が欠けない）
        const int heightBefore = status->height();
        status->setText(head + QStringLiteral("あなたの手番です") + tail + QStringLiteral("\n結果: 先手の勝ち（投了）\n確定しました"));
        for (int i = 0; i < 5; ++i) QCoreApplication::processEvents();
        QVERIFY(status->height() > heightBefore);
    }

    void windowStartsOffline()
    {
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("w")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        P2pWindow window(&lobby, &game);
        window.resize(820, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* tabs = window.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 6);
        // 接続していなければ招待はできない
        for (QPushButton* button : window.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("招待を作る") || button->text() == QStringLiteral("確かめる")
                || button->text() == QStringLiteral("接続する")) {
                QVERIFY2(!button->isEnabled(), qPrintable(button->text()));
            }
        }
        // 参加できるネットワークは「同じPC・同じLAN」と「インターネット」の2つ。既定は「同じPC・同じLAN」で直接つなぐ
        auto* lan = window.findChild<QRadioButton*>(QStringLiteral("p2pPlaceLan"));
        auto* internet = window.findChild<QRadioButton*>(QStringLiteral("p2pPlaceInternet"));
        QVERIFY(lan && internet);
        QVERIFY(lan->isChecked());
        QVERIFY(window.findChild<QRadioButton*>(QStringLiteral("p2pRouteDirect"))->isChecked());
        QCOMPARE(window.networkName(), QStringLiteral("sbq-local-1"));
        // 接続の状態と「接続」のボタンは、タブの上にあって、どのタブでも見える
        auto* connectButton = window.findChild<QPushButton*>(QStringLiteral("p2pConnect"));
        auto* statusTitle = window.findChild<QLabel*>(QStringLiteral("p2pStatus"));
        auto* routeInfo = window.findChild<QLabel*>(QStringLiteral("p2pRouteInfo"));
        QVERIFY(connectButton && statusTitle && routeInfo);
        QCOMPARE(connectButton->text(), QStringLiteral("接続"));
        QCOMPARE(statusTitle->text(), QStringLiteral("未接続"));
        QVERIFY2(routeInfo->text().contains(QStringLiteral("同じPC・同じLAN")), qPrintable(routeInfo->text()));
        for (int i = 0; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            QVERIFY2(connectButton->isVisible() && statusTitle->isVisible(), qPrintable(tabs->tabText(i)));
        }
        tabs->setCurrentIndex(0);
        // 対局していない間の「対局中」は、そのことと、対局の始め方を出す
        QCOMPARE(window.findChild<QLabel*>(QStringLiteral("p2pGameHeadline"))->text(), QStringLiteral("対局していません。"));
        // チェックとラジオボタンの文字は折り返せないので、短くする（長い説明は折り返すラベルに分ける。長いと、訳した
        // 画面でウィンドウの最小の幅が広がる）
        for (QAbstractButton* box : window.findChildren<QAbstractButton*>()) {
            if (!qobject_cast<QCheckBox*>(box) && !qobject_cast<QRadioButton*>(box)) continue;
            QVERIFY2(box->text().size() <= 30, qPrintable(box->text()));
        }
        // 公開募集は持たない（対局の相手は招待で決める）。自分のアドレスも画面に出さない
        QVERIFY(!window.findChild<QTableWidget*>(QStringLiteral("p2pOffers")));
        for (QLabel* label : window.findChildren<QLabel*>()) {
            QVERIFY2(!label->text().contains(QStringLiteral("/ip4/")) && !label->text().contains(QStringLiteral("/ip6/")),
                     qPrintable(label->text()));
        }
        saveShot(&window, QStringLiteral("p2p-window-play"));
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            saveShot(&window, QStringLiteral("p2p-window-tab%1").arg(i));
        }
        // 自動化 API を有効にしたときは使い始めない
        lobby.disableForAutomation();
        QCOMPARE(lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::AutomationEnabled);

        // 招待文字列のコピーには、クリップボードの履歴に残さない印を付け、期限が過ぎたら消す（監査 2026-11-13。
        // この試験は offscreen なので、利用者のクリップボードには触れない）
        auto* inviteOut = window.findChild<QPlainTextEdit*>(QStringLiteral("p2pInviteOut"));
        QVERIFY(inviteOut);
        inviteOut->setPlainText(QStringLiteral("sbqinv1.test"));
        auto* invitePanel = window.findChild<P2pInvitePanel*>();
        QVERIFY(invitePanel);
        QVERIFY(QMetaObject::invokeMethod(invitePanel, "onCopyInvite"));
        const QMimeData* copied = QApplication::clipboard()->mimeData();
        QVERIFY(copied);
        QCOMPARE(copied->text(), QStringLiteral("sbqinv1.test"));
        QCOMPARE(copied->data(QStringLiteral("x-kde-passwordManagerHint")), QByteArrayLiteral("secret"));
        // 期限（この試験では作っていないので過ぎている）を過ぎると、まだこの文字列のままなら消す
        QTRY_VERIFY(QApplication::clipboard()->text().isEmpty());

        // 合言葉（同じ LAN の招待の短い形）は、合言葉の付いた招待のときだけ出し、同じ印を付けてコピーする
        auto* codeRow = window.findChild<QWidget*>(QStringLiteral("p2pInviteCodeRow"));
        auto* codeOut = window.findChild<QLabel*>(QStringLiteral("p2pInviteCode"));
        auto* copyCode = window.findChild<QPushButton*>(QStringLiteral("p2pCopyCode"));
        QVERIFY(codeRow && codeOut && copyCode);
        tabs->setCurrentIndex(1);
        QVERIFY(!codeRow->isVisible());
        emit lobby.inviteCreated(QStringLiteral("sbqinv1.withcode"), QStringLiteral("008Q-4HMA-SW95"), 0, 2);
        QVERIFY(codeRow->isVisible());
        QCOMPARE(codeOut->text(), QStringLiteral("008Q-4HMA-SW95"));
        QCOMPARE(inviteOut->toPlainText(), QStringLiteral("sbqinv1.withcode"));
        // 合言葉は、ほかの文より大きく出す
        QVERIFY(codeOut->font().pointSizeF() > window.font().pointSizeF());
        copyCode->click();
        copied = QApplication::clipboard()->mimeData();
        QVERIFY(copied);
        QCOMPARE(copied->text(), QStringLiteral("008Q-4HMA-SW95"));
        QCOMPARE(copied->data(QStringLiteral("x-kde-passwordManagerHint")), QByteArrayLiteral("secret"));
        QTRY_VERIFY(QApplication::clipboard()->text().isEmpty());
        // 合言葉の付かない招待（協力ノードが合言葉に対応していない、など）では隠し、招待文字列を渡すことを知らせる
        auto* noCode = window.findChild<QLabel*>(QStringLiteral("p2pInviteNoCode"));
        QVERIFY(noCode);
        QVERIFY(!noCode->isVisible());
        emit lobby.inviteCreated(QStringLiteral("sbqinv1.nocode"), QString(), 0, 1);
        QVERIFY(!codeRow->isVisible());
        QVERIFY(codeOut->text().isEmpty());
        QVERIFY(noCode->isVisible());
        QVERIFY2(noCode->text().contains(QStringLiteral("合言葉が付きません")), qPrintable(noCode->text()));
        QVERIFY2(noCode->height() >= noCode->heightForWidth(noCode->width()), "案内が欠けている");

        // 招待を作り直す間（合言葉の鍵を作る間は応答が少し遅れる）、前の招待を欄に残さない
        emit lobby.inviteCreated(QStringLiteral("sbqinv1.previous"), QStringLiteral("008Q-4HMA-SW95"), 0, 2);
        QVERIFY(!noCode->isVisible());
        window.findChild<QLineEdit*>(QStringLiteral("p2pDisplayName"))->setText(QStringLiteral("試験"));
        QSignalSpy createRequested(&window, &P2pWindow::createInviteRequested);
        QVERIFY(QMetaObject::invokeMethod(invitePanel, "onCreateInvite"));
        QCOMPARE(createRequested.count(), 1);
        QVERIFY(inviteOut->toPlainText().isEmpty());
        QVERIFY(codeOut->text().isEmpty());
    }

    /// 「招待を受ける」の欄は、合言葉と招待文字列のどちらも受ける。合言葉は、全角・小文字・区切りを直して渡し、
    /// 断られた理由は合言葉の文で出す
    void inviteInputAcceptsCodes()
    {
        // 招待文字列は前後の空白を除くだけ
        QVERIFY(P2pWindowText::isInviteString(QStringLiteral("  sbqinv1.AAAA.00000000\n")));
        QCOMPARE(P2pWindowText::inviteInput(QStringLiteral("  sbqinv1.AA-AA.00000000\n")), QStringLiteral("sbqinv1.AA-AA.00000000"));
        // 合言葉は、英数字だけを大文字で残す（全角の英数字・全角のハイフン・長音・空白・改行を直す）
        QVERIFY(!P2pWindowText::isInviteString(QStringLiteral("008Q-4HMA-SW95")));
        QCOMPARE(P2pWindowText::inviteInput(QStringLiteral("008q-4hma-sw95")), QStringLiteral("008Q4HMASW95"));
        QCOMPARE(P2pWindowText::inviteInput(QStringLiteral(" 008Q 4HMA\nSW95 ")), QStringLiteral("008Q4HMASW95"));
        QCOMPARE(P2pWindowText::inviteInput(QStringLiteral("００８Ｑ－４ｈｍａーＳＷ９５")), QStringLiteral("008Q4HMASW95"));
        QCOMPARE(P2pWindowText::inviteInput(QStringLiteral("あいう")), QString());

        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("code-input")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        P2pWindow window(&lobby, &game);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QTabWidget*>()->setCurrentIndex(1);
        auto* input = window.findChild<QPlainTextEdit*>(QStringLiteral("p2pInviteIn"));
        auto* result = window.findChild<QLabel*>(QStringLiteral("p2pInviteInfo"));
        auto* connectButton = window.findChild<QPushButton*>(QStringLiteral("p2pConnectInvite"));
        auto* panel = window.findChild<P2pInvitePanel*>();
        QVERIFY(input && result && connectButton && panel);

        // 確かめるときに渡すのは、直した文字列
        QSignalSpy inspect(&window, &P2pWindow::inspectInviteRequested);
        input->setPlainText(QStringLiteral("００８ｑ－４ｈｍａ－ｓｗ９５"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onInspectInvite"));
        QCOMPARE(inspect.count(), 1);
        QCOMPARE(inspect.first().at(0).toString(), QStringLiteral("008Q4HMASW95"));

        // 合言葉を確かめた結果には、相手の PeerId と期限を出さない（合言葉に入っていない）
        P2pLobbyService::InviteInfo info;
        info.kind = PeerIpc::InviteKind::LanCode;
        info.addressCount = 2;
        info.usableAddressCount = 2;
        emit lobby.inviteInspected(info);
        QVERIFY(result->text().contains(QStringLiteral("合言葉")));
        QVERIFY(!result->text().contains(QStringLiteral("PeerId")));
        QCOMPARE(result->text().count(QLatin1Char('\n')), 1);

        // 断られた理由は、合言葉の文で出す
        emit lobby.inspectRejected(PeerIpc::InviteRejection::Unreadable);
        QCOMPARE(result->text(), QStringLiteral("合言葉を読めません。入力した文字を確かめてください。"));
        emit lobby.inspectRejected(PeerIpc::InviteRejection::WrongNetwork);
        QVERIFY(result->text().contains(QStringLiteral("同じPC・同じLAN")));
        emit lobby.inspectRejected(PeerIpc::InviteRejection::Own);
        QCOMPARE(result->text(), QStringLiteral("自分が作った合言葉です。"));
        // 協力ノードを使う合言葉: 招待が見つからない・問い合わせる協力ノードが無い
        emit lobby.inspectRejected(PeerIpc::InviteRejection::NotFound);
        QVERIFY(result->text().contains(QStringLiteral("この合言葉の招待が見つかりません")));
        emit lobby.inspectRejected(PeerIpc::InviteRejection::NoNode);
        QVERIFY(result->text().contains(QStringLiteral("協力ノード")));
        QVERIFY(result->text().contains(QStringLiteral("「接続」タブ")));
        // 協力ノードから引き取った招待は、招待文字列と同じく相手の PeerId と期限が分かる
        P2pLobbyService::InviteInfo fetched;
        fetched.kind = PeerIpc::InviteKind::NodeCode;
        fetched.peerId = QStringLiteral("12D3KooWabcdefghijklmnop");
        fetched.expiresAt = 1'900'000'000;
        fetched.addressCount = 1;
        fetched.usableAddressCount = 1;
        emit lobby.inviteInspected(fetched);
        QVERIFY(result->text().contains(QStringLiteral("協力ノードから受け取りました")));
        QVERIFY(result->text().contains(QStringLiteral("PeerId: 12D3KooWabcdefgh…")));
        QCOMPARE(result->text().count(QLatin1Char('\n')), 3);
        // 招待文字列を確かめた後は、招待文字列の文に戻る
        input->setPlainText(QStringLiteral("sbqinv1.AAAA.00000000"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onInspectInvite"));
        QCOMPARE(inspect.last().at(0).toString(), QStringLiteral("sbqinv1.AAAA.00000000"));
        emit lobby.inspectRejected(PeerIpc::InviteRejection::Unreadable);
        QCOMPARE(result->text(), QStringLiteral("招待文字列を読めません。"));

        // 接続した後に、相手が招待を受け付けなかった（合言葉が違う・期限切れ・使用済み）: 「接続しています…」のまま
        // 残さず、理由を出す。接続を頼むときは、確かめた結果の種類（同じ LAN の合言葉か）を渡す
        using Attempt = P2pLobbyService::ConnectAttempt;
        using Stage = P2pLobbyService::ConnectFailure::Stage;
        const auto failure = [](Attempt attempt, Stage stage) {
            P2pLobbyService::ConnectFailure f;
            f.attempt = attempt;
            f.stage = stage;
            return f;
        };
        QLineEdit* name = window.findChild<QLineEdit*>(QStringLiteral("p2pDisplayName"));
        QVERIFY(name);
        name->setText(QStringLiteral("試験"));
        QSignalSpy connectRequested(&window, &P2pWindow::connectInviteRequested);
        input->setPlainText(QStringLiteral("008Q-4HMA-SW95"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onInspectInvite"));
        emit lobby.inviteInspected(info);
        QVERIFY(QMetaObject::invokeMethod(panel, "onConnectInvite"));
        QCOMPARE(connectRequested.count(), 1);
        QCOMPARE(connectRequested.last().at(0).toString(), QStringLiteral("008Q4HMASW95"));
        QCOMPARE(connectRequested.last().at(1).value<Attempt>(), Attempt::LanCode);
        QCOMPARE(result->text(), QStringLiteral("接続しています…"));
        QVERIFY(input->toPlainText().isEmpty());
        emit game.negotiationEnded(false, P2p::ErrorCode::Cancelled, false);
        QVERIFY(result->text().isEmpty());
        emit lobby.connectAttemptFailed(failure(Attempt::LanCode, Stage::Refused));
        QVERIFY(result->text().contains(QStringLiteral("合言葉が受け付けられませんでした")));
        // 招待文字列で接続したときは、招待文字列の文
        input->setPlainText(QStringLiteral("sbqinv1.AAAA.00000000"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onConnectInvite"));
        QCOMPARE(connectRequested.last().at(1).value<Attempt>(), Attempt::InviteString);
        emit game.negotiationEnded(false, P2p::ErrorCode::Cancelled, false);
        emit lobby.connectAttemptFailed(failure(Attempt::InviteString, Stage::Refused));
        QVERIFY(result->text().contains(QStringLiteral("招待文字列が受け付けられませんでした")));
        // 通信プロセスが接続の要求を断ったときも、欄の下に、確かめたときと同じ文で出す
        P2pLobbyService::ConnectFailure expired = failure(Attempt::InviteString, Stage::Rejected);
        expired.rejection = PeerIpc::InviteRejection::Expired;
        emit lobby.connectAttemptFailed(expired);
        QCOMPARE(result->text(), QStringLiteral("招待文字列の期限が過ぎています。"));
        // 対局が始まったときは、途中の文を消すだけ
        input->setPlainText(QStringLiteral("008Q-4HMA-SW95"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onConnectInvite"));
        emit game.negotiationEnded(true, std::nullopt, false);
        QVERIFY(result->text().isEmpty());

        // 回線でつながらなかったときは、欄の「接続しています…」を消して、知らせの箱で出す（同じ LAN の合言葉なら、
        // 合言葉と相手のアプリを確かめる案内も添える）
        const auto visibleNotice = [&window]() -> QMessageBox* {
            const QList<QMessageBox*> boxes = window.findChildren<QMessageBox*>();
            for (QMessageBox* box : boxes) {
                if (box->isVisible()) return box;
            }
            return nullptr;
        };
        QSignalSpy lanConnect(&window, &P2pWindow::connectInviteRequested);
        input->setPlainText(QStringLiteral("008Q-4HMA-SW95"));
        QVERIFY(QMetaObject::invokeMethod(panel, "onInspectInvite"));
        emit lobby.inviteInspected(info);
        QVERIFY(QMetaObject::invokeMethod(panel, "onConnectInvite"));
        P2pLobbyService::ConnectFailure timedOut = failure(Attempt::LanCode, Stage::NotOpened);
        timedOut.reason = PeerIpc::CloseReason::Timeout;
        emit lobby.connectAttemptFailed(timedOut);
        QVERIFY(result->text().isEmpty());
        QMessageBox* notice = visibleNotice();
        QVERIFY(notice);
        QVERIFY2(notice->text().contains(QStringLiteral("時間切れ")) && notice->text().contains(QStringLiteral("合言葉が正しいか")),
                 qPrintable(notice->text()));
        notice->close();
        // 友だちへの申し込みの結果は、招待の欄に触れずに、知らせの箱で出す
        emit lobby.inviteInspected(info);
        const QString inspected = result->text();
        emit lobby.connectAttemptFailed(failure(Attempt::Friend, Stage::Refused));
        QCOMPARE(result->text(), inspected);
        QTRY_VERIFY(visibleNotice());
        QVERIFY2(visibleNotice()->text().contains(QStringLiteral("今は申し込みを受けられません")),
                 qPrintable(visibleNotice()->text()));
        emit lobby.connectAttemptFailed(failure(Attempt::Friend, Stage::Rejected));
        QVERIFY2(visibleNotice()->text().contains(QStringLiteral("在席の知らせを使えませんでした")),
                 qPrintable(visibleNotice()->text()));
        visibleNotice()->close();
    }

    /// 友だち: 「相手」タブで友だちにする・外す。ブロックした相手は友だちにできず、ブロックすると友だちから
    /// 外れる。友だちは別のファイルに保存する。「友だちに在席を知らせる」の既定は無効で、プレイヤーごとに覚える
    void friendsInPeersPanelAndSettings()
    {
        const QString path = m_dir.filePath(QStringLiteral("friend-panel.cbor"));
        const SigningKey a = SigningKey::generate();
        const SigningKey b = SigningKey::generate();
        {
            P2pKnownPeers known(path);
            QVERIFY(known.recordGameStarted(a.publicKey(), QStringLiteral("A")));
            QVERIFY(known.recordGameStarted(b.publicKey(), QStringLiteral("B")));
            QVERIFY(known.friends().isEmpty());
            QVERIFY(known.setFriend(a.publicKey(), true));
            QCOMPARE(known.friends().size(), 1);
            // ブロックした相手・記録のない相手は、友だちにできない
            QVERIFY(known.setBlocked(b.publicKey(), true));
            QVERIFY(!known.setFriend(b.publicKey(), true));
            QVERIFY(!known.setFriend(QByteArray(32, 'x'), true));
        }
        {
            // 読み直しても友だちのまま。相手の記録のファイルの形は変えない（友だちは別のファイル）
            P2pKnownPeers known(path);
            QVERIFY(known.load());
            QVERIFY(known.find(a.publicKey())->isFriend);
            QVERIFY(!known.find(b.publicKey())->isFriend);
            QVERIFY(QFile::exists(m_dir.filePath(QStringLiteral("friend-panel-friends.cbor"))));
            // ブロックすると、友だちから外れる
            QVERIFY(known.setBlocked(a.publicKey(), true));
            QVERIFY(!known.find(a.publicKey())->isFriend);
        }
        P2pKnownPeers known(path);
        QVERIFY(known.load());
        QVERIFY(known.friends().isEmpty());
        QVERIFY(known.setBlocked(a.publicKey(), false));
        QVERIFY(known.setFriend(a.publicKey(), true));

        P2pPeersPanel panel(&known, nullptr);
        panel.reload();
        auto* table = panel.findChild<QTableWidget*>(QStringLiteral("p2pPeerTable"));
        auto* friendButton = panel.findChild<QPushButton*>(QStringLiteral("p2pPeerFriend"));
        auto* apply = panel.findChild<QPushButton*>(QStringLiteral("p2pPeerApply"));
        auto* note = panel.findChild<QLabel*>(QStringLiteral("p2pFriendNote"));
        QVERIFY(table && friendButton && apply && note);
        constexpr int kFriendColumn = 3;
        QCOMPARE(table->horizontalHeaderItem(kFriendColumn)->text(), QStringLiteral("友だち"));
        const auto rowOf = [table](const QByteArray& key) {
            for (int row = 0; row < table->rowCount(); ++row) {
                if (table->item(row, 0)->data(Qt::UserRole).toByteArray() == key) return row;
            }
            return -1;
        };
        QCOMPARE(table->item(rowOf(a.publicKey()), kFriendColumn)->text(), QStringLiteral("友だち"));
        QVERIFY(table->item(rowOf(b.publicKey()), kFriendColumn)->text().isEmpty());
        // 選んでいなければ押せない。友だちは「友だちから外す」、ブロックした相手は友だちにできない
        QVERIFY(!friendButton->isEnabled() && !apply->isEnabled());
        table->selectRow(rowOf(a.publicKey()));
        QCOMPARE(friendButton->text(), QStringLiteral("友だちから外す"));
        QVERIFY(friendButton->isEnabled());
        // 在席が分からない間（協力ノードにつながっていない）は、申し込めない。使い方を案内する
        QVERIFY(!apply->isEnabled());
        QVERIFY(note->text().contains(QStringLiteral("協力ノード")));
        table->selectRow(rowOf(b.publicKey()));
        QCOMPARE(friendButton->text(), QStringLiteral("友だちに追加"));
        QVERIFY(!friendButton->isEnabled());
        // 押すと切り替わり、保存される
        table->selectRow(rowOf(a.publicKey()));
        friendButton->click();
        QVERIFY(known.friends().isEmpty());
        table->selectRow(rowOf(a.publicKey()));
        QCOMPARE(friendButton->text(), QStringLiteral("友だちに追加"));
        friendButton->click();
        QCOMPARE(known.friends().size(), 1);
        QCOMPARE(table->item(rowOf(a.publicKey()), kFriendColumn)->text(), QStringLiteral("友だち"));

        // 設定: 「友だちに在席を知らせる」は既定で無効。変えると保存する
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("friend-settings")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        NetworkSettings::p2pSharePresence(1).remove();
        QVERIFY(!NetworkSettings::p2pSharePresence(1)());
        P2pFriendPresence presence(&lobby, &game);
        {
            P2pWindow window(&lobby, &game);
            window.setFriendPresence(&presence);
            auto* share = window.findChild<QCheckBox*>(QStringLiteral("p2pSharePresence"));
            QVERIFY(share);
            QVERIFY(!share->isChecked());
            QVERIFY(!presence.isSharing());
            share->setChecked(true);
            QVERIFY(NetworkSettings::p2pSharePresence(1)());
            QVERIFY(presence.isSharing());
            // 接続していない間は、在席を使えない
            QVERIFY(!presence.isAvailable());
        }
        P2pWindow reopened(&lobby, &game);
        QVERIFY(reopened.findChild<QCheckBox*>(QStringLiteral("p2pSharePresence"))->isChecked());
        NetworkSettings::p2pSharePresence(1).remove();
    }

    /// ウィンドウの操作は意図のシグナルで画面の部品が受け、投了と合意中断はメニューと同じ QAction を通る
    /// （監査 2026-11-12 R10。メニューと同じ入口なので、同じ規則で窓口へ送る）
    void windowButtonsGoThroughTheMenuActions()
    {
        const QByteArray savedData = qgetenv("SHOGIBOARDQ_DATA_HOME");
        qputenv("SHOGIBOARDQ_DATA_HOME", m_dir.filePath(QStringLiteral("buttons-data")).toUtf8());
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        QMainWindow main;
        QAction resign;
        QAction breakOff;
        QSignalSpy resigned(&resign, &QAction::triggered);
        QSignalSpy brokeOff(&breakOff, &QAction::triggered);
        P2pGameUiController::Deps deps;
        deps.game = &game;
        deps.activity = &activity;
        deps.parentWidget = &main;
        deps.resignAction = &resign;
        deps.breakOffAction = &breakOff;
        P2pGameUiController ui(deps);
        ui.openWindow();
        auto* window = main.findChild<P2pWindow*>();
        QVERIFY(window);
        for (const char* name : {"p2pResign", "p2pOfferAbort"}) {
            auto* button = window->findChild<QPushButton*>(QLatin1String(name));
            QVERIFY2(button, name);
            button->setEnabled(true);   // 対局していないので押せない。結び付きだけを確かめる
            button->click();
        }
        QCOMPARE(resigned.count(), 1);
        QCOMPARE(brokeOff.count(), 1);
        ui.shutdown();
        if (savedData.isNull()) qunsetenv("SHOGIBOARDQ_DATA_HOME");
        else qputenv("SHOGIBOARDQ_DATA_HOME", savedData);
    }

    /// P2P対局のウィンドウは、対局中も開いたまま本体の盤と行き来して使う。親（本体）のあるダイアログを OS は本体の
    /// 子ウィンドウとして扱い、いつも本体の前面に置く（本体を前面に出せない）ので、通常のウィンドウにして OS の
    /// 親子関係を外す。閉じて開き直しても同じ
    void windowIsNotKeptAboveMainWindow()
    {
        const QByteArray savedData = qgetenv("SHOGIBOARDQ_DATA_HOME");
        qputenv("SHOGIBOARDQ_DATA_HOME", m_dir.filePath(QStringLiteral("independent-data")).toUtf8());
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        QMainWindow main;
        auto* central = new QWidget(&main);
        main.setCentralWidget(central);
        auto* inner = new QLabel(QStringLiteral("board"), central);
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));
        P2pGameUiController::Deps deps;
        deps.game = &game;
        deps.activity = &activity;
        deps.parentWidget = &main;
        P2pGameUiController ui(deps);
        for (int cycle = 0; cycle < 2; ++cycle) {
            ui.openWindow();
            auto* window = main.findChild<P2pWindow*>();
            QVERIFY(window);
            QVERIFY(QTest::qWaitForWindowExposed(window));
            QCOMPARE(window->windowType(), Qt::Window);
            QVERIFY(!window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
            QVERIFY(window->windowHandle());
            QVERIFY(!window->windowHandle()->transientParent());
            // Qt の所有関係は残し、開いたままでもアプリの終了を止めない
            QCOMPARE(window->parentWidget(), &main);
            QVERIFY(!window->testAttribute(Qt::WA_QuitOnClose));
            // 本体の中の部品（盤・ドック・ツールバー）を、OS のウィンドウに作り替えない。作り替えると、最初に
            // 開いたときに本体の全部の部品が OS の子ウィンドウになり、盤が一瞬小さく描かれる
            QVERIFY(!window->testAttribute(Qt::WA_NativeWindow));
            const QList<QWidget*> children{central, inner, main.menuBar()};
            for (const QWidget* child : children) {
                QVERIFY(!child->testAttribute(Qt::WA_NativeWindow));
                QVERIFY(!child->internalWinId());
            }
            QVERIFY(window->close());
        }
        ui.shutdown();
        if (savedData.isNull()) qunsetenv("SHOGIBOARDQ_DATA_HOME");
        else qputenv("SHOGIBOARDQ_DATA_HOME", savedData);
    }

    /// 相手から届いた知らせ（申し出の箱など）は、利用者が使っている側のウィンドウに出す。P2P対局のウィンドウを使って
    /// いる間はそのウィンドウに、本体の盤で指している間は本体に出す（後ろに回した P2P対局のウィンドウを親にすると、
    /// OS が知らせと一緒にそのウィンドウも前面に出し、盤を隠す）
    void noticesFollowTheWindowInUse()
    {
        const QByteArray savedData = qgetenv("SHOGIBOARDQ_DATA_HOME");
        qputenv("SHOGIBOARDQ_DATA_HOME", m_dir.filePath(QStringLiteral("notice-data")).toUtf8());
        {
            Seat applicant(QByteArray(38, 'g'));
            Seat host(QByteArray(38, 'n'));
            P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("notice-a")));
            P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("notice-h")));
            P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("notice-a.cbor")));
            P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("notice-h.cbor")));
            QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
            Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
            Seat& white = applicant.game.match()->iAmBlack() ? host : applicant;
            const auto offerBox = [](const QString& word) -> QMessageBox* {
                const QWidgetList windows = QApplication::topLevelWidgets();
                for (QWidget* widget : windows) {
                    auto* box = qobject_cast<QMessageBox*>(widget);
                    if (box && box->objectName() == QStringLiteral("p2pOfferBox") && box->isVisible()
                        && box->text().contains(word))
                        return box;
                }
                return nullptr;
            };

            white.window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&white.window));
            white.ui->openWindow();
            auto* p2p = white.window.findChild<P2pWindow*>();
            QVERIFY(p2p);
            QVERIFY(QTest::qWaitForWindowExposed(p2p));

            // P2P対局のウィンドウを使っている間は、そのウィンドウに出す
            QTRY_COMPARE(QApplication::activeWindow(), p2p);
            QTRY_VERIFY(black.game.isHumanToMove());
            black.game.offerDraw();
            QTRY_VERIFY(offerBox(QStringLiteral("引き分け")));
            QPointer<QMessageBox> first = offerBox(QStringLiteral("引き分け"));
            QCOMPARE(first->parentWidget(), p2p);
            // 先手が指すと申し出は無効になり、箱は閉じる
            QCOMPARE(black.game.playMove(UsiMove::fromString(u"2g2f")), GameChannel::MoveResult::Sent);
            QTRY_VERIFY(!first);
            QTRY_VERIFY(black.game.match()->lastState().ply == 1 && !black.game.match()->hasPendingMove());

            // 本体を使っている間（P2P対局のウィンドウは開いたまま後ろ）は、本体に出す
            white.window.activateWindow();
            QTRY_COMPARE(QApplication::activeWindow(), &white.window);
            QVERIFY(p2p->isVisible());
            black.game.offerAbort();
            QTRY_VERIFY(offerBox(QStringLiteral("中断")));
            QCOMPARE(offerBox(QStringLiteral("中断"))->parentWidget(), &white.window);
            white.ui->shutdown();
        }
        if (savedData.isNull()) qunsetenv("SHOGIBOARDQ_DATA_HOME");
        else qputenv("SHOGIBOARDQ_DATA_HOME", savedData);
    }

    /// 同じ PC でいくつものアプリが「P2P対局」を開くと、開いた順に、空いている最初の番号のプレイヤーになる
    /// （1人目・2人目・3人目…）。保存先はプレイヤーごとに別で、閉じたアプリの番号は次に開いたアプリが使う
    void appsOnOnePcBecomeSeparatePlayers()
    {
        const QString data = m_dir.filePath(QStringLiteral("players"));

        std::unique_ptr<P2pPlayerProfile> first(P2pPlayerProfile::acquire(data));
        std::unique_ptr<P2pPlayerProfile> second(P2pPlayerProfile::acquire(data));
        std::unique_ptr<P2pPlayerProfile> third(P2pPlayerProfile::acquire(data));
        QVERIFY(first && second && third);
        QCOMPARE(first->playerNumber(), 1);
        QCOMPARE(second->playerNumber(), 2);
        QCOMPARE(third->playerNumber(), 3);
        // 1人目の保存先はこれまでと同じ p2p。2人目からは別のフォルダで、鍵の場所も別
        QCOMPARE(P2pPlayerProfile::identityDirectory(first->root()), data + QStringLiteral("/p2p/identity"));
        QCOMPARE(P2pPlayerProfile::identityDirectory(second->root()), data + QStringLiteral("/p2p-2/identity"));
        QCOMPARE(P2pPlayerProfile::identityDirectory(third->root()), data + QStringLiteral("/p2p-3/identity"));
        QVERIFY(first->isOpen() && second->isOpen() && third->isOpen());

        // 2人目のアプリを閉じると、次に開いたアプリが2人目になる（3人目は動いたまま）
        second.reset();
        std::unique_ptr<P2pPlayerProfile> again(P2pPlayerProfile::acquire(data));
        QVERIFY(again);
        QCOMPARE(again->playerNumber(), 2);

        // 同時に使えるのは kMaxPlayers 個まで
        std::vector<std::unique_ptr<P2pPlayerProfile>> rest;
        for (int player = 4; player <= P2pPlayerSlot::kMaxPlayers; ++player) {
            rest.emplace_back(P2pPlayerProfile::acquire(data));
            QVERIFY(rest.back());
            QCOMPARE(rest.back()->playerNumber(), player);
        }
        QVERIFY(!P2pPlayerProfile::acquire(data));
    }

    /// 1人目のウィンドウはこれまでどおり。2人目からは、題名と上の案内で何人目かが分かり、表示名も別に覚える
    void windowShowsWhichPlayerItIs()
    {
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        const P2pLobbyService::Deps deps{&activity, &game};
        const QString data = m_dir.filePath(QStringLiteral("titles"));
        std::unique_ptr<P2pPlayerProfile> firstProfile(P2pPlayerProfile::acquire(data));
        std::unique_ptr<P2pPlayerProfile> secondProfile(P2pPlayerProfile::acquire(data));
        QVERIFY(firstProfile && secondProfile);
        P2pLobbyService first(deps, firstProfile.get());
        P2pLobbyService second(deps, secondProfile.get());
        NetworkSettings::p2pDisplayName(1).remove();
        NetworkSettings::p2pDisplayName(2).remove();
        {
            P2pWindow one(&first, &game);
            P2pWindow two(&second, &game);
            QCOMPARE(one.windowTitle(), QStringLiteral("P2P対局"));
            QVERIFY(!one.findChild<QLabel*>(QStringLiteral("p2pPlayerNotice")));
            QCOMPARE(two.windowTitle(), QStringLiteral("P2P対局 — 2人目"));
            auto* notice = two.findChild<QLabel*>(QStringLiteral("p2pPlayerNotice"));
            QVERIFY(notice);
            QVERIFY2(notice->text().contains(QStringLiteral("「2人目」")), qPrintable(notice->text()));
            two.show();
            QVERIFY(QTest::qWaitForWindowExposed(&two));
            QVERIFY(notice->isVisible());
            QVERIFY2(notice->height() >= notice->heightForWidth(notice->width()), "案内が欠けている");

            // 表示名はプレイヤーごとに覚える（接続・招待のときに保存する）
            auto* nameOne = one.findChild<QLineEdit*>(QStringLiteral("p2pDisplayName"));
            auto* nameTwo = two.findChild<QLineEdit*>(QStringLiteral("p2pDisplayName"));
            QVERIFY(nameOne && nameTwo);
            nameOne->setText(QStringLiteral("一人目"));
            QVERIFY(QMetaObject::invokeMethod(&one, "saveSettings"));
            nameTwo->setText(QStringLiteral("二人目"));
            QVERIFY(QMetaObject::invokeMethod(&two, "saveSettings"));
        }
        QCOMPARE(NetworkSettings::p2pDisplayName(1)(), QStringLiteral("一人目"));
        QCOMPARE(NetworkSettings::p2pDisplayName(2)(), QStringLiteral("二人目"));
        P2pWindow reopened(&second, &game);
        QCOMPARE(reopened.findChild<QLineEdit*>(QStringLiteral("p2pDisplayName"))->text(), QStringLiteral("二人目"));
    }

    /// 招待文字列を確かめた結果は、初めて開いたときの大きさのウィンドウでも欠けない（収まらなければウィンドウを広げる）。
    /// 折り返すラベルの高さは幅で決まり、ウィンドウの最小の高さには入らないので、Qt は自動では広げない
    void inviteCheckResultIsNotClipped()
    {
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("clip")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        P2pWindow window(&lobby, &game);
        window.resize(NetworkSettings::p2pWindowSize.defaultValue());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* tabs = window.findChild<QTabWidget*>();
        QVERIFY(tabs);
        tabs->setCurrentIndex(1);
        QCoreApplication::processEvents();

        const auto clippedLabel = [tabs]() {
            const QList<QLabel*> labels = tabs->currentWidget()->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                if (!label->isVisible() || label->text().isEmpty()) continue;
                const int needed = label->wordWrap() ? label->heightForWidth(label->width()) : label->sizeHint().height();
                if (label->height() < needed) return label->text();
            }
            return QString();
        };
        QVERIFY2(clippedLabel().isEmpty(), qPrintable(clippedLabel()));

        // 同じ LAN の相手の招待（確かめた結果は3行）
        P2pLobbyService::InviteInfo info;
        info.peerId = QStringLiteral("12D3KooWDpJ7As7BWAwRMfu1VU2WCqNjvq387JEYKDBj4kx6nXTN");
        info.expiresAt = 1794387939;
        info.addressCount = 6;
        info.usableAddressCount = 6;
        emit lobby.inviteInspected(info);
        QCoreApplication::processEvents();
        auto* result = window.findChild<QLabel*>(QStringLiteral("p2pInviteInfo"));
        QVERIFY(result);
        QCOMPARE(result->text().count(QLatin1Char('\n')), 2);
        QVERIFY2(clippedLabel().isEmpty(), qPrintable(clippedLabel()));

        // 私設アドレスへの接続の許可を尋ねるとき（行とチェックが増える）
        info.privateAddresses = {QStringLiteral("192.168.1.20"), QStringLiteral("10.0.0.5")};
        info.usableAddressCount = 0;
        emit lobby.inviteInspected(info);
        QCoreApplication::processEvents();
        QVERIFY2(clippedLabel().isEmpty(), qPrintable(clippedLabel()));
    }

    /// 「接続」タブ: 相手のいる場所とつなぎ方は、どちらの選択肢にも説明を付けて並べ、選んだ組み合わせでどうなるかを
    /// 下に出す（チェックだけだと、付けない場合に何が起きるかが分からなかった）。つながらない組み合わせ（「インター
    /// ネット」や中継だけで、協力ノードなし）では、結果の代わりに理由と直し方を出す。協力ノードの欄は、要る組み合わせの
    /// ときと、登録があるときだけ出す
    void connectionPanelExplainsTheChoices()
    {
        P2pConnectionPanel panel;
        panel.resize(760, 560);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* lan = panel.findChild<QRadioButton*>(QStringLiteral("p2pPlaceLan"));
        auto* internet = panel.findChild<QRadioButton*>(QStringLiteral("p2pPlaceInternet"));
        auto* direct = panel.findChild<QRadioButton*>(QStringLiteral("p2pRouteDirect"));
        auto* relay = panel.findChild<QRadioButton*>(QStringLiteral("p2pRouteRelay"));
        auto* nodes = panel.findChild<QListWidget*>(QStringLiteral("p2pBootstrap"));
        auto* add = panel.findChild<QPushButton*>(QStringLiteral("p2pNodeAdd"));
        auto* paste = panel.findChild<QPushButton*>(QStringLiteral("p2pNodePaste"));
        auto* remove = panel.findChild<QPushButton*>(QStringLiteral("p2pNodeRemove"));
        auto* showNodes = panel.findChild<QPushButton*>(QStringLiteral("p2pNodeShow"));
        auto* summary = panel.findChild<QFrame*>(QStringLiteral("p2pConnectionSummary"));
        auto* warning = panel.findChild<QFrame*>(QStringLiteral("p2pConnectionWarning"));
        auto* warningTitle = panel.findChild<QLabel*>(QStringLiteral("p2pConnectionWarningTitle"));
        auto* nodeStatus = panel.findChild<QLabel*>(QStringLiteral("p2pNodeStatus"));
        QVERIFY(lan && internet && direct && relay && nodes && add && paste && remove && showNodes);
        QVERIFY(summary && warning && warningTitle && nodeStatus);
        QSignalSpy changed(&panel, &P2pConnectionPanel::settingsChanged);
        // 枠の中の文（選択肢の説明・下の結果）
        const auto textIn = [](const QWidget* box) {
            QStringList texts;
            const QList<QLabel*> labels = box->findChildren<QLabel*>();
            for (const QLabel* label : labels) {
                if (!label->isHidden()) texts << label->text();
            }
            return texts.join(QLatin1Char('\n'));
        };

        // 既定: 同じPC・同じLAN で直接つなぐ。協力ノードは要らないので、欄は出さず、結果を出す
        QVERIFY(lan->isChecked() && direct->isChecked());
        QCOMPARE(panel.networkName(), QStringLiteral("sbq-local-1"));
        QVERIFY(!panel.relayOnly());
        QVERIFY(panel.bootstrap().isEmpty());
        QVERIFY(!nodes->isVisible() && showNodes->isVisible());
        QVERIFY(summary->isVisible() && !warning->isVisible());
        QVERIFY2(textIn(summary).contains(QStringLiteral("同じPC・同じLANの相手")), qPrintable(textIn(summary)));
        // どちらのつなぎ方にも、経路・相手に伝わるもの・要るものの説明がある（選ばなかった側の動作も読める）
        for (const QRadioButton* choice : {direct, relay}) {
            const QString text = textIn(choice->parentWidget());
            QVERIFY2(text.contains(QStringLiteral("経路")) && text.contains(QStringLiteral("相手に伝わる"))
                         && text.contains(QStringLiteral("要るもの")),
                     qPrintable(text));
        }
        QVERIFY(textIn(direct->parentWidget()).contains(QStringLiteral("LANの中のアドレス")));
        QVERIFY(textIn(relay->parentWidget()).contains(QStringLiteral("IPアドレスは伝わりません")));

        // 「インターネット」: 協力ノードが要る。無い間は、相手が接続できないことと直し方を出す
        internet->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(panel.networkName(), QStringLiteral("sbq-limited-1"));
        QVERIFY(nodes->isVisible() && !showNodes->isVisible());
        QVERIFY(warning->isVisible() && !summary->isVisible());
        QVERIFY2(warningTitle->text().contains(QStringLiteral("相手は接続できません")), qPrintable(warningTitle->text()));
        QVERIFY(textIn(direct->parentWidget()).contains(QStringLiteral("あなたのIPアドレス")));

        // 貼り付け: 協力ノードのアドレスの形の行だけを足す（説明の文・空の行・同じアドレスは足さない）
        const QString node = QStringLiteral("/ip4/203.0.113.24/tcp/4001/p2p/12D3KooWGHvttNCQYLHMsWkhQKAmvT6G87dfvtxDqurpa64rALxr");
        const QString other = QStringLiteral("/ip4/203.0.113.25/tcp/4001/p2p/12D3KooWDpJ7As7BWAwRMfu1VU2WCqNjvq387JEYKDBj4kx6nXTN");
        QApplication::clipboard()->setText(node + QStringLiteral("\n協力ノードです\n  \n") + node);
        paste->click();
        QCOMPARE(panel.bootstrap(), QStringList{node});
        QCOMPARE(changed.count(), 2);
        QVERIFY(summary->isVisible() && !warning->isVisible());
        QVERIFY2(textIn(summary).contains(QStringLiteral("インターネット上の相手")), qPrintable(textIn(summary)));
        QApplication::clipboard()->setText(node + QLatin1Char('\n') + other);
        paste->click();
        QCOMPARE(panel.bootstrap(), (QStringList{node, other}));
        QApplication::clipboard()->clear();

        // 中継だけを使う: IPアドレスが見えるのは協力ノードの運営者だけ
        relay->click();
        QVERIFY(panel.relayOnly());
        QVERIFY2(textIn(summary).contains(QStringLiteral("協力ノードの運営者だけ")), qPrintable(textIn(summary)));
        // 選んだ行を削除する。協力ノードが無くなると、中継を使えないことを出す
        QVERIFY(!remove->isEnabled());
        nodes->setCurrentRow(0);
        QVERIFY(remove->isEnabled());
        remove->click();
        nodes->setCurrentRow(0);
        remove->click();
        QVERIFY(panel.bootstrap().isEmpty());
        QVERIFY(warning->isVisible() && !summary->isVisible());
        QVERIFY2(warningTitle->text().contains(QStringLiteral("中継を使えません")), qPrintable(warningTitle->text()));
        // 同じPC・同じLAN でも、中継だけなら協力ノードが要る（欄は出したまま）
        lan->click();
        QVERIFY(nodes->isVisible() && warning->isVisible());
        // 直接つなぐに戻すと、協力ノードは要らないので、欄を隠す。使いたいときは「登録する…」で開ける
        direct->click();
        QVERIFY(!nodes->isVisible() && showNodes->isVisible() && summary->isVisible());
        showNodes->click();
        QVERIFY(nodes->isVisible());

        // 今の設定の1行（状態の帯に出す）と、招待に入るもの（「招待」タブに出す）
        const QString setting = panel.settingSummary();
        QVERIFY2(setting.contains(QStringLiteral("同じPC・同じLAN")) && setting.contains(QStringLiteral("直接つなぐ"))
                     && setting.contains(QStringLiteral("協力ノードなし")),
                 qPrintable(setting));
        QVERIFY(panel.inviteContentsText().contains(QStringLiteral("LANの中のアドレス")));

        // 接続している間は変えられない（枠を押しても変わらない）。変えられないことを知らせる
        const qsizetype before = changed.count();
        panel.setLocked(true);
        QVERIFY(!internet->isEnabled() && !add->isEnabled() && !paste->isEnabled());
        QVERIFY(panel.findChild<QLabel*>(QStringLiteral("p2pConnectionLock"))->isVisible());
        QTest::mouseClick(internet->parentWidget(), Qt::LeftButton);
        QVERIFY(lan->isChecked());
        QCOMPARE(changed.count(), before);
        // 接続していなければ、枠のどこを押しても選べる
        panel.setLocked(false);
        QVERIFY(!panel.findChild<QLabel*>(QStringLiteral("p2pConnectionLock"))->isVisible());
        QTest::mouseClick(internet->parentWidget(), Qt::LeftButton);
        QVERIFY(internet->isChecked());
        QCOMPARE(changed.count(), before + 1);

        // 接続している間は、協力ノードで中継と合言葉を使えるかを出す。値を外から入れても、変更の知らせは出さない
        panel.setBootstrap({node});
        panel.setRelayOnly(true);
        panel.setNetworkName(QStringLiteral("sbq-local-1"));
        QCOMPARE(changed.count(), before + 1);
        QVERIFY(lan->isChecked() && relay->isChecked());
        panel.setNodeStatus(P2pConnectionPanel::NodeStatus{1, 0});
        QVERIFY2(nodeStatus->text().contains(QStringLiteral("中継: 使えます"))
                     && nodeStatus->text().contains(QStringLiteral("対応した協力ノードがありません")),
                 qPrintable(nodeStatus->text()));
        panel.setNodeStatus(std::nullopt);
        QVERIFY(nodeStatus->text().isEmpty());
        saveShot(&panel, QStringLiteral("p2p-connection-panel"));
    }

    /// 「接続」タブの選択は、変えたときに保存し、開き直すと戻る。接続していない間は、状態の帯に今の設定を出す。
    /// 「招待」タブの注意は、今の設定で作る招待に何が入るかを出す
    void connectionChoicesAreSavedAndRestored()
    {
        const QString savedNetwork = NetworkSettings::p2pNetworkName();
        const bool savedRelayOnly = NetworkSettings::p2pRelayOnly();
        const QStringList savedNodes = NetworkSettings::p2pBootstrap();
        NetworkSettings::p2pNetworkName.set(QStringLiteral("sbq-local-1"));
        NetworkSettings::p2pRelayOnly.set(false);
        NetworkSettings::p2pBootstrap.set({});
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("choices")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        const QString node = QStringLiteral("/ip4/203.0.113.24/tcp/4001/p2p/12D3KooWGHvttNCQYLHMsWkhQKAmvT6G87dfvtxDqurpa64rALxr");
        {
            P2pWindow window(&lobby, &game);
            auto* routeInfo = window.findChild<QLabel*>(QStringLiteral("p2pRouteInfo"));
            auto* notes = window.findChild<QLabel*>(QStringLiteral("p2pInviteNotes"));
            QVERIFY(routeInfo && notes);
            QVERIFY2(notes->text().contains(QStringLiteral("LANの中のアドレス")), qPrintable(notes->text()));
            window.findChild<QRadioButton*>(QStringLiteral("p2pPlaceInternet"))->click();
            QCOMPARE(NetworkSettings::p2pNetworkName(), QStringLiteral("sbq-limited-1"));
            QVERIFY2(routeInfo->text().contains(QStringLiteral("インターネット"))
                         && routeInfo->text().contains(QStringLiteral("協力ノードなし")),
                     qPrintable(routeInfo->text()));
            QVERIFY2(notes->text().contains(QStringLiteral("あなたのIPアドレスが入ります")), qPrintable(notes->text()));
            QApplication::clipboard()->setText(node);
            window.findChild<QPushButton*>(QStringLiteral("p2pNodePaste"))->click();
            QApplication::clipboard()->clear();
            QCOMPARE(NetworkSettings::p2pBootstrap(), QStringList{node});
            window.findChild<QRadioButton*>(QStringLiteral("p2pRouteRelay"))->click();
            QVERIFY(NetworkSettings::p2pRelayOnly());
            QCOMPARE(window.networkName(), QStringLiteral("sbq-limited-1"));
            QCOMPARE(window.bootstrap(), QStringList{node});
            QVERIFY(window.relayOnly());
            QVERIFY2(routeInfo->text().contains(QStringLiteral("協力ノードの中継だけを使う")), qPrintable(routeInfo->text()));
            QVERIFY2(notes->text().contains(QStringLiteral("あなたのIPアドレスは入りません")), qPrintable(notes->text()));
        }
        {
            P2pWindow reopened(&lobby, &game);
            QVERIFY(reopened.findChild<QRadioButton*>(QStringLiteral("p2pPlaceInternet"))->isChecked());
            QVERIFY(reopened.findChild<QRadioButton*>(QStringLiteral("p2pRouteRelay"))->isChecked());
            QCOMPARE(reopened.findChild<QListWidget*>(QStringLiteral("p2pBootstrap"))->count(), 1);
            QCOMPARE(reopened.bootstrap(), QStringList{node});
            // 「設定」タブには、以前そこにあった欄の行き先を出し、「接続」タブへ移れる
            auto* tabs = reopened.findChild<QTabWidget*>(QStringLiteral("p2pTabs"));
            tabs->setCurrentIndex(tabs->count() - 1);
            reopened.findChild<QPushButton*>(QStringLiteral("p2pOpenConnectionTab"))->click();
            QCOMPARE(tabs->currentIndex(), 0);
        }
        NetworkSettings::p2pNetworkName.set(savedNetwork);
        NetworkSettings::p2pRelayOnly.set(savedRelayOnly);
        NetworkSettings::p2pBootstrap.set(savedNodes);
    }

    void identityDialogOffersStorageChoices()
    {
        P2pIdentityStore store(m_dir.filePath(QStringLiteral("identity")));
        P2pIdentityDialog dialog(P2pIdentityDialog::Mode::Create, &store);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        saveShot(&dialog, QStringLiteral("p2p-identity-create"));
        // 暗号化しないファイルを選ぶと、鍵を使えることを示す
        bool warned = false;
        for (QLabel* label : dialog.findChildren<QLabel*>()) {
            warned = warned || label->text().contains(QStringLiteral("なりすまして"));
        }
        QVERIFY(!warned);
    }

    void confirmDialogShowsUnverifiedOpponent()
    {
        AppActivityState activityA;
        AppActivityState activityB;
        P2pGameCoordinator applicant(P2pGameCoordinator::Deps{nullptr, nullptr, &activityA});
        P2pGameCoordinator host(P2pGameCoordinator::Deps{nullptr, nullptr, &activityB});
        LoopTransport toHost;
        LoopTransport toApplicant;
        toHost.peerId = QByteArray(38, 'a');
        toHost.other = &host;
        toApplicant.peerId = QByteArray(38, 'h');
        toApplicant.other = &applicant;
        const SigningKey applicantKey = SigningKey::generate();
        const SigningKey hostKey = SigningKey::generate();
        P2pKnownPeers known(m_dir.filePath(QStringLiteral("peers.cbor")));
        // 募集側のプレイヤーID に、確認済みの友人として名前を付けてある（名乗っただけの相手にこの記録を出さない）
        QVERIFY(known.recordGameStarted(hostKey.publicKey(), QStringLiteral("募集する人")));
        QVERIFY(known.setLocalName(hostKey.publicKey(), QStringLiteral("親友のA")));
        QVERIFY(known.setVerified(hostKey.publicKey(), true));
        applicant.setEnvironment(&toHost, nullptr, &known, &applicantKey);
        host.setEnvironment(&toApplicant, nullptr, &known, &hostKey);

        P2pGameCoordinator::NegotiationRequest h;
        h.connection = 1;
        h.opponentPeerId = QStringLiteral("a");
        h.opponentPeerIdBytes = toHost.peerId;
        h.role = kRoleHost;
        h.myDisplayName = QStringLiteral("募集する人");
        h.hostTerms = TimeControlTerms{600'000, 30'000};
        P2pGameCoordinator::NegotiationRequest a = h;
        a.opponentPeerId = QStringLiteral("h");
        a.opponentPeerIdBytes = toApplicant.peerId;
        a.role = kRoleApplicant;
        a.myDisplayName = QStringLiteral("申し込む人");
        QVERIFY(host.startNegotiation(h));
        QVERIFY(applicant.startNegotiation(a));
        QTRY_VERIFY(applicant.negotiation()->stage() == Negotiation::Stage::Confirming);

        P2pConfirmDialog dialog(&applicant, &known, false);
        // 取り消されるまでの残りは、開いた時点から出す（最初の時計の知らせを待たない）
        bool remainingShown = false;
        for (QLabel* label : dialog.findChildren<QLabel*>()) {
            remainingShown = remainingShown || label->text().contains(QStringLiteral("秒で取り消されます"));
        }
        QVERIFY(remainingShown);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        bool quoted = false;
        bool unverified = false;
        for (QLabel* label : dialog.findChildren<QLabel*>()) {
            // 相手の表示名はかぎ括弧で囲み、PlainText で出す
            if (label->text().contains(QStringLiteral("「募集する人」"))) {
                quoted = true;
                QCOMPARE(label->textFormat(), Qt::PlainText);
            }
            unverified = unverified || label->text().contains(QStringLiteral("まだ確かめていません"));
            // 署名を確かめる前は、申告の ID で手元の記録（付けた名前・確認済み）を引かない（監査 2026-11-13 P14）
            QVERIFY2(!label->text().contains(QStringLiteral("親友のA")), qPrintable(label->text()));
            QVERIFY2(!label->text().contains(QStringLiteral("確認済みの相手")), qPrintable(label->text()));
            QVERIFY2(!label->text().contains(QStringLiteral("確認済みとして")), qPrintable(label->text()));
        }
        QVERIFY(quoted);
        QVERIFY(unverified);
        saveShot(&dialog, QStringLiteral("p2p-confirm"));
        // 閉じると交渉を取りやめる
        dialog.reject();
        QTRY_VERIFY(applicant.phase() == P2pGameCoordinator::Phase::Idle);
    }

    /// 「対局者」の欄: 既定は人間。登録エンジンを選ぶと、その名前と場所が対局者になる。選択はプレイヤーごとに保存し、
    /// 保存したエンジンが削除されていれば人間に戻す（相手に伝える対局者を、黙って別のエンジンに替えない）
    void windowChoosesHumanOrRegisteredEngine()
    {
        const QList<EngineEntry> engines{{QStringLiteral("Engine A"), QStringLiteral("/engines/a"), QStringLiteral("A")},
                                         {QStringLiteral("Engine B"), QStringLiteral("/engines/b"), QStringLiteral("B")}};
        EngineProfileStore::saveEngines(engines);
        NetworkSettings::p2pPlayAsEngine(1).set(false);
        AppActivityState activity;
        GameBoard board;
        ShogiClock clock;
        P2pGameCoordinator game(P2pGameCoordinator::Deps{&board, &clock, &activity});
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("engine-choice")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, &profile);
        {
            P2pWindow window(&lobby, &game);
            auto* kind = window.findChild<QComboBox*>(QStringLiteral("p2pPlayerKind"));
            auto* settings = window.findChild<QPushButton*>(QStringLiteral("p2pEngineSettings"));
            QVERIFY(kind && settings);
            // 先頭が「人間」、その後に登録エンジン。既定は人間
            QCOMPARE(kind->count(), 3);
            QCOMPARE(kind->itemText(0), QStringLiteral("人間"));
            QCOMPARE(kind->currentIndex(), 0);
            QVERIFY(!window.playerSetup().isEngine());
            QVERIFY(!settings->isEnabled());
            kind->setCurrentIndex(2);
            const P2pGameCoordinator::PlayerSetup player = window.playerSetup();
            QVERIFY(player.isEngine());
            QCOMPARE(player.engineName, QStringLiteral("Engine B"));
            QCOMPARE(player.enginePath, QStringLiteral("/engines/b"));
            QVERIFY(settings->isEnabled());
            QVERIFY(QMetaObject::invokeMethod(&window, "saveSettings"));
            saveShot(&window, QStringLiteral("p2p-window-engine"));
        }
        {
            // 開き直すと、選んだエンジンに戻る
            P2pWindow window(&lobby, &game);
            QCOMPARE(window.playerSetup().engineName, QStringLiteral("Engine B"));
            // 人間に戻して保存しても、前に選んだエンジンの名前は残す（次にエンジンを選ぶときのため）
            window.findChild<QComboBox*>(QStringLiteral("p2pPlayerKind"))->setCurrentIndex(0);
            QVERIFY(QMetaObject::invokeMethod(&window, "saveSettings"));
            QVERIFY(!NetworkSettings::p2pPlayAsEngine(1)());
        }
        {
            P2pWindow window(&lobby, &game);
            QVERIFY(!window.playerSetup().isEngine());
        }
        // 選んだエンジンが登録から消えたら、別のエンジンではなく人間にする
        NetworkSettings::p2pPlayAsEngine(1).set(true);
        EngineProfileStore::saveEngines({engines.first()});
        {
            P2pWindow window(&lobby, &game);
            QVERIFY(!window.playerSetup().isEngine());
            // 開いている間に登録が変わっても同じ（閉じて開き直すと一覧を読み直す）
            window.findChild<QComboBox*>(QStringLiteral("p2pPlayerKind"))->setCurrentIndex(1);
            QCOMPARE(window.playerSetup().engineName, QStringLiteral("Engine A"));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QCOMPARE(window.playerSetup().engineName, QStringLiteral("Engine A"));
            window.hide();
            EngineProfileStore::saveEngines({engines.last()});
            window.show();
            QVERIFY(!window.playerSetup().isEngine());
            QCOMPARE(window.findChild<QComboBox*>(QStringLiteral("p2pPlayerKind"))->count(), 2);
        }
        EngineProfileStore::saveEngines({});
        NetworkSettings::p2pPlayAsEngine(1).set(false);
    }

    /// 条件確認画面は、だれが指すか（自分と相手。人間かエンジンか）を出す。エンジンで指す側は、エンジンの初期化が
    /// 済むまで開始を押せない
    void confirmDialogShowsWhoPlays()
    {
        AppActivityState activityA;
        AppActivityState activityB;
        EngineSlots engines;
        P2pGameCoordinator applicant(P2pGameCoordinator::Deps{nullptr, nullptr, &activityA});
        P2pGameCoordinator host(P2pGameCoordinator::Deps{nullptr, nullptr, &activityB, &engines});
        LoopTransport toHost;
        LoopTransport toApplicant;
        toHost.peerId = QByteArray(38, 'a');
        toHost.other = &host;
        toApplicant.peerId = QByteArray(38, 'h');
        toApplicant.other = &applicant;
        const SigningKey applicantKey = SigningKey::generate();
        const SigningKey hostKey = SigningKey::generate();
        P2pKnownPeers known(m_dir.filePath(QStringLiteral("peers-kinds.cbor")));
        applicant.setEnvironment(&toHost, nullptr, &known, &applicantKey);
        host.setEnvironment(&toApplicant, nullptr, &known, &hostKey);

        P2pGameCoordinator::NegotiationRequest h;
        h.connection = 1;
        h.opponentPeerId = QStringLiteral("a");
        h.opponentPeerIdBytes = toHost.peerId;
        h.role = kRoleHost;
        h.myDisplayName = QStringLiteral("募集する人");
        h.hostTerms = TimeControlTerms{600'000, 30'000};
        P2pGameCoordinator::NegotiationRequest a = h;
        a.opponentPeerId = QStringLiteral("h");
        a.opponentPeerIdBytes = toApplicant.peerId;
        a.role = kRoleApplicant;
        a.myDisplayName = QStringLiteral("申し込む人");
        // 招待した側がエンジン（初期化に時間のかかるモックのエンジン）
        h.player.kind = PlayerKind::Engine;
        h.player.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        h.player.engineName = QStringLiteral("確認画面のエンジン");
        qputenv("SBQ_MATCH_INIT_DELAY_MS", "500");
        QVERIFY(host.startNegotiation(h));
        QVERIFY(applicant.startNegotiation(a));
        QTRY_VERIFY(applicant.negotiation()->stage() == Negotiation::Stage::Confirming);
        QTRY_VERIFY(host.negotiation()->stage() == Negotiation::Stage::Confirming);
        qunsetenv("SBQ_MATCH_INIT_DELAY_MS");

        const auto textOf = [](const QDialog& dialog, const QString& name) {
            const QLabel* label = dialog.findChild<QLabel*>(name);
            return label ? label->text() : QString();
        };
        // 人間の側: 相手がエンジンで指すことを、開始を押す前に知る
        P2pConfirmDialog applicantDialog(&applicant, &known, false);
        const QString applicantPlayers = textOf(applicantDialog, QStringLiteral("p2pConfirmPlayers"));
        QVERIFY2(applicantPlayers.contains(QStringLiteral("あなた（人間）が盤で指します")), qPrintable(applicantPlayers));
        QVERIFY2(applicantPlayers.contains(QStringLiteral("相手の手は、エンジンが指します")), qPrintable(applicantPlayers));
        const QString conditions = textOf(applicantDialog, QStringLiteral("p2pConfirmConditions"));
        QVERIFY2(conditions.contains(QStringLiteral("エンジンが指す対局です")), qPrintable(conditions));
        QVERIFY2(!conditions.contains(QStringLiteral("人間どうしの対局")), qPrintable(conditions));
        auto* applicantStart = applicantDialog.findChild<QPushButton*>(QStringLiteral("p2pConfirmStart"));
        QVERIFY(applicantStart && applicantStart->isEnabled());
        applicantDialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applicantDialog));
        saveShot(&applicantDialog, QStringLiteral("p2p-confirm-engine-opponent"));

        // エンジンの側: エンジンの名前を出し、初期化が済むまで開始を押せない
        P2pConfirmDialog hostDialog(&host, &known, false);
        const QString hostPlayers = textOf(hostDialog, QStringLiteral("p2pConfirmPlayers"));
        QVERIFY2(hostPlayers.contains(QStringLiteral("エンジン「確認画面のエンジン」が指します")), qPrintable(hostPlayers));
        QVERIFY2(hostPlayers.contains(QStringLiteral("相手の手は、人間が指します")), qPrintable(hostPlayers));
        auto* hostStart = hostDialog.findChild<QPushButton*>(QStringLiteral("p2pConfirmStart"));
        QVERIFY(hostStart);
        QVERIFY(host.isEngineStarting());
        QVERIFY(!hostStart->isEnabled());
        QCOMPARE(hostStart->text(), QStringLiteral("エンジンを起動しています…"));
        QTRY_VERIFY(hostStart->isEnabled());
        QCOMPARE(hostStart->text(), QStringLiteral("開始"));
        hostDialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&hostDialog));
        saveShot(&hostDialog, QStringLiteral("p2p-confirm-engine-self"));
        // 先後の文とその決め方の文は、訳に任せた形でつなぐ（英語では間に空白が入る。以前は「…Black.Sides…」と続いていた）
        {
            QTranslator translator;
            QVERIFY(translator.load(QStringLiteral(TRANSLATION_DIR "/ShogiBoardQ_en.qm")));
            QVERIFY(qApp->installTranslator(&translator));
            P2pConfirmDialog english(&applicant, &known, false);
            const QString firstLine = textOf(english, QStringLiteral("p2pConfirmConditions")).section(QLatin1Char('\n'), 0, 0);
            qApp->removeTranslator(&translator);
            QVERIFY2(firstLine.startsWith(QStringLiteral("You play ")), qPrintable(firstLine));
            QVERIFY2(firstLine.contains(QStringLiteral(". Sides ")), qPrintable(firstLine));
        }
        // 取りやめると、起動したエンジンを手放す
        hostDialog.reject();
        QTRY_VERIFY(host.phase() == P2pGameCoordinator::Phase::Idle);
        QVERIFY(!engines.engineInSlot(1));
        QTRY_VERIFY(applicant.phase() == P2pGameCoordinator::Phase::Idle);
    }

    /// 対局が始まったら、主画面の盤を自分の側から見る向きにする（後手なら反転。通信対局と同じ）
    void boardFacesOwnSide()
    {
        Seat applicant(QByteArray(38, 'c'));
        Seat host(QByteArray(38, 'j'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("flip-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("flip-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("flip-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("flip-h.cbor")));
        // どちらの盤も反転していない状態から始める（後手の側は反転しなければならない）
        QVERIFY(!applicant.view->flipMode() && !host.view->flipMode());
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
        QTRY_VERIFY(applicant.game.match() && host.game.match());
        QVERIFY(applicant.game.match()->iAmBlack() != host.game.match()->iAmBlack());
        for (Seat* seat : {&applicant, &host}) {
            QCOMPARE(seat->view->flipMode(), !seat->game.match()->iAmBlack());
        }
    }

    /// 終局の結果と双方での確定は、それぞれの側に1つの知らせで出す（結果の一致と確定で2つ続けて出さない）
    void gameEndIsOneNotification()
    {
        Seat applicant(QByteArray(38, 'e'));
        Seat host(QByteArray(38, 'k'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("end-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("end-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("end-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("end-h.cbor")));
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));

        NotificationPresenter::ScopedCapture capture;
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
        QTRY_VERIFY(black.game.canResign());
        black.game.resign();
        QTRY_VERIFY(applicant.game.phase() == P2pGameCoordinator::Phase::Finished
                    && host.game.phase() == P2pGameCoordinator::Phase::Finished);
        QCoreApplication::processEvents();
        QCOMPARE(capture.notices().size(), 2);
        for (const QString& notice : capture.notices()) {
            const QStringList lines = notice.split(QLatin1Char('\n'));
            QVERIFY2(lines.size() == 2, qPrintable(notice));
            QVERIFY2(lines.at(0).startsWith(QStringLiteral("終局: ")) && lines.at(0).contains(QStringLiteral("投了")),
                     qPrintable(notice));
            QCOMPARE(lines.at(1), QStringLiteral("結果が一致し、双方で確定しました。"));
        }
    }

    /// 相手の申し出は待たずに尋ね、答えるまでの間も対局が進む（監査 2026-11-12 B2）。手数が進んで無効になった箱と、
    /// 同じ手数の次の申し出で置き換わった箱は、答えとして扱わずに閉じる
    void offerIsAskedWithoutBlocking()
    {
        Seat applicant(QByteArray(38, 'f'));
        Seat host(QByteArray(38, 'm'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("offer-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("offer-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("offer-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("offer-h.cbor")));
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
        Seat& white = applicant.game.match()->iAmBlack() ? host : applicant;
        const auto visibleBox = [](Seat& seat, const QString& word) -> QMessageBox* {
            const QList<QMessageBox*> boxes = seat.window.findChildren<QMessageBox*>(QStringLiteral("p2pOfferBox"));
            for (QMessageBox* box : boxes) {
                if (box->isVisible() && box->text().contains(word)) return box;
            }
            return nullptr;
        };

        // 先手が引き分けを申し出る: 後手には待たない箱が出る
        QTRY_VERIFY(black.game.isHumanToMove());
        black.game.offerDraw();
        QTRY_VERIFY(visibleBox(white, QStringLiteral("引き分け")));
        QPointer<QMessageBox> drawBox = visibleBox(white, QStringLiteral("引き分け"));
        QVERIFY(!drawBox->isModal());

        // 答える前に先手が指すと、申し出は無効になり、箱は閉じる
        QCOMPARE(black.game.playMove(UsiMove::fromString(u"2g2f")), GameChannel::MoveResult::Sent);
        QTRY_VERIFY(white.game.match()->lastState().ply == 1);
        QTRY_VERIFY(!drawBox);
        QTRY_VERIFY(black.game.match()->lastState().ply == 1 && !black.game.match()->hasPendingMove());

        // 同じ手数で引き分けの後に中断を申し出る: 引き分けの箱を閉じても、中断の申し出を断らない
        QSignalSpy declined(&black.game, &P2pGameCoordinator::proposalDeclined);
        black.game.offerDraw();
        QTRY_VERIFY(visibleBox(white, QStringLiteral("引き分け")));
        black.game.offerAbort();
        QTRY_VERIFY(visibleBox(white, QStringLiteral("中断")));
        QTRY_VERIFY(!visibleBox(white, QStringLiteral("引き分け")));
        // 出してすぐは「承諾」を押せない（箱が入れ替わった瞬間のクリックで、別の申し出を承諾しない。監査 2026-11-13）
        QAbstractButton* acceptAbort = visibleBox(white, QStringLiteral("中断"))->button(QMessageBox::Yes);
        QVERIFY(!acceptAbort->isEnabled());
        QTRY_VERIFY(acceptAbort->isEnabled());
        acceptAbort->click();
        QTRY_VERIFY(black.game.match()->agreedResult() && white.game.match()->agreedResult());
        QCOMPARE(black.game.match()->agreedResult()->cause, Cause::AgreedAbort);
        QCOMPARE(declined.count(), 0);
    }

    /// 相手が断った申し出は、同じ手数でももう一度申し出られ、相手にもう一度尋ねる（断られた後に「中断」を押し直しても、
    /// 相手に何も出なかった）
    void declinedOfferIsAskedAgain()
    {
        Seat applicant(QByteArray(38, 'h'));
        Seat host(QByteArray(38, 'p'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("reoffer-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("reoffer-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("reoffer-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("reoffer-h.cbor")));
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
        Seat& white = applicant.game.match()->iAmBlack() ? host : applicant;
        const auto abortBox = [&white]() -> QMessageBox* {
            const QList<QMessageBox*> boxes = white.window.findChildren<QMessageBox*>(QStringLiteral("p2pOfferBox"));
            for (QMessageBox* box : boxes) {
                if (box->isVisible() && box->text().contains(QStringLiteral("中断"))) return box;
            }
            return nullptr;
        };

        // 申し出た側に出ている「相手が申し出を断りました」の知らせの数
        const auto declinedNotices = [&black]() {
            int count = 0;
            const QList<QMessageBox*> boxes = black.window.findChildren<QMessageBox*>();
            for (QMessageBox* box : boxes) {
                if (box->isVisible() && box->text().contains(QStringLiteral("断りました"))) ++count;
            }
            return count;
        };

        // 手番の先手が中断を申し出て、後手が断る。同じ手数のまま、これを繰り返せる
        QTRY_VERIFY(black.game.isHumanToMove());
        for (int round = 1; round <= 3; ++round) {
            black.game.offerAbort();
            QTRY_VERIFY2(abortBox(), qPrintable(QStringLiteral("round %1").arg(round)));
            // 答えを待っている間に押し直しても、箱は1つのまま
            QPointer<QMessageBox> box = abortBox();
            black.game.offerAbort();
            QCoreApplication::processEvents();
            QCOMPARE(abortBox(), box.data());
            QSignalSpy declined(&black.game, &P2pGameCoordinator::proposalDeclined);
            box->button(QMessageBox::Cancel)->click();
            // 申し出た側に、断られたことを知らせる。前の知らせを閉じていなくても、知らせは重ねない
            QTRY_COMPARE(declined.count(), 1);
            QTRY_COMPARE(declinedNotices(), 1);
            QTRY_VERIFY(!abortBox());
            QVERIFY(black.game.isInProgress() && white.game.isInProgress());
            QCOMPARE(black.game.match()->lastState().ply, quint32(0));
        }

        // もう一度申し出て、後手が承諾すれば、合意の中断で終わる
        black.game.offerAbort();
        QTRY_VERIFY(abortBox());
        QAbstractButton* accept = abortBox()->button(QMessageBox::Yes);
        QTRY_VERIFY(accept->isEnabled());
        accept->click();
        QTRY_VERIFY(black.game.match()->agreedResult() && white.game.match()->agreedResult());
        QCOMPARE(black.game.match()->agreedResult()->cause, Cause::AgreedAbort);
    }

    /// 申し出た側にも、申し出たことと、相手の返事を待っていることを知らせる（相手には尋ねる箱が出るが、申し出た側には
    /// 何も出ず、「中断」を押せたのかが分からなかった）。知らせは待たずに出し、相手が答えた・手数が進んだ・対局が
    /// 終わったら閉じる。P2P対局のウィンドウの「対局中」の状態にも出す。送れなかったときは、そのことを知らせる
    void offererIsToldThatTheOfferIsWaiting()
    {
        Seat applicant(QByteArray(38, 'j'));
        Seat host(QByteArray(38, 'q'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("waiting-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("waiting-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("waiting-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("waiting-h.cbor")));
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
        Seat& white = applicant.game.match()->iAmBlack() ? host : applicant;
        // 申し出る先手の P2P対局のウィンドウ（「対局中」の状態の文を見る）
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("waiting")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&black.activity, &black.game}, &profile);
        P2pWindow p2p(&lobby, &black.game);
        auto* status = p2p.findChild<QLabel*>(QStringLiteral("p2pGameStatus"));
        QVERIFY(status);

        const auto boxOf = [](Seat& seat, const QString& word) -> QMessageBox* {
            const QList<QMessageBox*> boxes = seat.window.findChildren<QMessageBox*>();
            for (QMessageBox* box : boxes) {
                if (box->isVisible() && box->text().contains(word)) return box;
            }
            return nullptr;
        };
        // 返事を待っている知らせの数
        const auto waitingBoxes = [](Seat& seat) {
            int count = 0;
            const QList<QMessageBox*> boxes = seat.window.findChildren<QMessageBox*>(QStringLiteral("p2pOfferWaitingBox"));
            for (QMessageBox* box : boxes) {
                if (box->isVisible()) ++count;
            }
            return count;
        };

        // 中断を申し出る: 申し出た側に、待たない知らせが出る。P2P対局のウィンドウの状態にも出る
        QTRY_VERIFY(black.game.isHumanToMove());
        QCOMPARE(waitingBoxes(black), 0);
        black.game.offerAbort();
        QCOMPARE(waitingBoxes(black), 1);
        QMessageBox* waiting = boxOf(black, QStringLiteral("相手に対局の中断を申し出ました"));
        QVERIFY(waiting);
        QVERIFY(!waiting->isModal());
        QVERIFY(waiting->text().contains(QStringLiteral("相手の返事を待っています")));
        QVERIFY2(status->text().contains(QStringLiteral("中断を申し出ています")), qPrintable(status->text()));
        // 相手には尋ねる箱が出る。相手の側には、待っている知らせは出ない
        QTRY_VERIFY(boxOf(white, QStringLiteral("相手が対局の中断を申し出ました")));
        QCOMPARE(waitingBoxes(white), 0);

        // 答えを待つ間に押し直しても、知らせは1つのまま（相手にも尋ね直さない）
        black.game.offerAbort();
        QCOMPARE(waitingBoxes(black), 1);

        // 相手が断ると、待っている知らせを閉じて、断られたことを知らせる
        boxOf(white, QStringLiteral("相手が対局の中断を申し出ました"))->button(QMessageBox::Cancel)->click();
        QTRY_VERIFY(boxOf(black, QStringLiteral("相手が申し出を断りました")));
        QCOMPARE(waitingBoxes(black), 0);
        QVERIFY2(!status->text().contains(QStringLiteral("申し出ています")), qPrintable(status->text()));

        // 引き分けを申し出る: 断られた知らせを閉じて、待っている知らせを出す
        black.game.offerDraw();
        QCOMPARE(waitingBoxes(black), 1);
        QVERIFY(boxOf(black, QStringLiteral("相手に引き分けを申し出ました")));
        QVERIFY(!boxOf(black, QStringLiteral("相手が申し出を断りました")));
        QVERIFY2(status->text().contains(QStringLiteral("引き分けを申し出ています")), qPrintable(status->text()));
        QTRY_VERIFY(boxOf(white, QStringLiteral("相手が引き分けを申し出ました")));

        // 指した手の確認を待つ間は、申し出を送れない。送れなかったことを知らせる（待っている知らせは、引き分けのまま）
        QCOMPARE(black.game.playMove(UsiMove::fromString(u"7g7f")), GameChannel::MoveResult::Sent);
        black.game.offerAbort();
        QVERIFY(boxOf(black, QStringLiteral("今は申し出を送れません")));
        QVERIFY(boxOf(black, QStringLiteral("相手に引き分けを申し出ました")));
        // 返事の前に手数が進むと、申し出は無効になり、待っている知らせは閉じる
        QTRY_VERIFY(black.game.match()->lastState().ply == 1 && !black.game.match()->hasPendingMove());
        QTRY_COMPARE(waitingBoxes(black), 0);
        QTRY_VERIFY(!status->text().contains(QStringLiteral("申し出ています")));

        // 手番でない側も申し出られる（送れなかった知らせは、送れたら閉じる）
        black.game.offerAbort();
        QCOMPARE(waitingBoxes(black), 1);
        QVERIFY(!boxOf(black, QStringLiteral("今は申し出を送れません")));
        QTRY_VERIFY(boxOf(white, QStringLiteral("相手が対局の中断を申し出ました")));
        // 尋ねられた側が、箱で答えずに自分も中断を申し出ると、同じ申し出なので合意の中断になる。結果が決まったので、
        // どちらの側にも、待っている知らせ・送れなかった知らせは残らず、終局の知らせが出る
        white.game.offerAbort();
        QTRY_VERIFY(black.game.match()->agreedResult() && white.game.match()->agreedResult());
        QCOMPARE(black.game.match()->agreedResult()->cause, Cause::AgreedAbort);
        QTRY_VERIFY(boxOf(black, QStringLiteral("合意による中断")) && boxOf(white, QStringLiteral("合意による中断")));
        for (Seat* seat : {&black, &white}) {
            QCOMPARE(waitingBoxes(*seat), 0);
            QVERIFY(!boxOf(*seat, QStringLiteral("今は申し出を送れません")));
            QVERIFY(!boxOf(*seat, QStringLiteral("相手が対局の中断を申し出ました")));
        }
        QTRY_VERIFY(!status->text().contains(QStringLiteral("申し出ています")));
    }

    /// 記録と相手の表は、通信プロセスの知らせ（数秒ごと）では作り直さず、対局の後・タブを開いたときに作り直す。
    /// 列は中身が切れない幅にし、行番号は出さない
    void recordTablesAreRebuiltOnlyWhenNeeded()
    {
        Seat applicant(QByteArray(38, 'd'));
        Seat host(QByteArray(38, 'k'));
        P2pPlayerProfile profile(m_dir.filePath(QStringLiteral("records")));
        QVERIFY(profile.open());
        P2pLobbyService lobby(P2pLobbyService::Deps{&host.activity, &host.game}, &profile);
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("records-a")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("records-a.cbor")));
        P2pWindow window(&lobby, &host.game);
        window.resize(820, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* history = window.findChild<QTableWidget*>(QStringLiteral("p2pRecordTable"));
        auto* peers = window.findChild<QTableWidget*>(QStringLiteral("p2pPeerTable"));
        QVERIFY(history && peers);
        QCOMPARE(history->rowCount(), 0);

        // 1局指して投了で終える。対局が終わったら記録の表に1行、相手の表に1人が入る
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, profile.records(), profile.knownPeers()));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;  // 投了できるのは手番の側
        const bool hostIsBlack = &black == &host;
        QTRY_VERIFY(black.game.canResign());
        black.game.resign();
        QTRY_COMPARE(history->rowCount(), 1);
        QTRY_COMPARE(peers->rowCount(), 1);
        // 結果は、自分から見た勝ち負けで出す（先後と「先手の勝ち」を見比べなくてよい）。終わり方はツールチップで読める
        constexpr int kResultColumn = 3;
        QCOMPARE(history->horizontalHeaderItem(kResultColumn)->text(), QStringLiteral("あなたの結果"));
        QTRY_COMPARE(history->item(0, kResultColumn)->text(),
                     hostIsBlack ? QStringLiteral("負け（投了）") : QStringLiteral("勝ち（投了）"));
        QVERIFY2(history->item(0, kResultColumn)->toolTip().contains(QStringLiteral("双方で確定")),
                 qPrintable(history->item(0, kResultColumn)->toolTip()));
        // 相手の表は、名前の列が先（プレイヤーIDは確かめるときに見る）
        QCOMPARE(peers->horizontalHeaderItem(0)->text(), QStringLiteral("手元の名前"));
        QCOMPARE(peers->horizontalHeaderItem(2)->text(), QStringLiteral("プレイヤーID"));

        for (QTableWidget* table : {history, peers}) {
            QVERIFY(table->verticalHeader()->isHidden());
            // 最後の列（伸ばす列）を除き、見出しと中身が切れない幅
            for (int column = 0; column + 1 < table->columnCount(); ++column) {
                const QAbstractItemView* view = table;  // QTableView では protected
                const int needed = qMax(view->sizeHintForColumn(column), table->horizontalHeader()->sectionSizeHint(column));
                QVERIFY2(table->columnWidth(column) >= needed,
                         qPrintable(QStringLiteral("column %1: %2 < %3").arg(column).arg(table->columnWidth(column)).arg(needed)));
            }
        }

        // 選んだ行は、接続の状態が変わっても作り直さない（選択も行の部品もそのまま）
        // （作り直したかは、行の部品に付けた印が残るかで見る）
        constexpr int kMarkRole = Qt::UserRole + 100;
        history->selectRow(0);
        history->item(0, 0)->setData(kMarkRole, true);
        window.refresh();
        emit lobby.networkStatusChanged();
        emit lobby.statusChanged();
        QCoreApplication::processEvents();
        QVERIFY(history->item(0, 0)->data(kMarkRole).toBool());
        QVERIFY(history->item(0, 0)->isSelected());

        // 記録のタブを開くと作り直す。選んでいた対局は選んだまま
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("p2pTabs"));
        QVERIFY(tabs);
        tabs->setCurrentWidget(history->parentWidget());
        QVERIFY(tabs->currentWidget() == history->parentWidget());
        QVERIFY(!history->item(0, 0)->data(kMarkRole).toBool());
        QVERIFY(history->item(0, 0)->isSelected());
        // 見えている表は、最後の列を右端まで伸ばす（右に空きを残さない）
        QHeaderView* header = history->horizontalHeader();
        const int last = history->columnCount() - 1;
        QTRY_VERIFY2(header->sectionViewportPosition(last) + header->sectionSize(last) >= history->viewport()->width(),
                     qPrintable(QStringLiteral("%1 + %2 < %3")
                                    .arg(header->sectionViewportPosition(last))
                                    .arg(header->sectionSize(last))
                                    .arg(history->viewport()->width())));

        // 証跡を検証した結果は覚えておき、証跡が変わったときだけ検証し直す（監査 2026-11-12 S16）
        constexpr int kCategoryColumn = 4;
        QCOMPARE(history->item(0, kCategoryColumn)->text(), QStringLiteral("双方で確定"));
        QFile evidence(history->item(0, 0)->data(Qt::UserRole).toString());
        QVERIFY(evidence.open(QIODevice::Append));
        evidence.write("x");
        evidence.close();
        window.reloadRecords();
        QCOMPARE(history->item(0, kCategoryColumn)->text(), QStringLiteral("検証に失敗"));
    }

    /// 「対局したことのない相手の表示名を出さない」は、対局記録でも対局したことのない相手（「相手」タブで
    /// 記録を削除した相手）の名前だけを隠す。隠した名前はツールチップにも出さない
    void recordsHideOnlyUnknownNames()
    {
        Seat applicant(QByteArray(38, 'f'));
        Seat host(QByteArray(38, 'm'));
        P2pRecordStore applicantStore(m_dir.filePath(QStringLiteral("hide-a")));
        P2pRecordStore hostStore(m_dir.filePath(QStringLiteral("hide-h")));
        P2pKnownPeers applicantKnown(m_dir.filePath(QStringLiteral("hide-a.cbor")));
        P2pKnownPeers hostKnown(m_dir.filePath(QStringLiteral("hide-h.cbor")));
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, &hostStore, &hostKnown));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;
        QTRY_VERIFY(black.game.canResign());
        black.game.resign();
        QTRY_VERIFY(applicant.game.phase() == P2pGameCoordinator::Phase::Finished
                    && host.game.phase() == P2pGameCoordinator::Phase::Finished);
        QTRY_COMPARE(hostStore.history().size(), 1);

        P2pHistoryPanel panel(&hostStore, &hostKnown);
        panel.reload();  // 表はタブを開いたときに作る
        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(table);
        const auto opponentCell = [table]() { return table->item(0, 1); };
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(opponentCell()->text(), QStringLiteral("申し込む人"));

        // 対局した相手の名前は、設定を有効にしても出す
        panel.setHideUnknownNames(true);
        QCOMPARE(opponentCell()->text(), QStringLiteral("申し込む人"));

        // 「相手」タブで記録を削除すると、対局したことのない相手として名前を隠す（ツールチップにも出さない）
        hostKnown.remove(applicant.key.publicKey());
        panel.reload();
        QCOMPARE(opponentCell()->text(), QStringLiteral("名前は非表示"));
        QVERIFY(!opponentCell()->toolTip().contains(QStringLiteral("申し込む人")));

        // 設定を外せば出す
        panel.setHideUnknownNames(false);
        QCOMPARE(opponentCell()->text(), QStringLiteral("申し込む人"));
    }
};

QTEST_MAIN(TestP2pUi)
#include "tst_p2p_ui.moc"
