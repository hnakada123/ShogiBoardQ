/// @file p2pwindow.cpp
/// @brief P2P対局のウィンドウの実装

#include "p2pwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "dialogfontscale.h"
#include "dialogutils.h"
#include "engineselector.h"
#include "layoutboundarywidget.h"
#include "networksettings.h"
#include "p2pbuildexpiry.h"
#include "p2pconnectionpanel.h"
#include "p2pdisplayname.h"
#include "p2pfriendpresence.h"
#include "p2pgamecoordinator.h"
#include "p2pgamepanel.h"
#include "p2phistorypanel.h"
#include "p2pidentitystore.h"
#include "p2ppeerspanel.h"
#include "p2pplayerprofile.h"
#include "p2pprotocolvalues.h"
#include "p2psettingspanel.h"
#include "p2pstatusstrip.h"
#include "p2pwindowtext.h"

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
    : QDialog(parent, Qt::Window)
    , m_lobby(lobby)
    , m_game(game)
    , m_player(lobby && lobby->profile() ? lobby->profile()->playerNumber() : 1)
    , m_engineNameKey(NetworkSettings::p2pEngineNameKey(m_player))
{
    setObjectName(QStringLiteral("p2pWindow"));
    setWindowTitle(m_player > 1 ? tr("P2P対局 — %1人目").arg(m_player) : tr("P2P対局"));
    setModal(false);
    auto* layout = new QVBoxLayout(this);
    if (m_player > 1) {
        // 同じ PC でほかのアプリが先に開いているとき。どのタブでも見えるよう、タブの上に出す
        QLabel* notice = plainLabel(
            this, tr("このPCでは、ほかの ShogiBoardQ が先に P2P対局を開いているので、このアプリは「%1人目」として開きました。"
                     "プレイヤー鍵（プレイヤーID）・表示名・対局記録・相手の一覧は、%1人目用に別に保存します。"
                     "同じPCのアプリどうしで、招待で対局できます。")
                      .arg(m_player));
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
    m_inviteTab = createInviteTab();
    m_tabs->addTab(m_inviteTab, tr("招待"));
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
    connect(m_connection, &P2pConnectionPanel::contentResized, this, &P2pWindow::growToFit);
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
        connect(m_lobby, &P2pLobbyService::inviteCreated, this, &P2pWindow::onInviteCreated);
        connect(m_lobby, &P2pLobbyService::inviteInspected, this, &P2pWindow::onInviteInspected);
        connect(m_lobby, &P2pLobbyService::inviteRejected, this, &P2pWindow::onInviteRejected);
        connect(m_lobby, &P2pLobbyService::connectFailed, this, &P2pWindow::onConnectFailed);
    }
    if (m_game) {
        connect(m_game, &P2pGameCoordinator::statusChanged, m_gamePanel, &P2pGamePanel::refresh);
        connect(m_game, &P2pGameCoordinator::gameStarted, m_gamePanel, &P2pGamePanel::refresh);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::refresh);
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pWindow::reloadRecords);
        connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pWindow::refresh);
        connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pWindow::clearConnectProgress);
        connect(m_game, &P2pGameCoordinator::inviteRefused, this, &P2pWindow::onInviteRefused);
        // 対局が始まると相手の対局数が増える
        connect(m_game, &P2pGameCoordinator::gameStarted, this, &P2pWindow::reloadRecords);
    }
}

QWidget* P2pWindow::createInviteTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(createConditionBox(tab));
    // 招待する側と招待を受ける側は、どちらか一方を使うので、左右に並べる
    auto* columns = new QHBoxLayout;
    // 招待する側は出すものが多い（合言葉・招待文字列・注意）ので、幅を広く取る
    columns->addWidget(createInviteOutBox(tab), 3);
    columns->addWidget(createInviteInBox(tab), 2);
    layout->addLayout(columns);
    layout->addStretch();
    return tab;
}

