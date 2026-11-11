/// @file p2pwindow.cpp
/// @brief P2P対局のウィンドウの実装

#include "p2pwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "dialogfontscale.h"
#include "dialogutils.h"
#include "networksettings.h"
#include "p2pdisplayname.h"
#include "p2pgamecoordinator.h"
#include "p2phistorypanel.h"
#include "p2pidentitydialog.h"
#include "p2pidentitystore.h"
#include "p2ppeerspanel.h"
#include "p2pwindowtext.h"
#include "peeripc.h"

namespace {

QLabel* plainLabel(QWidget* parent, const QString& text = QString())
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

QString dateText(quint64 seconds)
{
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds)).toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
}

} // namespace

P2pWindow::P2pWindow(P2pLobbyService* lobby, P2pGameCoordinator* game, QWidget* parent)
    : QDialog(parent)
    , m_lobby(lobby)
    , m_game(game)
{
    setWindowTitle(tr("P2P対局（実験版）"));
    setModal(false);
    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("p2pTabs"));
    m_tabs->addTab(createConnectionTab(), tr("接続と条件"));
    m_tabs->addTab(createInviteTab(), tr("招待で接続"));
    m_gameTab = createGameTab();
    m_tabs->addTab(m_gameTab, tr("対局中"));
    m_history = new P2pHistoryPanel(lobby ? lobby->records() : nullptr, lobby ? lobby->knownPeers() : nullptr, this);
    m_tabs->addTab(m_history, tr("対局記録"));
    m_peers = new P2pPeersPanel(lobby ? lobby->knownPeers() : nullptr, lobby ? lobby->identity() : nullptr, this);
    m_tabs->addTab(m_peers, tr("相手の管理"));
    m_tabs->addTab(createSettingsTab(), tr("設定"));
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &P2pWindow::close);
    layout->addWidget(buttons);

    loadSettings();
    connect(m_tabs, &QTabWidget::currentChanged, this, &P2pWindow::onTabChanged);
    if (m_lobby) {
        connect(m_lobby, &P2pLobbyService::statusChanged, this, &P2pWindow::refresh);
        connect(m_lobby, &P2pLobbyService::networkStatusChanged, this, &P2pWindow::refresh);
        connect(m_lobby, &P2pLobbyService::inviteCreated, this, &P2pWindow::onInviteCreated);
        connect(m_lobby, &P2pLobbyService::inviteInspected, this, &P2pWindow::onInviteInspected);
        connect(m_lobby, &P2pLobbyService::requestFailed, this, &P2pWindow::onRequestFailed);
        connect(m_lobby, &P2pLobbyService::connectFailed, this, &P2pWindow::onConnectFailed);
    }
    if (m_game) {
        connect(m_game, &P2pGameCoordinator::statusChanged, this, &P2pWindow::updateGameStatus);
        connect(m_game, &P2pGameCoordinator::gameStarted, this, &P2pWindow::updateGameStatus);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::refresh);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::reloadRecords);
        connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pWindow::refresh);
        // 対局が始まると相手の対局数が増える
        connect(m_game, &P2pGameCoordinator::gameStarted, this, &P2pWindow::reloadRecords);
    }
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &P2pWindow::updateGameStatus);
    timer->start(1000);

    DialogFontScale::Options options;
    options.size = &NetworkSettings::p2pWindowSize;
    DialogFontScale::install(this, NetworkSettings::p2pWindowFontSize, options);
    refresh();
    reloadRecords();
}

// ---------------------------------------------------------------- タブ

