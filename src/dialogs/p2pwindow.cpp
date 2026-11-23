/// @file p2pwindow.cpp
/// @brief P2P対局のウィンドウの実装

#include "p2pwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "buttonstyles.h"
#include "dialogfontscale.h"
#include "dialogutils.h"
#include "layoutboundarywidget.h"
#include "networksettings.h"
#include "p2pbuildexpiry.h"
#include "p2pconnectionpanel.h"
#include "p2pfriendpresence.h"
#include "p2pgamecoordinator.h"
#include "p2pgamepanel.h"
#include "p2phistorypanel.h"
#include "p2pidentitystore.h"
#include "p2pinvitepanel.h"
#include "p2ppeerspanel.h"
#include "p2pplayerprofile.h"
#include "p2psettingspanel.h"
#include "p2pstatusstrip.h"
#include "p2pwindowtext.h"

P2pWindow::P2pWindow(P2pLobbyService* lobby, P2pGameCoordinator* game, QWidget* parent)
    : QDialog(parent, Qt::Window)
    , m_lobby(lobby)
    , m_game(game)
    , m_player(lobby && lobby->profile() ? lobby->profile()->playerNumber() : 1)
{
    setObjectName(QStringLiteral("p2pWindow"));
    setWindowTitle(m_player > 1 ? tr("P2P対局 — %1人目").arg(m_player) : tr("P2P対局"));
    setModal(false);
    auto* layout = new QVBoxLayout(this);
    if (m_player > 1) {
        // 同じ PC でほかのアプリが先に開いているとき。どのタブでも見えるよう、タブの上に出す
        QLabel* notice = DialogUtils::wrappedLabel(
            tr("このPCでは、ほかの ShogiBoardQ が先に P2P対局を開いているので、このアプリは「%1人目」として開きました。"
               "プレイヤー鍵（プレイヤーID）・表示名・対局記録・相手の一覧は、%1人目用に別に保存します。"
               "同じPCのアプリどうしで、招待で対局できます。")
                .arg(m_player),
            this);
        notice->setObjectName(QStringLiteral("p2pPlayerNotice"));
        layout->addWidget(notice);
    }
    // 接続の状態と「接続」「切断」は、どのタブでも見えるよう、タブの上に出す
    m_strip = new P2pStatusStrip(this);
    m_strip->button()->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_strip->button(), &QPushButton::clicked, this, &P2pWindow::onConnectClicked);
    layout->addWidget(m_strip);
    // タブは使う順に並べる: つなぎ方を決める → 相手を招待する・招待を受ける → 対局
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("p2pTabs"));
    m_connection = new P2pConnectionPanel(this);
    m_tabs->addTab(m_connection, tr("接続", "タブの名前"));
    m_invite = new P2pInvitePanel(lobby, game, m_player, this);
    connect(m_invite, &P2pInvitePanel::createInviteRequested, this, &P2pWindow::createInviteRequested);
    connect(m_invite, &P2pInvitePanel::inspectInviteRequested, this, &P2pWindow::inspectInviteRequested);
    connect(m_invite, &P2pInvitePanel::connectInviteRequested, this, &P2pWindow::connectInviteRequested);
    m_tabs->addTab(m_invite, tr("招待"));
    // 状態の文は1手ごとに替わる。文の入れ替えは配置のやり直しを親へ伝え、タブの部品（QTabWidget）は
    // それを受けるたびに全体を描き直す。ページの大きさの条件が変わったときだけ伝える入れ物で包む
    m_gamePanel = new P2pGamePanel(game, this);
    connect(m_gamePanel, &P2pGamePanel::resignRequested, this, &P2pWindow::resignRequested);
    connect(m_gamePanel, &P2pGamePanel::offerDrawRequested, this, &P2pWindow::offerDrawRequested);
    connect(m_gamePanel, &P2pGamePanel::offerAbortRequested, this, &P2pWindow::offerAbortRequested);
    connect(m_gamePanel, &P2pGamePanel::leaveGameRequested, this, &P2pWindow::leaveGameRequested);
    m_gameTab = new LayoutBoundaryWidget(m_gamePanel, this);
    m_tabs->addTab(m_gameTab, tr("対局中"));
    const P2pPlayerProfile* profile = lobby ? lobby->profile() : nullptr;
    m_history = new P2pHistoryPanel(profile ? profile->records() : nullptr, profile ? profile->knownPeers() : nullptr, this);
    m_tabs->addTab(m_history, tr("対局記録"));
    m_peers = new P2pPeersPanel(profile ? profile->knownPeers() : nullptr, profile ? profile->identity() : nullptr, this);
    connect(m_peers, &P2pPeersPanel::applyRequested, this, &P2pWindow::applyToFriendRequested);
    connect(m_peers, &P2pPeersPanel::sharingToggled, this, &P2pWindow::onSharingToggled);
    m_tabs->addTab(m_peers, tr("相手"));
    m_settings = new P2pSettingsPanel(this);
    connect(m_settings, &P2pSettingsPanel::copyPlayerIdRequested, this, &P2pWindow::onCopyPlayerId);
    connect(m_settings, &P2pSettingsPanel::recreateIdentityRequested, this, &P2pWindow::recreateIdentityRequested);
    connect(m_settings, &P2pSettingsPanel::connectionTabRequested, this, &P2pWindow::showConnectionTab);
    m_tabs->addTab(m_settings, tr("設定"));
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &P2pWindow::close);
    layout->addWidget(buttons);

    // 保存した値を入れ終えてから、欄の変更を保存につなぐ（途中でつなぐと、まだ読んでいない欄を既定値で上書きする）
    loadSettings();
    connect(m_connection, &P2pConnectionPanel::settingsChanged, this, &P2pWindow::onConnectionSettingsChanged);
    // 文が替わって要る高さが変わるのは、「接続」タブと「招待」タブ（ほかのタブは表が縮む・高さを固定している）
    connect(m_connection, &P2pConnectionPanel::contentResized, this, &P2pWindow::growToFit);
    connect(m_invite, &P2pInvitePanel::contentResized, this, &P2pWindow::growToFit);
    connect(m_settings, &P2pSettingsPanel::hideUnknownNamesToggled, this, &P2pWindow::saveSettings);
    connect(m_tabs, &QTabWidget::currentChanged, this, &P2pWindow::onTabChanged);
    connectSources();
    // 再接続の残り時間などは、知らせがなくても進む
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, m_gamePanel, &P2pGamePanel::refresh);
    timer->start(1000);

    DialogFontScale::Options options;
    options.size = &NetworkSettings::p2pWindowSize;
    DialogFontScale::install(this, NetworkSettings::p2pWindowFontSize, options);
    refresh();
    reloadRecords();
    // 戻した大きさに、いちばん高いタブの中身が収まらなければ広げる（2人目からは上に案内が入る）
    growToFit();
}

