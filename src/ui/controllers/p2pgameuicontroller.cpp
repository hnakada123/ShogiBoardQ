/// @file p2pgameuicontroller.cpp
/// @brief P2P対局の画面の実装

#include "p2pgameuicontroller.h"

#include <QAction>
#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <QWidget>

#include "apppaths.h"
#include "dialogutils.h"
#include "kifufilecontroller.h"
#include "mainwindowappearancecontroller.h"
#include "networksettings.h"
#include "notificationpresenter.h"
#include "p2pconfirmdialog.h"
#include "p2pdisplayname.h"
#include "p2pfriendpresence.h"
#include "p2pgamecoordinator.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2plobbyservice.h"
#include "p2poffercontroller.h"
#include "p2pplayerprofile.h"
#include "p2pplayerslot.h"
#include "p2precordstore.h"
#include "p2pwindow.h"
#include "p2pwindowtext.h"
#include "peerconfinement.h"
#include "shogiview.h"

namespace {
/// アプリの終了で、通信プロセスが中断の知らせを送って終わるのを待つ上限（通信プロセスは shutdown の 0.3 秒後に終わる）
constexpr int kExitWaitMs = 1500;
} // namespace

P2pGameUiController::P2pGameUiController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    if (m_deps.game) {
        connect(m_deps.game, &P2pGameCoordinator::negotiationChanged, this, &P2pGameUiController::onNegotiationChanged);
        connect(m_deps.game, &P2pGameCoordinator::negotiationEnded, this, &P2pGameUiController::onNegotiationEnded);
        connect(m_deps.game, &P2pGameCoordinator::gameStarted, this, &P2pGameUiController::onGameStarted);
        // 申し出の箱は、利用者が使っている側のウィンドウに出す
        P2pOfferController::Hooks hooks;
        hooks.noticeParent = [this]() { return noticeParent(); };
        m_offers = new P2pOfferController(m_deps.game, std::move(hooks), this);
        connect(m_deps.game, &P2pGameCoordinator::gameFinished, this, &P2pGameUiController::onGameFinished);
        connect(m_deps.game, &P2pGameCoordinator::engineNotice, this, &P2pGameUiController::onEngineNotice);
    }
}

P2pGameUiController::~P2pGameUiController() = default;

QWidget* P2pGameUiController::dialogParent() const
{
    // 閉じた（隠れた）ウィンドウは親にしない。OS の親のないダイアログになり、本体の後ろに隠れうる
    return m_window && m_window->isVisible() ? static_cast<QWidget*>(m_window) : m_deps.parentWidget;
}

QWidget* P2pGameUiController::noticeParent() const
{
    // P2P対局のウィンドウか、そこから開いたダイアログを使っている間だけ、P2P対局のウィンドウに出す。
    // 後ろに回したウィンドウを親にすると、OS が知らせと一緒にそのウィンドウも前面に出し、指している盤を隠す
    if (!m_window || !m_window->isVisible()) return m_deps.parentWidget;
    for (QWidget* window = QApplication::activeWindow(); window;) {
        if (window == m_window) return m_window;
        const QWidget* owner = window->parentWidget();
        window = owner ? owner->window() : nullptr;
    }
    // 本体を最小化しているときは、見えている側に出す
    const QWidget* main = m_deps.parentWidget;
    return main && main->isVisible() && !main->isMinimized() ? m_deps.parentWidget : static_cast<QWidget*>(m_window);
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
    // 友だちの在席（預ける・調べる・友だちの申し込みを受ける）。申し込みを受けるときの条件は、ウィンドウの
    // 「招待」タブの条件の今の値（招待を作るときと同じ）
    m_presence = new P2pFriendPresence(m_lobby, m_deps.game, this);
    m_presence->setConditionsProvider([this]() -> std::optional<P2pGameCoordinator::Conditions> {
        if (!m_window) return std::nullopt;
        const P2pGameCoordinator::Conditions mine = m_window->conditions();
        return mine.isValid() ? std::optional<P2pGameCoordinator::Conditions>(mine) : std::nullopt;
    });
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
        connect(m_window, &P2pWindow::applyToFriendRequested, this, &P2pGameUiController::onApplyToFriendRequested);
        m_window->setFriendPresence(m_presence);
        // 投了と合意中断は、メニュー・ツールバーと同じ操作にする
        if (m_deps.resignAction) connect(m_window, &P2pWindow::resignRequested, m_deps.resignAction, &QAction::trigger);
        if (m_deps.breakOffAction) {
            connect(m_window, &P2pWindow::offerAbortRequested, m_deps.breakOffAction, &QAction::trigger);
        }
        connect(m_window, &P2pWindow::offerDrawRequested, this, &P2pGameUiController::onOfferDrawRequested);
        connect(m_window, &P2pWindow::leaveGameRequested, this, &P2pGameUiController::onLeaveGameRequested);
    }
    m_window->refresh();
    DialogUtils::showWindow(m_window);
    m_window->raise();
    m_window->activateWindow();
}