QWidget* P2pWindow::createConditionBox(QWidget* parent)
{
    // 名乗る名前と条件は、招待を作るときも、招待を受けるときも使う
    auto* box = new QGroupBox(tr("あなたと対局の条件"), parent);
    auto* layout = new QVBoxLayout(box);
    const auto captioned = [box](QHBoxLayout* row, const QString& caption, QWidget* field, int stretch = 0) {
        auto* label = new QLabel(caption, box);
        label->setBuddy(field);
        row->addWidget(label);
        row->addWidget(field, stretch);
    };

    auto* nameRow = new QHBoxLayout;
    m_displayName = new QLineEdit(box);
    m_displayName->setObjectName(QStringLiteral("p2pDisplayName"));
    m_displayName->setPlaceholderText(tr("32文字まで（英数字・かな・漢字など）"));
    captioned(nameRow, tr("表示名"), m_displayName, 2);
    // 自分の手を人間が指すか、登録エンジンが指すか（コンボの先頭が「人間」。対局開始ダイアログの対局者の欄と同じ）
    nameRow->addSpacing(8);
    m_playerKind = new QComboBox(box);
    m_playerKind->setObjectName(QStringLiteral("p2pPlayerKind"));
    captioned(nameRow, tr("対局者"), m_playerKind, 3);
    m_engineSettings = new QPushButton(tr("エンジン設定"), box);
    m_engineSettings->setObjectName(QStringLiteral("p2pEngineSettings"));
    m_engineSettings->setStyleSheet(ButtonStyles::secondaryNeutral());
    m_engineSettings->setAutoDefault(false);
    nameRow->addWidget(m_engineSettings);
    layout->addLayout(nameRow);

    auto* timeRow = new QHBoxLayout;
    m_mainMinutes = new QSpinBox(box);
    m_mainMinutes->setObjectName(QStringLiteral("p2pMainMinutes"));
    m_mainMinutes->setRange(0, 1440);
    m_mainMinutes->setSuffix(tr(" 分"));
    captioned(timeRow, tr("持ち時間"), m_mainMinutes);
    timeRow->addSpacing(8);
    m_byoyomi = new QSpinBox(box);
    m_byoyomi->setObjectName(QStringLiteral("p2pByoyomi"));
    m_byoyomi->setRange(0, 600);
    m_byoyomi->setSuffix(tr(" 秒"));
    captioned(timeRow, tr("秒読み"), m_byoyomi);
    timeRow->addSpacing(8);
    m_side = new QComboBox(box);
    m_side->setObjectName(QStringLiteral("p2pSide"));
    m_side->addItem(tr("指定なし（双方の乱数で決める）"), 0);
    m_side->addItem(tr("先手希望"), 1);
    m_side->addItem(tr("後手希望"), 2);
    captioned(timeRow, tr("先後"), m_side, 1);
    layout->addLayout(timeRow);

    layout->addWidget(plainLabel(box, tr("エンジンを選ぶと、あなたの手はエンジンが指し、そのことが相手に伝わり、対局記録（証跡）にも残ります。")));
    EngineSelector::Options engineOptions;
    engineOptions.keys = {m_engineNameKey.constData(), nullptr, nullptr};
    engineOptions.builtinLabel = tr("人間");
    m_engineSelector = new EngineSelector({m_playerKind, m_engineSettings, nullptr}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested, this, &P2pWindow::onEngineSettingsRequested);
    return box;
}