// ---------------------------------------------------------------- タブ

void P2pWindow::connectSources()
{
    // ロビーと司令塔は、表示のために読むだけ（操作は意図のシグナルで知らせる）
    if (m_lobby) {
        connect(m_lobby, &P2pLobbyService::statusChanged, this, &P2pWindow::refresh);
        connect(m_lobby, &P2pLobbyService::networkStatusChanged, this, &P2pWindow::refresh);
        connect(m_lobby, &P2pLobbyService::connectAttemptFailed, this, &P2pWindow::onConnectAttemptFailed);
    }
    if (m_game) {
        connect(m_game, &P2pGameCoordinator::statusChanged, m_gamePanel, &P2pGamePanel::refresh);
        connect(m_game, &P2pGameCoordinator::gameStarted, m_gamePanel, &P2pGamePanel::refresh);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::refresh);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::reloadRecords);
        connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pWindow::refresh);
        // 対局が始まると相手の対局数が増える
        connect(m_game, &P2pGameCoordinator::gameStarted, this, &P2pWindow::reloadRecords);
    }
}

// ---------------------------------------------------------------- 設定

void P2pWindow::loadSettings()
{
    m_invite->loadSettings();
    m_connection->setNetworkName(NetworkSettings::p2pNetworkName.valueOr(QString::fromLatin1(P2p::kLocalNetworkName)));
    m_connection->setBootstrap(NetworkSettings::p2pBootstrap());
    m_connection->setRelayOnly(NetworkSettings::p2pRelayOnly());
    m_settings->setHideUnknownNames(NetworkSettings::p2pHideUnknownNames());
    // 記録の一覧にも、開いたときから同じ設定を使わせる（保存のときだけ渡すと、接続するまで名前が出たままになる）
    if (m_history) m_history->setHideUnknownNames(hideUnknownNames());
    // 「友だちに在席を知らせる」は、友だちの一覧のある「相手」タブに置く
    if (m_peers) m_peers->setSharing(NetworkSettings::p2pSharePresence(m_player)());
}

