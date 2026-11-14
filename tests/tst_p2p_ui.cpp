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
#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "gameboard.h"
#include "mainwindowappearancecontroller.h"
#include "networksettings.h"
#include "notificationpresenter.h"
#include "p2pconfirmdialog.h"
#include "p2pgamecoordinator.h"
#include "p2pgameuicontroller.h"
#include "p2phistorypanel.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2plobbyservice.h"
#include "p2pplayerprofile.h"
#include "p2pplayerslot.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "p2pwindow.h"
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
            if (button->text() == QStringLiteral("招待文字列を作る") || button->text() == QStringLiteral("招待文字列を確かめる")
                || button->text() == QStringLiteral("接続する")) {
                QVERIFY2(!button->isEnabled(), qPrintable(button->text()));
            }
        }
        // 参加できるネットワークはビルドの一覧だけ
        auto* network = window.findChild<QComboBox*>(QStringLiteral("p2pNetwork"));
        QVERIFY(network);
        QCOMPARE(network->count(), 2);
        // チェックの文字は折り返せないので、短くする（長い説明は折り返すラベルに分ける。長いと、訳した画面で
        // ウィンドウの最小の幅が広がる）
        for (QCheckBox* box : window.findChildren<QCheckBox*>()) {
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
        QVERIFY(QMetaObject::invokeMethod(&window, "onCopyInvite"));
        const QMimeData* copied = QApplication::clipboard()->mimeData();
        QVERIFY(copied);
        QCOMPARE(copied->text(), QStringLiteral("sbqinv1.test"));
        QCOMPARE(copied->data(QStringLiteral("x-kde-passwordManagerHint")), QByteArrayLiteral("secret"));
        // 期限（この試験では作っていないので過ぎている）を過ぎると、まだこの文字列のままなら消す
        QTRY_VERIFY(QApplication::clipboard()->text().isEmpty());
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
        const QList<QTableWidget*> tables = window.findChildren<QTableWidget*>();
        QTableWidget* history = nullptr;
        QTableWidget* peers = nullptr;
        for (QTableWidget* table : tables) {
            (table->columnCount() == 6 ? history : peers) = table;
        }
        QVERIFY(history && peers);
        QCOMPARE(history->rowCount(), 0);

        // 1局指して投了で終える。対局が終わったら記録の表に1行、相手の表に1人が入る
        QVERIFY(startGame(applicant, host, &applicantStore, &applicantKnown, profile.records(), profile.knownPeers()));
        Seat& black = applicant.game.match()->iAmBlack() ? applicant : host;  // 投了できるのは手番の側
        QTRY_VERIFY(black.game.canResign());
        black.game.resign();
        QTRY_COMPARE(history->rowCount(), 1);
        QTRY_COMPARE(peers->rowCount(), 1);

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

    /// 「対局したことのない相手の表示名を出さない」は、対局記録でも対局したことのない相手（「相手の管理」で
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

        // 「相手の管理」で記録を削除すると、対局したことのない相手として名前を隠す（ツールチップにも出さない）
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
