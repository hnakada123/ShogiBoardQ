/// @file p2pgameuicontroller.cpp
/// @brief P2P対局の画面の実装

#include "p2pgameuicontroller.h"

#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include "apppaths.h"
#include "dialogutils.h"
#include "kifufilecontroller.h"
#include "mainwindowappearancecontroller.h"
#include "networksettings.h"
#include "notificationpresenter.h"
#include "p2pconfirmdialog.h"
#include "p2pgamecoordinator.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2plobbyservice.h"
#include "p2pplayerprofile.h"
#include "p2pplayerslot.h"
#include "p2precordstore.h"
#include "p2pwindow.h"
#include "p2pwindowtext.h"
#include "shogiview.h"

namespace {
/// 申し出の箱を出してから「承諾」を押せるまでの時間
constexpr int kOfferAcceptDelayMs = 1000;
/// アプリの終了で、通信プロセスが中断の知らせを送って終わるのを待つ上限（通信プロセスは shutdown の 0.3 秒後に終わる）
constexpr int kExitWaitMs = 1500;
} // namespace

#include <QDir>
#include <QStringList>

P2pGameUiController::P2pGameUiController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    if (m_deps.game) {
        connect(m_deps.game, &P2pGameCoordinator::negotiationChanged, this, &P2pGameUiController::onNegotiationChanged);
        connect(m_deps.game, &P2pGameCoordinator::negotiationEnded, this, &P2pGameUiController::onNegotiationEnded);
        connect(m_deps.game, &P2pGameCoordinator::gameStarted, this, &P2pGameUiController::onGameStarted);
        connect(m_deps.game, &P2pGameCoordinator::offerReceived, this, &P2pGameUiController::onOfferReceived);
        connect(m_deps.game, &P2pGameCoordinator::statusChanged, this, &P2pGameUiController::closeStaleOffer);
        connect(m_deps.game, &P2pGameCoordinator::resultAgreed, this, &P2pGameUiController::closeStaleOffer);
        connect(m_deps.game, &P2pGameCoordinator::proposalDeclined, this, &P2pGameUiController::onProposalDeclined);
        connect(m_deps.game, &P2pGameCoordinator::gameFinished, this, &P2pGameUiController::onGameFinished);
    }
}

P2pGameUiController::~P2pGameUiController() = default;

QWidget* P2pGameUiController::dialogParent() const
{
    return m_window ? static_cast<QWidget*>(m_window) : m_deps.parentWidget;
}

void P2pGameUiController::disableForAutomation()
{
    m_automationDisabled = true;
    if (m_lobby) m_lobby->disableForAutomation();
}

void P2pGameUiController::shutdown()
{
    if (m_lobby) {
        m_lobby->end();
        // 終了の後はイベントループが回らない。中断の知らせが相手に届くよう、通信プロセスが終わるのを短く待つ
        m_lobby->waitForExit(kExitWaitMs);
    }
    if (m_window) m_window->close();
}

bool P2pGameUiController::ensureLobby()
{
    if (m_lobby) return true;
    P2pLobbyService::Deps deps;
    deps.activity = m_deps.activity;
    deps.game = m_deps.game;
    // 同じ PC でほかの ShogiBoardQ が P2P対局を使っていれば、空いている次の番号のプレイヤー（2人目・3人目…）に
    // なる。保存先（プレイヤー鍵・対局記録）はプレイヤーごとに別で、同じ PC の中で対局を試せる
    m_profile = P2pPlayerProfile::acquire(AppPaths::dataDirectory(), this);
    if (!m_profile) {
        NotificationPresenter::warning(
            m_deps.parentWidget, tr("P2P対局"),
            tr("このPCで同時に P2P対局を使える ShogiBoardQ は %1 個までです。ほかの ShogiBoardQ を閉じてから開いてください。")
                .arg(P2pPlayerSlot::kMaxPlayers));
        return false;
    }
    m_lobby = new P2pLobbyService(deps, m_profile, this);
    connect(m_lobby, &P2pLobbyService::peerFailed, this, &P2pGameUiController::onPeerFailed);
    connect(m_profile->identity(), &P2pIdentityStore::finished, this, &P2pGameUiController::onIdentityFinished);
    // p2p/ 全体の大きさが上限を超えていたら、古い記録を消すかを尋ねる（黙って消さない）
    if (m_profile->records()->totalBytes() > P2pRecordStore::kMaxTotalBytes) {
        NotificationPresenter::warning(m_deps.parentWidget, tr("P2P対局"),
                                       tr("P2P対局の記録が 1 GiB を超えています。「対局記録」で古い記録を削除してください。"));
    }
    return true;
}