QWidget* P2pWindow::createConnectionTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* networkBox = new QGroupBox(tr("接続"), tab);
    auto* networkForm = new QFormLayout(networkBox);
    m_network = new QComboBox(networkBox);
    m_network->setObjectName(QStringLiteral("p2pNetwork"));
    m_network->addItem(tr("ローカル検証用（同じPC・同じLAN）"), QStringLiteral("sbq-local-1"));
    m_network->addItem(tr("限定試験用"), QStringLiteral("sbq-limited-1"));
    auto* connectRow = new QHBoxLayout;
    m_connect = new QPushButton(networkBox);
    m_connect->setObjectName(QStringLiteral("p2pConnect"));
    connect(m_connect, &QPushButton::clicked, this, &P2pWindow::onConnectClicked);
    connectRow->addWidget(m_network, 1);
    connectRow->addWidget(m_connect);
    networkForm->addRow(tr("ネットワーク"), connectRow);
    m_status = plainLabel(networkBox);
    networkForm->addRow(tr("状態"), m_status);
    auto* idRow = new QHBoxLayout;
    m_playerId = plainLabel(networkBox);
    m_playerId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* copyId = new QPushButton(tr("コピー"), networkBox);
    connect(copyId, &QPushButton::clicked, this, &P2pWindow::onCopyPlayerId);
    idRow->addWidget(m_playerId, 1);
    idRow->addWidget(copyId);
    networkForm->addRow(tr("あなたのプレイヤーID"), idRow);
    // 自分や相手のアドレス（IP）は画面に出さない。招待がどの経路で届くかだけを文で示す
    m_routeInfo = plainLabel(networkBox);
    m_routeInfo->setObjectName(QStringLiteral("p2pRouteInfo"));
    networkForm->addRow(tr("招待の経路"), m_routeInfo);
    layout->addWidget(networkBox);

    auto* conditionBox = new QGroupBox(tr("名乗る名前と条件"), tab);
    auto* conditionForm = new QFormLayout(conditionBox);
    m_displayName = new QLineEdit(conditionBox);
    m_displayName->setObjectName(QStringLiteral("p2pDisplayName"));
    m_displayName->setPlaceholderText(tr("32文字まで（英数字・かな・漢字など）"));
    conditionForm->addRow(tr("表示名"), m_displayName);
    auto* timeRow = new QHBoxLayout;
    m_mainMinutes = new QSpinBox(conditionBox);
    m_mainMinutes->setRange(0, 1440);
    m_mainMinutes->setSuffix(tr(" 分"));
    m_byoyomi = new QSpinBox(conditionBox);
    m_byoyomi->setRange(0, 600);
    m_byoyomi->setSuffix(tr(" 秒"));
    timeRow->addWidget(new QLabel(tr("持ち時間"), conditionBox));
    timeRow->addWidget(m_mainMinutes);
    timeRow->addWidget(new QLabel(tr("秒読み"), conditionBox));
    timeRow->addWidget(m_byoyomi);
    timeRow->addStretch();
    conditionForm->addRow(tr("時間"), timeRow);
    m_side = new QComboBox(conditionBox);
    m_side->addItem(tr("指定なし（双方の乱数で決める）"), 0);
    m_side->addItem(tr("先手希望"), 1);
    m_side->addItem(tr("後手希望"), 2);
    conditionForm->addRow(tr("先後"), m_side);
    layout->addWidget(conditionBox);
    layout->addWidget(plainLabel(tab, tr("対局の相手は招待で決めます。「招待で接続」タブで招待文字列を作って相手に渡すか、"
                                         "受け取った招待文字列で接続してください。")));
    layout->addStretch();
    return tab;
}