void P2pWindow::onConnectionSettingsChanged()
{
    saveSettings();
    refresh();
}

void P2pWindow::showConnectionTab()
{
    m_tabs->setCurrentWidget(m_connection);
}

void P2pWindow::setFriendPresence(P2pFriendPresence* presence)
{
    m_presence = presence;
    if (m_peers) m_peers->setPresence(presence);
    if (m_presence) m_presence->setSharing(NetworkSettings::p2pSharePresence(m_player)());
}

void P2pWindow::onSharingToggled(bool sharing)
{
    NetworkSettings::p2pSharePresence(m_player).set(sharing);
    if (m_presence) m_presence->setSharing(sharing);
}

void P2pWindow::saveSettings()
{
    m_invite->saveSettings();
    NetworkSettings::p2pNetworkName.set(networkName());
    NetworkSettings::p2pBootstrap.set(bootstrap());
    NetworkSettings::p2pRelayOnly.set(relayOnly());
    NetworkSettings::p2pHideUnknownNames.set(hideUnknownNames());
    if (m_history) m_history->setHideUnknownNames(hideUnknownNames());
}

P2pGameCoordinator::Conditions P2pWindow::conditions() const { return m_invite->conditions(); }
QString P2pWindow::networkName() const { return m_connection->networkName(); }
QStringList P2pWindow::bootstrap() const { return m_connection->bootstrap(); }
bool P2pWindow::relayOnly() const { return m_connection->relayOnly(); }
bool P2pWindow::hideUnknownNames() const { return m_settings->hideUnknownNames(); }

bool P2pWindow::confirmDisplayName()
{
    if (m_invite->hasValidDisplayName()) return true;
    m_tabs->setCurrentWidget(m_invite);
    return m_invite->checkDisplayName();
}

// ---------------------------------------------------------------- 表示

void P2pWindow::showGameTab()
{
    m_tabs->setCurrentWidget(m_gameTab);
}

void P2pWindow::setVisible(bool visible)
{
    // 親（本体）のあるダイアログは、OS がいつも本体の前面に置き、盤を前面に出せない
    if (visible) DialogUtils::makeIndependentWindow(this);
    // 閉じている間にエンジンの登録が変わっていれば合わせる（交渉・対局の間は、選んだ対局者を変えない）
    if (visible && !isVisible() && m_invite && !(m_game && m_game->isBusy())) m_invite->reloadEngines();
    QDialog::setVisible(visible);
}