void P2pGameUiController::openWindow()
{
    if (m_automationDisabled) {
        NotificationPresenter::information(
            m_deps.parentWidget, tr("P2P対局"),
            P2pWindowText::startFailureText(P2pLobbyService::StartFailure::AutomationEnabled));
        return;
    }
    if (!ensureLobby()) return;
    if (!m_window) {
        m_window = new P2pWindow(m_lobby, m_deps.game, m_deps.parentWidget);
        m_window->setAttribute(Qt::WA_DeleteOnClose, false);
        connect(m_window, &P2pWindow::startRequested, this, &P2pGameUiController::onStartRequested);
        connect(m_window, &P2pWindow::stopRequested, this, &P2pGameUiController::onStopRequested);
        connect(m_window, &P2pWindow::recreateIdentityRequested, this, &P2pGameUiController::onRecreateIdentity);
        connect(m_window, &P2pWindow::createInviteRequested, this, &P2pGameUiController::onCreateInviteRequested);
        connect(m_window, &P2pWindow::inspectInviteRequested, this, &P2pGameUiController::onInspectInviteRequested);
        connect(m_window, &P2pWindow::connectInviteRequested, this, &P2pGameUiController::onConnectInviteRequested);
        // 投了と合意中断は、メニュー・ツールバーと同じ操作にする
        if (m_deps.resignAction) connect(m_window, &P2pWindow::resignRequested, m_deps.resignAction, &QAction::trigger);
        if (m_deps.breakOffAction) {
            connect(m_window, &P2pWindow::offerAbortRequested, m_deps.breakOffAction, &QAction::trigger);
        }
        connect(m_window, &P2pWindow::offerDrawRequested, this, &P2pGameUiController::onOfferDrawRequested);
        connect(m_window, &P2pWindow::leaveGameRequested, this, &P2pGameUiController::onLeaveGameRequested);
    }
    m_window->refresh();
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void P2pGameUiController::onStartRequested()
{
    if (!m_lobby || !m_window) return;
#ifndef Q_OS_LINUX
    // 通信プロセスの権限を制限できる OS だけで使う。プレイヤー鍵を作る前に知らせる
    onPeerFailed(PeerProcess::Failure::UnsupportedOs, QString());
    return;
#else
    // 対局が始まると棋譜を作り直すので、未保存の棋譜を先に確かめる（ほかの対局の始め方と同じ。通信プロセスを動かす
    // 前に聞き、交渉の途中にはモーダルを出さない）
    if (m_deps.kifuFiles && !m_deps.kifuFiles->confirmDiscardUnsaved()) return;
    if (!m_lobby || !m_window || !m_profile) return;
    P2pIdentityStore* identity = m_profile->identity();
    if (identity->isUnlocked()) {
        beginNetwork();
        return;
    }
    switch (identity->method()) {
    case P2pIdentityStore::Method::None:
    case P2pIdentityStore::Method::Passphrase: {
        // 作る・パスフレーズで読み込む（鍵の導出は GUI スレッドの外。ダイアログを閉じた後に状態を確かめ直す）
        P2pIdentityDialog dialog(identity->method() == P2pIdentityStore::Method::None ? P2pIdentityDialog::Mode::Create
                                                                                      : P2pIdentityDialog::Mode::Unlock,
                                 identity, dialogParent());
        if (dialog.exec() == QDialog::Accepted && m_lobby && identity->isUnlocked()) beginNetwork();
        return;
    }
    case P2pIdentityStore::Method::Keychain:
    case P2pIdentityStore::Method::Plain:
        // パスフレーズの要らない読み込みは非同期（終わったら onIdentityFinished で参加する）
        m_startAfterUnlock = true;
        identity->unlock();
        return;
    }
#endif
}

void P2pGameUiController::onIdentityFinished(P2pIdentityStore::Failure failure)
{
    if (!m_startAfterUnlock) return;
    m_startAfterUnlock = false;
    if (failure != P2pIdentityStore::Failure::None || !m_profile || !m_profile->identity()->isUnlocked()) {
        NotificationPresenter::warning(dialogParent(), tr("P2P対局"),
                                       tr("プレイヤー鍵を読み込めませんでした。「設定」でプレイヤー鍵を作り直せます。"));
        return;
    }
    beginNetwork();
}

void P2pGameUiController::beginNetwork()
{
    if (!m_lobby || !m_window) return;
    const QString message =
        P2pWindowText::startFailureText(m_lobby->begin(m_window->networkName(), m_window->bootstrap(), m_window->relayOnly()));
    if (!message.isEmpty()) NotificationPresenter::warning(dialogParent(), tr("P2P対局"), message);
    if (m_window) m_window->refresh();
}

void P2pGameUiController::onStopRequested()
{
    if (!m_lobby) return;
    if (m_deps.game && m_deps.game->isInProgress()
        && !DialogUtils::confirmAction(dialogParent(), tr("P2P対局"),
                                       tr("対局中です。切断すると対局は中断され、結果は確定しません。切断しますか？"),
                                       tr("切断"), QMessageBox::Warning)) {
        return;
    }
    m_lobby->end();
    if (m_window) m_window->refresh();
}

void P2pGameUiController::onRecreateIdentity()
{
    if (!m_lobby) return;
    if (m_lobby->status() != P2pLobbyService::Status::Off && m_lobby->status() != P2pLobbyService::Status::Failed) {
        NotificationPresenter::information(dialogParent(), tr("プレイヤー鍵"), tr("切断してから作り直してください。"));
        return;
    }
    if (!DialogUtils::confirmAction(dialogParent(), tr("プレイヤー鍵を作り直す"),
                                    tr("プレイヤー鍵を作り直すとプレイヤーIDが変わり、相手からは別人に見えます。古い鍵は消します。"
                                       "作り直しますか？"),
                                    tr("作り直す"), QMessageBox::Warning)) {
        return;
    }
    m_profile->identity()->remove();
    P2pIdentityDialog dialog(P2pIdentityDialog::Mode::Create, m_profile->identity(), dialogParent());
    dialog.exec();
    if (m_window) m_window->refresh();
}

// ---------------------------------------------------------------- 招待と対局中の操作

void P2pGameUiController::onCreateInviteRequested()
{
    if (m_lobby && m_window) m_lobby->createInvite(m_window->displayName(), m_window->terms(), m_window->sidePreference());
}

void P2pGameUiController::onInspectInviteRequested(const QString& invite)
{
    if (m_lobby) m_lobby->inspectInvite(invite);
}

void P2pGameUiController::onConnectInviteRequested(const QString& invite, bool allowPrivate)
{
    if (m_lobby && m_window) m_lobby->connectInvite(invite, allowPrivate, m_window->displayName());
}

void P2pGameUiController::onOfferDrawRequested()
{
    if (m_deps.game) m_deps.game->offerDraw();
}

void P2pGameUiController::onLeaveGameRequested()
{
    if (!m_deps.game) return;
    if (!DialogUtils::confirmAction(dialogParent(), tr("対局をやめる"),
                                    tr("対局をやめますか？ 結果は確定せず、相手には中断が伝わります。"),
                                    tr("対局をやめる"), QMessageBox::Warning)) {
        return;
    }
    // 確かめている間に対局が終わっていれば、司令塔が何もしない
    m_deps.game->leaveGame();
}

// ---------------------------------------------------------------- 交渉

void P2pGameUiController::onNegotiationChanged()
{
    const P2p::Negotiation* n = m_deps.game ? m_deps.game->negotiation() : nullptr;
    if (!n || n->stage() < P2p::Negotiation::Stage::Confirming || m_confirm) {
        if (m_confirm) m_confirm->refresh();
        return;
    }
    if (!m_lobby) return;
    m_confirm = new P2pConfirmDialog(m_deps.game, m_profile->knownPeers(), NetworkSettings::p2pHideUnknownNames(),
                                     dialogParent());
    m_confirm->setAttribute(Qt::WA_DeleteOnClose);
    m_confirm->show();
    m_confirm->raise();
    m_confirm->activateWindow();
}

void P2pGameUiController::onNegotiationEnded(bool started, std::optional<P2p::ErrorCode> code, bool byPeer)
{
    if (m_confirm) m_confirm->close();
    if (started || !code) return;
    // 自分で取りやめたときは知らせない
    if (*code == P2p::ErrorCode::Cancelled && !byPeer) return;
    QString text = byPeer ? tr("相手が対局を始めませんでした。") : tr("対局を始められませんでした。");
    if (*code == P2p::ErrorCode::Cancelled) text = tr("相手が取りやめました。");
    else text += P2pWindowText::errorText(*code);
    NotificationPresenter::information(dialogParent(), tr("P2P対局"), text, NotificationPresenter::Wait::No);
}

// ---------------------------------------------------------------- 対局

void P2pGameUiController::onGameStarted()
{
    // 後手のときは自分側から見る（通信対局と同じく、通常の対局と同じ経路で反転する）
    const P2p::Match* match = m_deps.game ? m_deps.game->match() : nullptr;
    if (match && m_deps.appearance && m_deps.shogiView && m_deps.shogiView->flipMode() != !match->iAmBlack()) {
        m_deps.appearance->flipBoard();
    }
    if (m_window) m_window->showGameTab();
}

void P2pGameUiController::onOfferReceived(P2p::Cause cause)
{
    // 待たずに尋ねる（司令塔の知らせの途中でイベントループを回さない。答えるまでの間も盤で指せる）。
    // 前の申し出の箱が残っていれば、答えとして扱わずに閉じる
    closeOfferBox();
    const QString question = cause == P2p::Cause::AgreedDraw ? tr("相手が引き分けを申し出ました。承諾しますか？")
                                                              : tr("相手が対局の中断を申し出ました。承諾しますか？");
    m_offerBox = DialogUtils::createConfirmBox(dialogParent(), tr("P2P対局"), question, tr("承諾"));
    m_offerBox->setObjectName(QStringLiteral("p2pOfferBox"));
    connect(m_offerBox, &QMessageBox::finished, this, &P2pGameUiController::onOfferAnswered);
    // 出してすぐは「承諾」を押せない。相手が引き分けと中断の申し出を続けて送ると箱が入れ替わり、前の申し出に向けた
    // クリックで、別の申し出を承諾しうる（ボタンの文字は同じ）
    if (QAbstractButton* accept = m_offerBox->button(QMessageBox::Yes)) accept->setEnabled(false);
    QTimer::singleShot(kOfferAcceptDelayMs, this, &P2pGameUiController::enableOfferAccept);
    m_offerBox->show();
}

void P2pGameUiController::enableOfferAccept()
{
    if (!m_offerBox) return;
    if (QAbstractButton* accept = m_offerBox->button(QMessageBox::Yes)) accept->setEnabled(true);
}

void P2pGameUiController::onOfferAnswered(int result)
{
    // 答えるまでに手数が進んだ・対局が終わったなら、対局の状態機械が無効として扱う
    if (m_deps.game) m_deps.game->answerOffer(result == QMessageBox::Yes);
}

void P2pGameUiController::closeStaleOffer()
{
    if (!m_offerBox) return;
    const P2p::Match* match = m_deps.game ? m_deps.game->match() : nullptr;
    if (!match || !match->hasPendingOffer()) closeOfferBox();
}

void P2pGameUiController::closeOfferBox()
{
    if (!m_offerBox) return;
    // 閉じると finished が出るので、先に外す（古い箱を閉じたことを、次の申し出への答えにしない）
    disconnect(m_offerBox, &QMessageBox::finished, this, &P2pGameUiController::onOfferAnswered);
    m_offerBox->close();
    m_offerBox.clear();
}

void P2pGameUiController::onProposalDeclined()
{
    NotificationPresenter::information(dialogParent(), tr("P2P対局"), tr("相手が申し出を断りました。対局を続けます。"),
                                       NotificationPresenter::Wait::No);
}

void P2pGameUiController::onGameFinished(P2p::Settlement ending, std::optional<P2p::ErrorCode> error,
                                         const QString& evidencePath)
{
    closeStaleOffer();
    // 終局の結果と、双方で確定できたかは1つの知らせにまとめる（結果の一致と確定は続けて起きる）。
    // 相手の確定を待つ間（届かなければ最長30秒）も、結果は棋譜の終局の行と P2P対局のウィンドウに出ている
    QStringList lines;
    const P2p::Match* match = m_deps.game ? m_deps.game->match() : nullptr;
    if (match && match->agreedResult()) {
        const P2p::FinalResult& result = *match->agreedResult();
        lines << tr("終局: %1").arg(P2pWindowText::outcomeText(result.outcome, result.cause));
    }
    lines << P2pWindowText::settlementText(ending, error);
    if (evidencePath.isEmpty()) lines << tr("対局の記録（証跡）を保存できませんでした。");
    NotificationPresenter::information(dialogParent(), tr("P2P対局"), lines.join(QLatin1Char('\n')),
                                       NotificationPresenter::Wait::No);
    if (m_window) m_window->refresh();
}

void P2pGameUiController::onPeerFailed(PeerProcess::Failure failure, const QString& detail)
{
    const QString text = P2pWindowText::peerFailureText(failure, detail);
    if (text.isEmpty()) return;
    // 通信プロセスから届いた知らせの処理の中なので、閉じるのを待たない（待つと入れ子のイベントループの中で通信プロセスの
    // 部品が消え、戻った先で消えたものを読む。監査 2026-11-13 P4）。続けて届いたら前の箱を閉じる
    if (m_failureBox) m_failureBox->close();
    m_failureBox = NotificationPresenter::warning(dialogParent(), tr("P2P対局"), text, NotificationPresenter::Wait::No);
    if (m_window) m_window->refresh();
}