QWidget* P2pWindow::createInviteTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* createBox = new QGroupBox(tr("相手を招待する"), tab);
    auto* createLayout = new QVBoxLayout(createBox);
    createLayout->addWidget(plainLabel(createBox, tr("招待文字列には、あなたへの接続先が入ります（「直接接続を使わない」設定でなければ、"
                                                     "あなたのIPアドレスも入ります）。相手だけに届く経路で渡し、公開の場所に貼らないでください。"
                                                     "招待文字列は10分で使えなくなり、1回だけ使えます。"
                                                     "持ち時間と先後は「接続と条件」の条件を使います。")));
    m_createInvite = new QPushButton(tr("招待文字列を作る"), createBox);
    connect(m_createInvite, &QPushButton::clicked, this, &P2pWindow::onCreateInvite);
    createLayout->addWidget(m_createInvite);
    m_inviteOut = new QPlainTextEdit(createBox);
    m_inviteOut->setObjectName(QStringLiteral("p2pInviteOut"));
    m_inviteOut->setReadOnly(true);
    m_inviteOut->setMaximumHeight(90);
    createLayout->addWidget(m_inviteOut);
    auto* outRow = new QHBoxLayout;
    m_inviteOutInfo = plainLabel(createBox);
    m_copyInvite = new QPushButton(tr("コピー"), createBox);
    connect(m_copyInvite, &QPushButton::clicked, this, &P2pWindow::onCopyInvite);
    outRow->addWidget(m_inviteOutInfo, 1);
    outRow->addWidget(m_copyInvite);
    createLayout->addLayout(outRow);
    layout->addWidget(createBox);

    auto* joinBox = new QGroupBox(tr("招待で接続する"), tab);
    auto* joinLayout = new QVBoxLayout(joinBox);
    joinLayout->addWidget(plainLabel(joinBox, tr("受け取った招待文字列を貼り付けてください。接続しただけでは対局は始まりません。"
                                                 "条件確認画面で相手のプレイヤーIDを確かめてから開始します。")));
    m_inviteIn = new QPlainTextEdit(joinBox);
    m_inviteIn->setObjectName(QStringLiteral("p2pInviteIn"));
    m_inviteIn->setMaximumHeight(90);
    joinLayout->addWidget(m_inviteIn);
    m_inspectInvite = new QPushButton(tr("招待文字列を確かめる"), joinBox);
    connect(m_inspectInvite, &QPushButton::clicked, this, &P2pWindow::onInspectInvite);
    joinLayout->addWidget(m_inspectInvite);
    m_inviteInInfo = plainLabel(joinBox);
    m_inviteInInfo->setObjectName(QStringLiteral("p2pInviteInfo"));
    m_inviteInInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    joinLayout->addWidget(m_inviteInInfo);
    m_allowPrivate = new QCheckBox(tr("上の私設アドレス（同じLANの相手）へ接続してよい"), joinBox);
    m_allowPrivate->setVisible(false);
    joinLayout->addWidget(m_allowPrivate);
    m_connectInvite = new QPushButton(tr("接続する"), joinBox);
    m_connectInvite->setEnabled(false);
    connect(m_connectInvite, &QPushButton::clicked, this, &P2pWindow::onConnectInvite);
    joinLayout->addWidget(m_connectInvite);
    layout->addWidget(joinBox);
    layout->addStretch();
    return tab;
}

QWidget* P2pWindow::createGameTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    m_gameStatus = plainLabel(tab);
    m_gameStatus->setObjectName(QStringLiteral("p2pGameStatus"));
    m_gameStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_gameStatus);
    auto* buttons = new QHBoxLayout;
    m_resign = new QPushButton(tr("投了"), tab);
    m_offerDraw = new QPushButton(tr("引き分けを申し出る"), tab);
    m_offerAbort = new QPushButton(tr("中断を申し出る"), tab);
    m_leave = new QPushButton(tr("対局をやめる（結果未確定）"), tab);
    connect(m_resign, &QPushButton::clicked, this, &P2pWindow::onResign);
    connect(m_offerDraw, &QPushButton::clicked, this, &P2pWindow::onOfferDraw);
    connect(m_offerAbort, &QPushButton::clicked, this, &P2pWindow::onOfferAbort);
    connect(m_leave, &QPushButton::clicked, this, &P2pWindow::onLeaveGame);
    for (QPushButton* b : {m_resign, m_offerDraw, m_offerAbort, m_leave}) buttons->addWidget(b);
    layout->addLayout(buttons);
    layout->addWidget(plainLabel(tab, tr("合意による引き分け・中断は、相手が承諾したときだけ成立します。"
                                         "「対局をやめる」は一方的な離脱で、結果は確定しません。")));
    layout->addStretch();
    return tab;
}

