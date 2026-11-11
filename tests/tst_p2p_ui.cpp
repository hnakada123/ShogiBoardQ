/// @file tst_p2p_ui.cpp
/// @brief P2P対局の画面の試験（ウィンドウ・条件確認画面・プレイヤー鍵のダイアログを作り、表示と操作の可否を確かめる）
///
/// SBQ_P2P_UI_SHOTS にフォルダを指定すると、各画面の画像を保存する（見た目の確認用）。

#include <QtTest>

#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "gameboard.h"
#include "p2pconfirmdialog.h"
#include "p2pgamecoordinator.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2plobbyservice.h"
#include "p2ptransport.h"
#include "p2pwindow.h"
#include "shogiclock.h"

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
    void withdrawOffer() override {}
};

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
        P2pLobbyService lobby(P2pLobbyService::Deps{&activity, &game}, m_dir.filePath(QStringLiteral("w")));
        QCOMPARE(lobby.prepare(), P2pLobbyService::StartFailure::None);
        P2pWindow window(&lobby, &game);
        window.resize(820, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* tabs = window.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 6);
        // 接続していなければ募集・招待はできない
        for (QPushButton* button : window.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("募集を出す") || button->text() == QStringLiteral("申し込む")
                || button->text() == QStringLiteral("招待文字列を作る")) {
                QVERIFY2(!button->isEnabled(), qPrintable(button->text()));
            }
        }
        // 参加できるネットワークはビルドの一覧だけ
        auto* network = window.findChild<QComboBox*>();
        QVERIFY(network);
        QCOMPARE(network->count(), 2);
        saveShot(&window, QStringLiteral("p2p-window-play"));
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            saveShot(&window, QStringLiteral("p2p-window-tab%1").arg(i));
        }
        // 自動化 API を有効にしたときは使い始めない
        lobby.disableForAutomation();
        QCOMPARE(lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::AutomationEnabled);
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
        applicant.setEnvironment(&toHost, nullptr, &known, &applicantKey);
        host.setEnvironment(&toApplicant, nullptr, &known, &hostKey);

        P2pGameCoordinator::NegotiationRequest h;
        h.connection = 1;
        h.opponentPeerId = QStringLiteral("a");
        h.opponentPeerIdBytes = toHost.peerId;
        h.role = kRoleHost;
        h.myDisplayName = QStringLiteral("募集する人");
        h.offerId = zeroHash();
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
        }
        QVERIFY(quoted);
        QVERIFY(unverified);
        saveShot(&dialog, QStringLiteral("p2p-confirm"));
        // 閉じると交渉を取りやめる
        dialog.reject();
        QTRY_VERIFY(applicant.phase() == P2pGameCoordinator::Phase::Idle);
    }
};

QTEST_MAIN(TestP2pUi)
#include "tst_p2p_ui.moc"