void P2pGameUiController::onStartRequested()
{
    if (!m_lobby || !m_window) return;
    // 通信プロセスの権限を制限できる OS だけで使う。プレイヤー鍵を作る前に知らせる
    if (!PeerConfinement::isSupported()) {
        onPeerFailed(PeerProcess::Failure::UnsupportedOs, QString());
        return;
    }
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
    if (!m_lobby || !m_window) return;
    m_lobby->createInvite(m_window->conditions());
}

void P2pGameUiController::onInspectInviteRequested(const QString& invite)
{
    if (m_lobby) m_lobby->inspectInvite(invite);
}

void P2pGameUiController::onConnectInviteRequested(const QString& invite, P2pLobbyService::ConnectAttempt attempt,
                                                   bool allowPrivate)
{
    if (!m_lobby || !m_window) return;
    m_lobby->connectInvite(invite, attempt, allowPrivate, m_window->conditions());
}

void P2pGameUiController::onApplyToFriendRequested(const QByteArray& playerKey)
{
    if (!m_presence || !m_window || !m_window->confirmDisplayName()) return;
    QString text;
    switch (m_presence->apply(playerKey, m_window->conditions())) {
    case P2pFriendPresence::ApplyResult::Started:
        // つながると、双方に条件確認画面が開く。つながらない・断られたときの文は、ウィンドウが出す
        // （P2pLobbyService::connectAttemptFailed）
        return;
    case P2pFriendPresence::ApplyResult::NotOnline:
        text = tr("この友だちは、今は在席を知らせていません。");
        break;
    case P2pFriendPresence::ApplyResult::Busy:
        text = tr("交渉中・対局中は、対局を申し込めません。");
        break;
    case P2pFriendPresence::ApplyResult::Unavailable:
        text = tr("接続していないか、合言葉に対応した協力ノードにつながっていません。");
        break;
    }
    NotificationPresenter::information(noticeParent(), tr("対局の申し込み"), text, NotificationPresenter::Wait::No);
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
                                     noticeParent());
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
    // こちらで版が合わないと判断するのは、エンジンで指す対局を、対局者の種別を扱えない版の相手と始めようとしたとき
    else if (*code == P2p::ErrorCode::UnsupportedVersion && !byPeer) {
        text += tr("相手のアプリは、エンジンが指す対局に対応していない版です。相手のアプリを新しい版にするか、"
                   "「対局者」を「人間」にしてください。");
    } else text += P2pWindowText::errorText(*code);
    NotificationPresenter::information(noticeParent(), tr("P2P対局"), text, NotificationPresenter::Wait::No);
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

void P2pGameUiController::onGameFinished(P2p::Settlement ending, std::optional<P2p::ErrorCode> error,
                                         const QString& evidencePath)
{
    if (m_offers) m_offers->closeStale();
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
    NotificationPresenter::information(noticeParent(), tr("P2P対局"), lines.join(QLatin1Char('\n')),
                                       NotificationPresenter::Wait::No);
    if (m_window) m_window->refresh();
}

void P2pGameUiController::onEngineNotice(const QString& message)
{
    // 司令塔の知らせの途中なので、閉じるのを待たない。交渉の取りやめ・投了・中断は司令塔が済ませている
    if (m_engineBox) m_engineBox->close();
    m_engineBox = NotificationPresenter::warning(noticeParent(), tr("P2P対局"), message, NotificationPresenter::Wait::No);
}

void P2pGameUiController::onPeerFailed(PeerProcess::Failure failure, const QString& detail)
{
    const QString text = P2pWindowText::peerFailureText(failure, detail);
    if (text.isEmpty()) return;
    // 通信プロセスから届いた知らせの処理の中なので、閉じるのを待たない（待つと入れ子のイベントループの中で通信プロセスの
    // 部品が消え、戻った先で消えたものを読む。監査 2026-11-13 P4）。続けて届いたら前の箱を閉じる
    if (m_failureBox) m_failureBox->close();
    m_failureBox = NotificationPresenter::warning(noticeParent(), tr("P2P対局"), text, NotificationPresenter::Wait::No);
    if (m_window) m_window->refresh();
}