QWidget* P2pWindow::createSettingsTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* form = new QFormLayout;
    m_bootstrap = new QPlainTextEdit(tab);
    m_bootstrap->setObjectName(QStringLiteral("p2pBootstrap"));
    m_bootstrap->setPlaceholderText(tr("協力ノードのアドレス（/ip4/…/p2p/… の形）を1行に1つ"));
    m_bootstrap->setMaximumHeight(100);
    form->addRow(tr("接続候補"), m_bootstrap);
    m_relayOnly = new QCheckBox(tr("直接接続を使わない（接続候補の協力ノードの中継だけを使い、相手からIPアドレスを隠す。"
                                   "協力ノードの運営者には見えます）"),
                                tab);
    form->addRow(QString(), m_relayOnly);
    m_hideUnknown = new QCheckBox(tr("対局したことのない相手の表示名を出さない"), tab);
    form->addRow(QString(), m_hideUnknown);
    layout->addLayout(form);
    for (QCheckBox* box : {m_relayOnly, m_hideUnknown}) connect(box, &QCheckBox::toggled, this, &P2pWindow::saveSettings);
    connect(m_bootstrap, &QPlainTextEdit::textChanged, this, &P2pWindow::saveSettings);

    m_identityInfo = plainLabel(tab);
    m_identityInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_identityInfo);
    auto* recreate = new QPushButton(tr("プレイヤー鍵を作り直す…"), tab);
    connect(recreate, &QPushButton::clicked, this, &P2pWindow::recreateIdentityRequested);
    layout->addWidget(recreate, 0, Qt::AlignLeft);
    m_buildInfo = plainLabel(tab);
    layout->addWidget(m_buildInfo);
    layout->addWidget(plainLabel(tab, P2pIdentityDialog::knownLimitsText()));
    layout->addStretch();
    return tab;
}

// ---------------------------------------------------------------- 設定

void P2pWindow::loadSettings()
{
    // 読み込みの途中で欄の変更の知らせから保存しない（まだ読んでいない欄を既定値で上書きしてしまう）
    const QSignalBlocker blockBootstrap(m_bootstrap);
    const QSignalBlocker blockRelay(m_relayOnly);
    const QSignalBlocker blockHide(m_hideUnknown);
    m_displayName->setText(NetworkSettings::p2pDisplayName());
    m_mainMinutes->setValue(NetworkSettings::p2pMainTimeSeconds() / 60);
    m_byoyomi->setValue(NetworkSettings::p2pByoyomiSeconds());
    m_side->setCurrentIndex(m_side->findData(NetworkSettings::p2pSidePreference()));
    const int network = m_network->findData(NetworkSettings::p2pNetworkName());
    m_network->setCurrentIndex(network < 0 ? 0 : network);
    m_bootstrap->setPlainText(NetworkSettings::p2pBootstrap().join(QLatin1Char('\n')));
    m_relayOnly->setChecked(NetworkSettings::p2pRelayOnly());
    m_hideUnknown->setChecked(NetworkSettings::p2pHideUnknownNames());
}

void P2pWindow::saveSettings()
{
    if (P2p::DisplayName::isValid(displayName())) NetworkSettings::p2pDisplayName.set(displayName());
    NetworkSettings::p2pMainTimeSeconds.set(m_mainMinutes->value() * 60);
    NetworkSettings::p2pByoyomiSeconds.set(m_byoyomi->value());
    NetworkSettings::p2pSidePreference.set(m_side->currentData().toInt());
    NetworkSettings::p2pNetworkName.set(networkName());
    NetworkSettings::p2pBootstrap.set(bootstrap());
    NetworkSettings::p2pRelayOnly.set(relayOnly());
    NetworkSettings::p2pHideUnknownNames.set(hideUnknownNames());
    if (m_history) m_history->setHideUnknownNames(hideUnknownNames());
}

QString P2pWindow::displayName() const { return m_displayName->text(); }

P2p::TimeControlTerms P2pWindow::terms() const
{
    return P2p::TimeControlTerms{static_cast<quint64>(m_mainMinutes->value()) * 60'000,
                                 static_cast<quint64>(m_byoyomi->value()) * 1000};
}

P2p::SidePreference P2pWindow::sidePreference() const
{
    return static_cast<P2p::SidePreference>(m_side->currentData().toInt());
}

QString P2pWindow::networkName() const { return m_network->currentData().toString(); }

QStringList P2pWindow::bootstrap() const
{
    QStringList lines;
    const QStringList allLines = m_bootstrap->toPlainText().split(QLatin1Char('\n'));
    for (const QString& line : allLines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) lines << trimmed;
    }
    return lines.mid(0, 16);
}

bool P2pWindow::relayOnly() const { return m_relayOnly->isChecked(); }
bool P2pWindow::hideUnknownNames() const { return m_hideUnknown->isChecked(); }