void P2pWindow::refresh()
{
    if (!m_lobby) return;
    const P2pLobbyService::Status status = m_lobby->status();
    const bool online = status == P2pLobbyService::Status::Online;
    const bool running = online || status == P2pLobbyService::Status::Starting;
    const bool busy = m_game && m_game->isBusy();
    QString statusText;
    P2pStatusStrip::State state = P2pStatusStrip::State::Offline;
    switch (status) {
    case P2pLobbyService::Status::Off: statusText = tr("未接続"); break;
    case P2pLobbyService::Status::Starting:
        statusText = tr("接続しています…");
        state = P2pStatusStrip::State::Starting;
        break;
    case P2pLobbyService::Status::Online:
        statusText = tr("接続中（つながっているピア: %1）").arg(m_lobby->connectedPeers());
        state = P2pStatusStrip::State::Online;
        break;
    case P2pLobbyService::Status::Failed:
        statusText = tr("P2P対局を使えません");
        state = P2pStatusStrip::State::Failed;
        break;
    }
    // 接続中は、招待がどの経路で届くか。接続していない間は、「接続」タブで選んだつなぎ方
    m_strip->setStatus(state, statusText,
                       online ? tr("招待の経路: %1").arg(routeText(routeState()))
                              : tr("今の設定: %1").arg(m_connection->settingSummary()));
    QPushButton* connectButton = m_strip->button();
    const QString connectText = running ? tr("切断") : tr("接続");
    if (connectButton->text() != connectText) {
        connectButton->setText(connectText);
        // 次に押す操作が「接続」のときだけ強調する
        connectButton->setStyleSheet(running ? ButtonStyles::dialogSecondaryAction() : ButtonStyles::primaryAction());
    }
    // ほかの動作（ローカル対局・CSA 通信対局・検討など）の最中は接続を始めない（動作状態を取り上げない）
    connectButton->setEnabled(!busy && (running || m_lobby->canBegin()));
    // つなぎ方は、接続している間は変えられない（通信プロセスを起動するときに渡す）
    m_connection->setLocked(running);
    m_connection->setNodeStatus(online ? std::optional<P2pConnectionPanel::NodeStatus>(P2pConnectionPanel::NodeStatus{
                                             m_lobby->relayReservations(), m_lobby->meetNodes()})
                                       : std::nullopt);
    m_invite->setInviteContentsText(m_connection->inviteContentsText());
    m_invite->refresh();

    P2pIdentityStore* identity = m_lobby->profile() ? m_lobby->profile()->identity() : nullptr;
    const QByteArray key = identity ? identity->publicKey() : QByteArray();
    m_settings->setPlayerId(key.isEmpty() ? tr("（まだ作っていません）") : P2pWindowText::playerIdText(key));

    QStringList identityLines;
    if (identity) {
        switch (identity->method()) {
        case P2pIdentityStore::Method::Keychain: identityLines << tr("プレイヤー鍵の保管: OSの鍵の保管庫"); break;
        case P2pIdentityStore::Method::Passphrase: identityLines << tr("プレイヤー鍵の保管: パスフレーズで暗号化したファイル"); break;
        case P2pIdentityStore::Method::Plain: identityLines << tr("プレイヤー鍵の保管: 暗号化しないファイル"); break;
        case P2pIdentityStore::Method::None: identityLines << tr("プレイヤー鍵: まだ作っていません"); break;
        }
    }
    const PeerIpc::Sandbox sandbox = m_lobby->sandbox();
    if (sandbox.applied) {
        identityLines << tr("通信プロセスの権限の制限: %1").arg(sandbox.mechanisms.join(QStringLiteral(", ")));
        identityLines << tr("loopbackとLAN内への接続は、OSの側では止められません（通信プロセス自身が断ります）。");
    }
    m_settings->setIdentityInfo(identityLines.join(QLatin1Char('\n')));
    m_settings->setBuildInfo(tr("このビルドの使用期限: %1（期限後は「インターネット」に参加しません。「同じPC・同じLAN」は期限後も使えます）")
                                 .arg(P2pWindowText::dateTimeText(static_cast<quint64>(P2pBuildExpiry::expiresAt()))));
    m_gamePanel->refresh();
}

P2pWindow::RouteState P2pWindow::routeState() const
{
    RouteState state;
    state.relayOnly = m_lobby->relayOnly();
    state.localProfile = m_lobby->isLocalProfile();
    state.publicReachability = m_lobby->reachability() == PeerIpc::Reachability::Public;
    state.hasNodes = m_lobby->hasNodes();
    state.connectedPeers = m_lobby->connectedPeers();
    state.relayReservations = m_lobby->relayReservations();
    return state;
}