QWidget* P2pWindow::createInviteOutBox(QWidget* parent)
{
    auto* box = new QGroupBox(tr("招待する"), parent);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(plainLabel(box, tr("上の条件で招待を作り、相手に伝えます。")));
    m_createInvite = new QPushButton(tr("招待を作る"), box);
    m_createInvite->setObjectName(QStringLiteral("p2pCreateInvite"));
    m_createInvite->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_createInvite, &QPushButton::clicked, this, &P2pWindow::onCreateInvite);
    layout->addWidget(m_createInvite);

    // 合言葉（口頭でも伝えられる短い形）。合言葉の付いた招待のときだけ見せる
    m_codeRow = new QWidget(box);
    m_codeRow->setObjectName(QStringLiteral("p2pInviteCodeRow"));
    auto* codeLayout = new QGridLayout(m_codeRow);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setVerticalSpacing(0);
    codeLayout->setColumnStretch(0, 1);
    codeLayout->addWidget(new QLabel(tr("合言葉（口頭でも伝えられます）"), m_codeRow), 0, 0, 1, 2);
    m_codeOut = new QLabel(m_codeRow);
    m_codeOut->setObjectName(QStringLiteral("p2pInviteCode"));
    m_codeOut->setTextFormat(Qt::PlainText);
    m_codeOut->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // 読み上げたり見ながら打ったりするので、大きく、字の幅のそろった書体で出す
    QFont codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    codeFont.setPointSizeF(font().pointSizeF() * 1.8);
    codeFont.setBold(true);
    m_codeOut->setFont(codeFont);
    m_copyCode = new QPushButton(tr("合言葉をコピー"), m_codeRow);
    m_copyCode->setObjectName(QStringLiteral("p2pCopyCode"));
    connect(m_copyCode, &QPushButton::clicked, this, &P2pWindow::onCopyCode);
    codeLayout->addWidget(m_codeOut, 1, 0);
    codeLayout->addWidget(m_copyCode, 1, 1);
    m_codeRow->setVisible(false);
    layout->addWidget(m_codeRow);
    // 合言葉の付かない招待（協力ノードが合言葉に対応していない、など）は、理由と、招待文字列を渡すことを知らせる
    m_noCode = plainLabel(box);
    m_noCode->setObjectName(QStringLiteral("p2pInviteNoCode"));
    m_noCode->setVisible(false);
    layout->addWidget(m_noCode);

    layout->addWidget(plainLabel(box, tr("招待文字列（メッセージなどで送る長い形）")));
    m_inviteOut = new QPlainTextEdit(box);
    m_inviteOut->setObjectName(QStringLiteral("p2pInviteOut"));
    m_inviteOut->setReadOnly(true);
    m_inviteOut->setMaximumHeight(56);
    layout->addWidget(m_inviteOut);
    auto* outRow = new QHBoxLayout;
    m_inviteOutInfo = plainLabel(box);
    m_copyInvite = new QPushButton(tr("招待文字列をコピー"), box);
    m_copyInvite->setObjectName(QStringLiteral("p2pCopyInvite"));
    connect(m_copyInvite, &QPushButton::clicked, this, &P2pWindow::onCopyInvite);
    outRow->addWidget(m_inviteOutInfo, 1);
    outRow->addWidget(m_copyInvite, 0, Qt::AlignTop);
    layout->addLayout(outRow);
    m_inviteNotes = plainLabel(box);
    m_inviteNotes->setObjectName(QStringLiteral("p2pInviteNotes"));
    layout->addWidget(m_inviteNotes);
    layout->addStretch();
    return box;
}