bool P2pWindow::checkDisplayName()
{
    if (P2p::DisplayName::isValid(displayName())) return true;
    DialogUtils::warning(this, tr("表示名"),
                         tr("表示名は1〜32文字で、前後に空白を置かず、英数字・記号・かな・漢字・全角英数だけを使ってください。"));
    m_tabs->setCurrentIndex(0);
    m_displayName->setFocus();
    return false;
}

// ---------------------------------------------------------------- 表示

void P2pWindow::showGameTab()
{
    m_tabs->setCurrentWidget(m_gameTab);
}

void P2pWindow::refresh()
{
    if (!m_lobby) return;
    const P2pLobbyService::Status status = m_lobby->status();
    const bool online = status == P2pLobbyService::Status::Online;
    const bool running = online || status == P2pLobbyService::Status::Starting;
    const bool busy = m_game && m_game->isBusy();
    QString statusText;
    switch (status) {
    case P2pLobbyService::Status::Off: statusText = tr("未接続"); break;
    case P2pLobbyService::Status::Starting: statusText = tr("接続しています…"); break;
    case P2pLobbyService::Status::Online:
        statusText = tr("接続中（つながっているピア: %1）").arg(m_lobby->connectedPeers());
        break;
    case P2pLobbyService::Status::Failed: statusText = tr("P2P対局を使えません"); break;
    }
    m_status->setText(statusText);
    m_connect->setText(running ? tr("切断") : tr("接続"));
    m_network->setEnabled(!running);
    m_connect->setEnabled(!busy);

    P2pIdentityStore* identity = m_lobby->identity();
    const QByteArray key = identity ? identity->publicKey() : QByteArray();
    m_playerId->setText(key.isEmpty() ? tr("（まだ作っていません）") : P2pWindowText::playerIdText(key));
    m_routeInfo->setText(online ? routeText() : QString());

    m_createInvite->setEnabled(online && !busy);
    m_inspectInvite->setEnabled(online && !busy);
    if (!online || busy) m_connectInvite->setEnabled(false);

    QStringList identityLines;
    if (identity) {
        switch (identity->method()) {
        case P2pIdentityStore::Method::Keychain: identityLines << tr("プレイヤー鍵の保管: OSの鍵の保管庫"); break;
        case P2pIdentityStore::Method::Passphrase: identityLines << tr("プレイヤー鍵の保管: パスフレーズで暗号化したファイル"); break;
        case P2pIdentityStore::Method::Plain: identityLines << tr("プレイヤー鍵の保管: 暗号化しないファイル"); break;
        case P2pIdentityStore::Method::None: identityLines << tr("プレイヤー鍵: まだ作っていません"); break;
        }
    }
    const QJsonObject sandbox = m_lobby->sandbox();
    if (!sandbox.isEmpty()) {
        QStringList mechanisms;
        const QJsonArray array1 = sandbox.value(QStringLiteral("mechanisms")).toArray();
        for (const auto& m : array1) mechanisms << m.toString();
        identityLines << tr("通信プロセスの権限の制限: %1").arg(mechanisms.join(QStringLiteral(", ")));
        identityLines << tr("loopbackとLAN内への接続は、OSの側では止められません（通信プロセス自身が断ります）。");
    }
    m_identityInfo->setText(identityLines.join(QLatin1Char('\n')));
    m_buildInfo->setText(tr("このビルドの使用期限: %1（期限後は限定試験用のネットワークに参加しません）")
                             .arg(dateText(static_cast<quint64>(P2pLobbyService::buildExpiresAt()))));
    updateGameStatus();
}