QString P2pWindow::routeText(const RouteState& state)
{
    // 招待した相手が、どの経路でここへつなげるか（自分や相手のアドレスは出さない）
    const bool relay = state.relayReservations > 0;
    // 協力ノードを登録したのに中継の予約がまだない間は、登録を促さない。つないでいる・予約している途中か、登録した
    // 協力ノードに届かない・中継に対応していないかで、どれなのかは通信プロセスも知らせないので、待っても変わらない
    // ときに確かめることを出す（以前は、予約を待つ間も「登録してください」と出た）
    const auto unreached = [] {
        return tr("協力ノードにまだつながっていません。しばらく待っても変わらないときは、登録したアドレスと、"
                  "協力ノードが動いているかを確かめてください。");
    };
    if (state.relayOnly) {
        if (relay) return tr("協力ノードの中継だけを通ります（直接はつなぎません）。");
        if (!state.hasNodes) {
            return tr("中継を使えません。「協力ノードの中継だけを使う」には、中継のできる協力ノードを「接続」タブで登録してください。");
        }
        if (state.connectedPeers == 0) return unreached();
        return tr("協力ノードに中継を予約しています。しばらく待っても変わらないときは、協力ノードを --relay と、"
                  "この「相手のいる場所」に合う --network を付けて起動しているかを確かめてください。");
    }
    if (state.localProfile) {
        return relay ? tr("同じPC・同じLANの相手と直接つなぎます（協力ノードの中継も使えます）。")
                     : tr("同じPC・同じLANの相手と直接つなぎます。");
    }
    if (state.publicReachability) return relay ? tr("直接つなぎます（協力ノードの中継も使えます）。") : tr("直接つなぎます。");
    if (relay) return tr("協力ノードの中継を通ります（つながった後、直接の接続に切り替わることがあります）。");
    if (!state.hasNodes) return tr("外から届く接続先がまだありません。「接続」タブで協力ノードを登録してください。");
    if (state.connectedPeers == 0) return unreached();
    return tr("外から届く接続先を、協力ノードで確かめています。しばらく待っても変わらないときは、協力ノードを --relay と、"
              "この「相手のいる場所」に合う --network を付けて起動しているかを確かめてください。");
}

void P2pWindow::reloadRecords()
{
    if (m_history) m_history->reload();
    if (m_peers) m_peers->reload();
}

void P2pWindow::onTabChanged(int index)
{
    QWidget* page = m_tabs->widget(index);
    if (page == m_history && m_history) m_history->reload();
    if (page == m_peers && m_peers) m_peers->reload();
}

// ---------------------------------------------------------------- 操作

void P2pWindow::onConnectClicked()
{
    saveSettings();
    if (!m_lobby) return;
    const P2pLobbyService::Status status = m_lobby->status();
    if (status == P2pLobbyService::Status::Online || status == P2pLobbyService::Status::Starting) {
        emit stopRequested();
    } else {
        emit startRequested();
    }
}

void P2pWindow::growToFit()
{
    QLayout* box = layout();
    if (!box) return;
    // 子の変更（文・表示の切り替え）は、LayoutRequest のイベントで1段ずつ親のレイアウトへ伝わる。届く前に測ると
    // 変わる前の高さになるので、タブの中の入れ子の段数だけ先に届ける
    for (int level = 0; level < 8; ++level) QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    box->activate();
    int needed = box->totalHeightForWidth(width());
    if (const QScreen* display = screen()) needed = qMin(needed, display->availableGeometry().height());
    if (needed > height()) resize(width(), needed);
}

void P2pWindow::onConnectAttemptFailed(const P2pLobbyService::ConnectFailure& failure)
{
    using Stage = P2pLobbyService::ConnectFailure::Stage;
    const bool friendAttempt = failure.attempt == P2pLobbyService::ConnectAttempt::Friend;
    const QString text = P2pWindowText::connectFailureText(failure, m_lobby && m_lobby->isLocalProfile());
    // 招待の欄で確かめた・使った文字列が受け付けられなかったときは、欄の下に出す
    if (!friendAttempt && failure.stage != Stage::NotOpened) {
        m_invite->showConnectResult(text);
        return;
    }
    if (!friendAttempt) m_invite->clearConnectProgress();
    // 回線でつながらなかったとき・友だちへの申し込みの結果は、知らせの箱で出す。通信プロセスの知らせの処理の中なので、
    // 閉じるのを待たない（監査 2026-11-13 P4）。続けて届いたら前の箱を閉じる
    const bool notOpened = failure.stage == Stage::NotOpened;
    if (m_connectFailedBox) m_connectFailedBox->close();
    m_connectFailedBox = DialogUtils::createNoticeBox(this, notOpened ? QMessageBox::Warning : QMessageBox::Information,
                                                      notOpened ? tr("接続") : tr("対局の申し込み"), text);
    m_connectFailedBox->show();
}

void P2pWindow::onCopyPlayerId()
{
    if (m_lobby && m_lobby->profile()) {
        QApplication::clipboard()->setText(QString::fromLatin1(m_lobby->profile()->identity()->publicKey().toHex()));
    }
}