QWidget* P2pWindow::createInviteInBox(QWidget* parent)
{
    // 上から順に進める: 入力する → 確かめる → 結果を見る → 接続する
    auto* box = new QGroupBox(tr("招待を受ける"), parent);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(plainLabel(box, tr("受け取った合言葉を入力するか、招待文字列を貼り付けてください。")));
    m_inviteIn = new QPlainTextEdit(box);
    m_inviteIn->setObjectName(QStringLiteral("p2pInviteIn"));
    m_inviteIn->setMaximumHeight(56);
    layout->addWidget(m_inviteIn);
    m_inspectInvite = new QPushButton(tr("確かめる"), box);
    m_inspectInvite->setObjectName(QStringLiteral("p2pInspectInvite"));
    connect(m_inspectInvite, &QPushButton::clicked, this, &P2pWindow::onInspectInvite);
    layout->addWidget(m_inspectInvite);
    m_inviteInInfo = plainLabel(box);
    m_inviteInInfo->setObjectName(QStringLiteral("p2pInviteInfo"));
    m_inviteInInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_inviteInInfo);
    m_allowPrivate = new QCheckBox(tr("上の私設アドレス（同じLANの相手）へ接続してよい"), box);
    m_allowPrivate->setObjectName(QStringLiteral("p2pAllowPrivate"));
    m_allowPrivate->setVisible(false);
    layout->addWidget(m_allowPrivate);
    m_connectInvite = new QPushButton(tr("接続する"), box);
    m_connectInvite->setObjectName(QStringLiteral("p2pConnectInvite"));
    m_connectInvite->setStyleSheet(ButtonStyles::primaryAction());
    m_connectInvite->setEnabled(false);
    connect(m_connectInvite, &QPushButton::clicked, this, &P2pWindow::onConnectInvite);
    layout->addWidget(m_connectInvite);
    layout->addWidget(plainLabel(box, tr("接続しただけでは対局は始まりません。条件確認画面で相手のプレイヤーIDを確かめてから開始します。")));
    layout->addStretch();
    return box;
}

// ---------------------------------------------------------------- 設定

void P2pWindow::loadSettings()
{
    m_displayName->setText(NetworkSettings::p2pDisplayName(m_player)());
    m_mainMinutes->setValue(NetworkSettings::p2pMainTimeSeconds() / 60);
    m_byoyomi->setValue(NetworkSettings::p2pByoyomiSeconds());
    m_side->setCurrentIndex(m_side->findData(NetworkSettings::p2pSidePreference()));
    // 既定は人間。保存したエンジンが削除されていれば人間に戻す（別のエンジンに替えない）
    const std::optional<QString> savedEngine =
        EngineSelectionSettings::savedSelectionName({m_engineNameKey.constData(), nullptr, nullptr});
    if (NetworkSettings::p2pPlayAsEngine(m_player)() && savedEngine
        && m_engineSelector->engines().indexOfName(*savedEngine) >= 0) {
        m_engineSelector->select(*savedEngine);
    } else {
        m_engineSelector->selectBuiltin();
    }
    m_connection->setNetworkName(NetworkSettings::p2pNetworkName());
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
    if (P2p::DisplayName::isValid(displayName())) NetworkSettings::p2pDisplayName(m_player).set(displayName());
    NetworkSettings::p2pMainTimeSeconds.set(m_mainMinutes->value() * 60);
    NetworkSettings::p2pByoyomiSeconds.set(m_byoyomi->value());
    NetworkSettings::p2pSidePreference.set(m_side->currentData().toInt());
    // 人間を選んでいる間は、前に選んだエンジンを残す
    const bool engine = playerSetup().isEngine();
    NetworkSettings::p2pPlayAsEngine(m_player).set(engine);
    if (engine) m_engineSelector->saveSelection();
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

P2pGameCoordinator::PlayerSetup P2pWindow::playerSetup() const
{
    P2pGameCoordinator::PlayerSetup setup;
    if (const std::optional<EngineEntry> engine = m_engineSelector->selected()) {
        setup.kind = P2p::PlayerKind::Engine;
        setup.enginePath = engine->path;
        setup.engineName = engine->name;
    }
    return setup;
}

void P2pWindow::reloadEngines()
{
    const std::optional<EngineEntry> before = m_engineSelector->selected();
    m_engineSelector->reload();
    const std::optional<EngineEntry> after = m_engineSelector->selected();
    // 読み直しは、選んでいたエンジンが無くなると先頭の登録エンジンを選ぶ。相手に伝える対局者を黙って替えない
    if (!before || !after || before->name != after->name) m_engineSelector->selectBuiltin();
}

void P2pWindow::onEngineSettingsRequested(const EngineEntry& engine)
{
    ChangeEngineSettingsDialog::showFor(engine, this);
}

QString P2pWindow::networkName() const { return m_connection->networkName(); }
QStringList P2pWindow::bootstrap() const { return m_connection->bootstrap(); }
bool P2pWindow::relayOnly() const { return m_connection->relayOnly(); }
bool P2pWindow::hideUnknownNames() const { return m_settings->hideUnknownNames(); }

bool P2pWindow::checkDisplayName()
{
    if (P2p::DisplayName::isValid(displayName())) return true;
    QString text;
    if (!displayName().isEmpty()) {
        text = tr("表示名は1〜32文字で、前後に空白を置かず、英数字・記号・かな・漢字・全角英数だけを使ってください。");
    } else if (m_player > 1) {
        // 表示名はプレイヤーごとに覚えるので、2人目からは1人目の名前が入っていない
        text = tr("「招待」タブの「表示名」に、相手に見える名前を入れてください。"
                  "表示名は%1人目用に別に保存するので、1人目の表示名は入っていません。")
                   .arg(m_player);
    } else {
        text = tr("「招待」タブの「表示名」に、相手に見える名前を入れてください。");
    }
    DialogUtils::warning(this, tr("表示名"), text);
    m_tabs->setCurrentWidget(m_inviteTab);
    m_displayName->setFocus();
    return false;
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
    if (visible && !isVisible() && m_engineSelector && !(m_game && m_game->isBusy())) reloadEngines();
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
                       online ? tr("招待の経路: %1").arg(routeText()) : tr("今の設定: %1").arg(m_connection->settingSummary()));
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
    updateInviteNotes();

    P2pIdentityStore* identity = m_lobby->profile() ? m_lobby->profile()->identity() : nullptr;
    const QByteArray key = identity ? identity->publicKey() : QByteArray();
    m_settings->setPlayerId(key.isEmpty() ? tr("（まだ作っていません）") : P2pWindowText::playerIdText(key));

    // 交渉・対局の間は、相手に伝えた対局者を変えられない
    m_engineSelector->setEnabled(!busy);
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
    const PeerIpc::Sandbox sandbox = m_lobby->sandbox();
    if (sandbox.applied) {
        identityLines << tr("通信プロセスの権限の制限: %1").arg(sandbox.mechanisms.join(QStringLiteral(", ")));
        identityLines << tr("loopbackとLAN内への接続は、OSの側では止められません（通信プロセス自身が断ります）。");
    }
    m_settings->setIdentityInfo(identityLines.join(QLatin1Char('\n')));
    m_settings->setBuildInfo(tr("このビルドの使用期限: %1（期限後は「インターネット」に参加しません。「同じPC・同じLAN」は期限後も使えます）")
                                 .arg(dateText(static_cast<quint64>(P2pBuildExpiry::expiresAt()))));
    m_gamePanel->refresh();
}