QString P2pWindow::routeText() const
{
    // 招待した相手が、どの経路でここへつなげるか（自分や相手のアドレスは出さない）
    const bool relay = m_lobby->relayReservations() > 0;
    if (m_lobby->relayOnly()) {
        return relay ? tr("協力ノードの中継だけを通ります（直接接続を使わない設定）。")
                     : tr("中継を使えません。「直接接続を使わない」には、中継のできる協力ノードを「設定」の「接続候補」に入れてください。");
    }
    if (m_lobby->isLocalProfile()) {
        return relay ? tr("同じPC・同じLANの相手と直接つなぎます（協力ノードの中継も使えます）。")
                     : tr("同じPC・同じLANの相手と直接つなぎます。");
    }
    const bool direct = m_lobby->reachability() == QLatin1String("public");
    if (direct) return relay ? tr("直接つなぎます（協力ノードの中継も使えます）。") : tr("直接つなぎます。");
    if (relay) return tr("協力ノードの中継を通ります（つながった後、直接の接続に切り替わることがあります）。");
    return tr("外から届く接続先がまだありません。協力ノードを「設定」の「接続候補」に入れてください。");
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

void P2pWindow::updateGameStatus()
{
    const P2p::Match* match = m_game ? m_game->match() : nullptr;
    const bool playing = m_game && m_game->phase() == P2pGameCoordinator::Phase::Playing && match;
    m_resign->setEnabled(playing && m_game->canResign());
    for (QPushButton* b : {m_offerDraw, m_offerAbort, m_leave}) b->setEnabled(playing && m_game->isInProgress());
    if (!match) {
        m_gameStatus->setText(tr("対局していません。"));
        return;
    }
    QStringList lines;
    lines << tr("相手: 「%1」").arg(m_game->opponentDisplayName());
    lines << (match->iAmBlack() ? tr("あなたは先手") : tr("あなたは後手"));
    lines << tr("経路: %1").arg(m_game->viaRelay() ? tr("中継") : tr("直接"));
    switch (match->phase()) {
    case P2p::Match::Phase::Opening: lines << tr("開始の手続き中…"); break;
    case P2p::Match::Phase::Playing:
        if (match->hasPendingMove()) lines << tr("着手の確認を待っています…");
        else lines << (match->isMyTurn() ? tr("あなたの手番です") : tr("相手の手番です"));
        break;
    case P2p::Match::Phase::Reconnecting:
        lines << tr("再接続中（残り %1 秒）").arg(match->reconnectRemainingMs() / 1000);
        break;
    case P2p::Match::Phase::Finishing: lines << tr("結果を確定しています…"); break;
    case P2p::Match::Phase::Finished: lines << P2pWindowText::endingText(match->ending(), match->endingError()); break;
    }
    const qint64 disconnectLeft = static_cast<qint64>(P2p::NetworkPolicy::v1().disconnectTotalMs) - match->reconnectTotalMs();
    lines << tr("切断時間の残り: %1 秒").arg(qMax<qint64>(0, disconnectLeft) / 1000);
    if (match->agreedResult()) {
        lines << tr("結果: %1").arg(P2pWindowText::outcomeText(match->agreedResult()->outcome, match->agreedResult()->cause));
    }
    m_gameStatus->setText(lines.join(QLatin1Char('\n')));
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

void P2pWindow::onCreateInvite()
{
    if (!m_lobby || !checkDisplayName() || !terms().isValid()) return;
    saveSettings();
    m_lobby->createInvite(displayName(), terms(), sidePreference());
}

void P2pWindow::onInviteCreated(const QString& invite, quint64 expiresAt, int addressCount)
{
    m_inviteOut->setPlainText(invite);
    // 接続先の入っていない招待文字列では、相手はつなげない
    m_inviteOutInfo->setText(addressCount > 0 ? tr("期限: %1（到達候補 %2 件）").arg(dateText(expiresAt)).arg(addressCount)
                                              : tr("この招待文字列には接続先が入っていないので、相手は接続できません。"
                                                   "「接続と条件」の「招待の経路」を確かめてください。"));
    growToFit();
}

void P2pWindow::growToFit()
{
    QLayout* box = layout();
    if (!box) return;
    box->activate();
    int needed = box->totalHeightForWidth(width());
    if (const QScreen* display = screen()) needed = qMin(needed, display->availableGeometry().height());
    if (needed > height()) resize(width(), needed);
}

void P2pWindow::onCopyInvite()
{
    if (!m_inviteOut->toPlainText().isEmpty()) QApplication::clipboard()->setText(m_inviteOut->toPlainText());
}

void P2pWindow::onInspectInvite()
{
    if (!m_lobby) return;
    m_connectInvite->setEnabled(false);
    m_allowPrivate->setVisible(false);
    m_inviteInInfo->setText(tr("確かめています…"));
    m_lobby->inspectInvite(m_inviteIn->toPlainText().trimmed());
}

void P2pWindow::onInviteInspected(const P2pLobbyService::InviteInfo& info)
{
    QStringList lines;
    lines << tr("招待した相手のPeerId: %1…").arg(info.peerId.left(16));
    lines << tr("期限: %1").arg(dateText(info.expiresAt));
    lines << tr("到達候補: %1 件（使える候補 %2 件）").arg(info.addressCount).arg(info.usableAddressCount);
    if (!info.privateAddresses.isEmpty()) {
        lines << tr("私設アドレス（同じLANの相手）:");
        lines << info.privateAddresses;
    }
    m_inviteInInfo->setText(lines.join(QLatin1Char('\n')));
    m_allowPrivate->setChecked(false);
    m_allowPrivate->setVisible(!info.privateAddresses.isEmpty());
    m_connectInvite->setEnabled(info.usableAddressCount > 0 || !info.privateAddresses.isEmpty());
    growToFit();
}

void P2pWindow::onConnectInvite()
{
    if (!m_lobby || !checkDisplayName()) return;
    saveSettings();
    m_connectInvite->setEnabled(false);
    m_lobby->connectInvite(m_inviteIn->toPlainText().trimmed(), m_allowPrivate->isChecked(), displayName());
    // 招待文字列はもう使わない（ログ・設定に残さない）
    m_inviteIn->clear();
    m_inviteInInfo->setText(tr("接続しています…"));
}

void P2pWindow::onRequestFailed(const QString& method, const QString& code)
{
    if (method == QLatin1String("inspectInvite") || method == QLatin1String("connectInvite")) {
        QString text;
        if (code == QLatin1String("invite_expired")) text = tr("招待文字列の期限が過ぎています。");
        else if (code == QLatin1String("invite_wrong_network")) text = tr("別のネットワークの招待文字列です。");
        else if (code == QLatin1String("invite_own")) text = tr("自分が作った招待文字列です。");
        else text = tr("招待文字列を読めません。");
        m_inviteInInfo->setText(text);
    }
}

void P2pWindow::onConnectFailed(const QString& reason)
{
    QString text;
    if (reason == QLatin1String("no_relay_route")) {
        text = tr("この招待文字列には、あなたの協力ノードの中継を通る接続先がありません。「直接接続を使わない」設定では、"
                  "相手と同じ協力ノードを「設定」の「接続候補」に入れる必要があります。");
    } else {
        text = reason == QLatin1String("timeout") ? tr("相手に接続できませんでした（時間切れ）。") : tr("相手に接続できませんでした。");
        if (m_lobby && m_lobby->isLocalProfile()) {
            // 同じ LAN の別の PC: 招待した側の PC が、外から届く接続を止めていることが多い
            text += QLatin1Char('\n')
                    + tr("同じLANの別のPCとつなぐときは、招待した側のPCのファイアウォールで、TCP と UDP の %1 番ポートへの接続が"
                         "許可されているか確かめてください。招待する側を入れ替えると、つながることがあります。")
                          .arg(PeerIpc::kLanPort);
        }
    }
    m_inviteInInfo->clear();
    DialogUtils::warning(this, tr("接続"), text);
}

void P2pWindow::onCopyPlayerId()
{
    if (m_lobby && m_lobby->identity()) {
        QApplication::clipboard()->setText(QString::fromLatin1(m_lobby->identity()->publicKey().toHex()));
    }
}

void P2pWindow::onResign()
{
    if (m_game && DialogUtils::confirmAction(this, tr("投了"), tr("投了しますか？"), tr("投了"))) m_game->resign();
}

void P2pWindow::onOfferDraw()
{
    if (m_game) m_game->offerDraw();
}

void P2pWindow::onOfferAbort()
{
    if (m_game) m_game->offerAbort();
}

void P2pWindow::onLeaveGame()
{
    if (m_game && DialogUtils::confirmAction(this, tr("対局をやめる"),
                                             tr("対局をやめますか？ 結果は確定せず、相手には中断が伝わります。"),
                                             tr("対局をやめる"), QMessageBox::Warning)) {
        m_game->leaveGame();
    }
}