void P2pWindow::updateInviteNotes()
{
    const QStringList notes{tr("・相手だけに伝え、公開の場所には貼らないでください。"),
                            tr("・10分で使えなくなります。合言葉と招待文字列のどちらかで、1回だけ使えます。"),
                            tr("・%1").arg(m_connection->inviteContentsText())};
    const QString text = notes.join(QLatin1Char('\n'));
    if (m_inviteNotes->text() == text) return;
    m_inviteNotes->setMinimumHeight(0);
    m_inviteNotes->setText(text);
}

QString P2pWindow::routeText() const
{
    // 招待した相手が、どの経路でここへつなげるか（自分や相手のアドレスは出さない）
    const bool relay = m_lobby->relayReservations() > 0;
    if (m_lobby->relayOnly()) {
        return relay ? tr("協力ノードの中継だけを通ります（直接はつなぎません）。")
                     : tr("中継を使えません。「協力ノードの中継だけを使う」には、中継のできる協力ノードを「接続」タブで登録してください。");
    }
    if (m_lobby->isLocalProfile()) {
        return relay ? tr("同じPC・同じLANの相手と直接つなぎます（協力ノードの中継も使えます）。")
                     : tr("同じPC・同じLANの相手と直接つなぎます。");
    }
    const bool direct = m_lobby->reachability() == PeerIpc::Reachability::Public;
    if (direct) return relay ? tr("直接つなぎます（協力ノードの中継も使えます）。") : tr("直接つなぎます。");
    if (relay) return tr("協力ノードの中継を通ります（つながった後、直接の接続に切り替わることがあります）。");
    return tr("外から届く接続先がまだありません。「接続」タブで協力ノードを登録してください。");
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

void P2pWindow::onCreateInvite()
{
    if (!m_lobby || !checkDisplayName() || !terms().isValid()) return;
    saveSettings();
    // 新しい招待ができるまで（合言葉の鍵を作る間）、前の招待を新しい招待と取り違えないように消しておく
    m_inviteOut->clear();
    m_codeOut->clear();
    m_noCode->setVisible(false);
    m_inviteOutInfo->clear();
    emit createInviteRequested();
}

void P2pWindow::onInviteCreated(const QString& invite, const QString& code, quint64 expiresAt, int addressCount)
{
    m_inviteOut->setPlainText(invite);
    m_codeOut->setText(code);
    m_codeRow->setVisible(!code.isEmpty());
    // 合言葉の付かない招待: 行を隠すだけだと、なぜ合言葉が出ないのかが分からない
    const bool noCode = code.isEmpty() && !invite.isEmpty();
    if (noCode) {
        // 協力ノードを使う招待（「インターネット」・中継だけ）は、合言葉に対応した協力ノードがあるときだけ合言葉が付く
        const bool viaNode = m_lobby && (!m_lobby->isLocalProfile() || m_lobby->relayOnly());
        m_noCode->setMinimumHeight(0);
        m_noCode->setText(viaNode && m_lobby->meetNodes() == 0
                              ? tr("この招待には合言葉が付きません（登録した協力ノードが、合言葉に対応していません）。"
                                   "下の招待文字列を、メッセージなどで相手に渡してください。")
                              : tr("この招待には合言葉が付きません。下の招待文字列を、メッセージなどで相手に渡してください。"));
    }
    m_noCode->setVisible(noCode);
    m_inviteExpiresAt = expiresAt;
    // 接続先の入っていない招待文字列では、相手はつなげない
    m_inviteOutInfo->setText(addressCount > 0 ? tr("期限: %1（到達候補 %2 件）").arg(dateText(expiresAt)).arg(addressCount)
                                              : tr("この招待文字列には接続先が入っていないので、相手は接続できません。"
                                                   "ウィンドウの上の「招待の経路」を確かめてください。"));
    growToFit();
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

void P2pWindow::onCopyInvite()
{
    copyInviteText(m_inviteOut->toPlainText());
}

void P2pWindow::onCopyCode()
{
    copyInviteText(m_codeOut->text());
}

void P2pWindow::copyInviteText(const QString& text)
{
    if (text.isEmpty()) return;
    // 招待文字列と合言葉には、接続先（IP）と、招待を使うための秘密が入る。クリップボードの履歴（KDE の Klipper など）に
    // 残さないよう秘密の印を付け、期限が過ぎたら、クリップボードがまだこの文字列のままなら消す
    auto* mime = new QMimeData;
    mime->setText(text);
    mime->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
    QApplication::clipboard()->setMimeData(mime);
    m_copiedInvite = text;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 left = qBound(qint64(0), static_cast<qint64>(m_inviteExpiresAt) - now, qint64(15 * 60));
    QTimer::singleShot(static_cast<int>(left * 1000), this, &P2pWindow::clearCopiedInvite);
}

void P2pWindow::clearCopiedInvite()
{
    if (m_copiedInvite.isEmpty()) return;
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard->text() == m_copiedInvite) clipboard->clear();
    m_copiedInvite.clear();
}

void P2pWindow::onInspectInvite()
{
    if (!m_lobby) return;
    m_connectInvite->setEnabled(false);
    m_allowPrivate->setVisible(false);
    m_inviteInInfo->setText(tr("確かめています…"));
    const QString input = m_inviteIn->toPlainText();
    m_inviteInIsCode = !P2pWindowText::isInviteString(input);
    m_inviteInHasAddress = false;
    emit inspectInviteRequested(P2pWindowText::inviteInput(input));
}

void P2pWindow::onInviteInspected(const P2pLobbyService::InviteInfo& info)
{
    QStringList lines;
    m_inviteInHasAddress = info.kind == PeerIpc::InviteKind::LanCode;
    if (info.kind == PeerIpc::InviteKind::LanCode) {
        // 同じ LAN の合言葉には、相手の PeerId と期限が入っていない（期限は招待した側が確かめる）
        lines << tr("合言葉を読みました（同じLANの相手の招待）。");
    } else {
        // 協力ノードを使う合言葉は、協力ノードから引き取った招待の中身が分かる
        if (info.kind == PeerIpc::InviteKind::NodeCode) lines << tr("合言葉の招待を、協力ノードから受け取りました。");
        lines << tr("招待した相手のPeerId: %1…").arg(info.peerId.left(16));
        lines << tr("期限: %1").arg(dateText(info.expiresAt));
    }
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
    const QString input = m_inviteIn->toPlainText();
    m_inviteInIsCode = !P2pWindowText::isInviteString(input);
    m_friendApplying = false;
    emit connectInviteRequested(P2pWindowText::inviteInput(input), m_allowPrivate->isChecked());
    // 招待文字列・合言葉はもう使わない（ログ・設定に残さない）
    m_inviteIn->clear();
    m_inviteInInfo->setText(tr("接続しています…"));
    m_connectingByInvite = true;
}

void P2pWindow::onInviteRejected(PeerIpc::InviteRejection rejection)
{
    QString text;
    if (m_inviteInIsCode) {
        switch (rejection) {
        case PeerIpc::InviteRejection::WrongNetwork:
            // 同じ LAN の合言葉（相手の LAN の中の接続先が入っている）を、「インターネット」で入力した
            text = tr("この合言葉は、同じLANの相手の招待のものです。「同じPC・同じLAN」で接続しているときに使えます。");
            break;
        case PeerIpc::InviteRejection::Own: text = tr("自分が作った合言葉です。"); break;
        case PeerIpc::InviteRejection::Expired:
        case PeerIpc::InviteRejection::NotFound:
            // 協力ノードを使う合言葉: 協力ノードは、期限が過ぎた・1回引き取られた招待を消すので、理由を見分けられない
            text = tr("この合言葉の招待が見つかりません。合言葉が違うか、期限（10分）が過ぎたか、もう使われています。"
                      "相手に新しい招待を作ってもらってください。");
            break;
        case PeerIpc::InviteRejection::NoNode:
            text = tr("この合言葉の招待は、協力ノードから受け取ります。招待した人と同じ協力ノード（合言葉に対応したもの）を"
                      "「接続」タブで登録して、接続し直してください。");
            break;
        case PeerIpc::InviteRejection::Unreadable:
            text = tr("合言葉を読めません。入力した文字を確かめてください。");
            break;
        }
    } else {
        switch (rejection) {
        case PeerIpc::InviteRejection::Expired: text = tr("招待文字列の期限が過ぎています。"); break;
        case PeerIpc::InviteRejection::WrongNetwork: text = tr("別のネットワークの招待文字列です。"); break;
        case PeerIpc::InviteRejection::Own: text = tr("自分が作った招待文字列です。"); break;
        // 協力ノードへの問い合わせは合言葉だけなので、招待文字列では起きない
        case PeerIpc::InviteRejection::NotFound:
        case PeerIpc::InviteRejection::NoNode:
        case PeerIpc::InviteRejection::Unreadable: text = tr("招待文字列を読めません。"); break;
        }
    }
    m_inviteInInfo->setText(text);
    growToFit();
}

void P2pWindow::clearConnectProgress()
{
    if (!m_connectingByInvite) return;
    m_connectingByInvite = false;
    m_inviteInInfo->clear();
}

void P2pWindow::onInviteRefused()
{
    clearConnectProgress();
    if (m_friendApplying) {
        // 友だちは、対局中・交渉中の間と、在席の知らせを取り消した後は、申し込みを何も返さずに閉じる
        m_friendApplying = false;
        if (m_connectFailedBox) m_connectFailedBox->close();
        m_connectFailedBox = DialogUtils::createNoticeBox(
            this, QMessageBox::Information, tr("対局の申し込み"),
            tr("相手は、今は申し込みを受けられません。対局中か、在席の知らせが古くなっています。"
               "少し待ってから、もう一度お試しください。"));
        m_connectFailedBox->show();
        return;
    }
    // 招待した側は、招待のトークンが合わない接続を何も返さずに閉じる。合言葉は、形が正しければ中身の違いを
    // こちらでは見分けられない（期限も入っていない）ので、考えられる理由を並べる
    const QString text = m_inviteInIsCode
        ? tr("相手につながりましたが、合言葉が受け付けられませんでした。合言葉が違うか、期限（10分）が過ぎたか、"
             "もう使われています。相手に新しい招待を作ってもらってください。")
        : tr("相手につながりましたが、招待文字列が受け付けられませんでした。もう使われたか、取り消されています。"
             "相手に新しい招待を作ってもらってください。");
    m_inviteInInfo->setText(text);
    growToFit();
}

void P2pWindow::onConnectFailed(PeerIpc::CloseReason reason)
{
    m_connectingByInvite = false;
    QString text;
    if (m_friendApplying) {
        m_friendApplying = false;
        text = reason == PeerIpc::CloseReason::NoRelayRoute
            ? tr("相手の在席の知らせに、あなたの協力ノードの中継を通る接続先がありません。「協力ノードの中継だけを使う」設定では、"
                 "相手と同じ協力ノードを「接続」タブで登録する必要があります。")
            : tr("相手に接続できませんでした。相手が接続を終えたか、回線の状態が変わっています。");
    } else if (reason == PeerIpc::CloseReason::NoRelayRoute) {
        text = tr("この招待文字列には、あなたの協力ノードの中継を通る接続先がありません。「協力ノードの中継だけを使う」設定では、"
                  "相手と同じ協力ノードを「接続」タブで登録する必要があります。");
    } else {
        text = reason == PeerIpc::CloseReason::Timeout ? tr("相手に接続できませんでした（時間切れ）。") : tr("相手に接続できませんでした。");
        if (m_inviteInIsCode && m_inviteInHasAddress) {
            // 同じ LAN の合言葉には接続先が入っている。1文字違うと別の PC を指すことがある
            text += QLatin1Char('\n') + tr("合言葉が正しいか、招待した側のアプリが接続したままかを確かめてください。");
        }
        if (m_lobby && m_lobby->isLocalProfile()) {
            // 同じ LAN の別の PC: 招待した側の PC が、外から届く接続を止めていることが多い
            text += QLatin1Char('\n')
                    + tr("同じLANの別のPCとつなぐときは、招待した側のPCのファイアウォールで、TCP と UDP の %1 番ポートへの接続が"
                         "許可されているか確かめてください。招待する側を入れ替えると、つながることがあります。")
                          .arg(kP2pLanPort);
        }
    }
    m_inviteInInfo->clear();
    // 通信プロセスの知らせの処理の中なので、閉じるのを待たない（監査 2026-11-13 P4）。続けて届いたら前の箱を閉じる
    if (m_connectFailedBox) m_connectFailedBox->close();
    m_connectFailedBox = DialogUtils::createNoticeBox(this, QMessageBox::Warning, tr("接続"), text);
    m_connectFailedBox->show();
}

void P2pWindow::onCopyPlayerId()
{
    if (m_lobby && m_lobby->profile()) {
        QApplication::clipboard()->setText(QString::fromLatin1(m_lobby->profile()->identity()->publicKey().toHex()));
    }
}
