/// @file p2pinvitepanel.cpp
/// @brief P2P対局のウィンドウの「招待」タブの実装

#include "p2pinvitepanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "dialogutils.h"
#include "engineselector.h"
#include "networksettings.h"
#include "p2pdisplayname.h"
#include "p2pgamecoordinator.h"
#include "p2pwindowtext.h"

P2pInvitePanel::P2pInvitePanel(P2pLobbyService* lobby, P2pGameCoordinator* game, int player, QWidget* parent)
    : QWidget(parent)
    , m_lobby(lobby)
    , m_game(game)
    , m_player(player)
    , m_engineNameKey(NetworkSettings::p2pEngineNameKey(player))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(createConditionBox());
    // 招待する側と招待を受ける側は、どちらか一方を使うので、左右に並べる
    auto* columns = new QHBoxLayout;
    // 招待する側は出すものが多い（合言葉・招待文字列・注意）ので、幅を広く取る
    columns->addWidget(createInviteOutBox(), 3);
    columns->addWidget(createInviteInBox(), 2);
    layout->addLayout(columns);
    layout->addStretch();

    // ロビーと司令塔は、表示のために読むだけ（操作は意図のシグナルで知らせる）
    if (m_lobby) {
        connect(m_lobby, &P2pLobbyService::inviteCreated, this, &P2pInvitePanel::onInviteCreated);
        connect(m_lobby, &P2pLobbyService::inviteInspected, this, &P2pInvitePanel::onInviteInspected);
        connect(m_lobby, &P2pLobbyService::inspectRejected, this, &P2pInvitePanel::onInspectRejected);
    }
    if (m_game) connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pInvitePanel::clearConnectProgress);
}

// ---------------------------------------------------------------- 部品

QWidget* P2pInvitePanel::createConditionBox()
{
    // 名乗る名前と条件は、招待を作るときも、招待を受けるときも使う
    auto* box = new QGroupBox(tr("あなたと対局の条件"), this);
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

    layout->addWidget(DialogUtils::wrappedLabel(
        tr("エンジンを選ぶと、あなたの手はエンジンが指し、そのことが相手に伝わり、対局記録（証跡）にも残ります。"), box));
    EngineSelector::Options engineOptions;
    engineOptions.keys = {m_engineNameKey.constData(), nullptr, nullptr};
    engineOptions.builtinLabel = tr("人間");
    m_engineSelector = new EngineSelector({m_playerKind, m_engineSettings, nullptr}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested, this, &P2pInvitePanel::onEngineSettingsRequested);
    return box;
}

QWidget* P2pInvitePanel::createInviteOutBox()
{
    auto* box = new QGroupBox(tr("招待する"), this);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(DialogUtils::wrappedLabel(tr("上の条件で招待を作り、相手に伝えます。"), box));
    m_createInvite = new QPushButton(tr("招待を作る"), box);
    m_createInvite->setObjectName(QStringLiteral("p2pCreateInvite"));
    m_createInvite->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_createInvite, &QPushButton::clicked, this, &P2pInvitePanel::onCreateInvite);
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
    connect(m_copyCode, &QPushButton::clicked, this, &P2pInvitePanel::onCopyCode);
    codeLayout->addWidget(m_codeOut, 1, 0);
    codeLayout->addWidget(m_copyCode, 1, 1);
    m_codeRow->setVisible(false);
    layout->addWidget(m_codeRow);
    // 合言葉の付かない招待（協力ノードが合言葉に対応していない、など）は、理由と、招待文字列を渡すことを知らせる
    m_noCode = DialogUtils::wrappedLabel(QString(), box);
    m_noCode->setObjectName(QStringLiteral("p2pInviteNoCode"));
    m_noCode->setVisible(false);
    layout->addWidget(m_noCode);

    layout->addWidget(DialogUtils::wrappedLabel(tr("招待文字列（メッセージなどで送る長い形）"), box));
    m_inviteOut = new QPlainTextEdit(box);
    m_inviteOut->setObjectName(QStringLiteral("p2pInviteOut"));
    m_inviteOut->setReadOnly(true);
    m_inviteOut->setMaximumHeight(56);
    layout->addWidget(m_inviteOut);
    auto* outRow = new QHBoxLayout;
    m_inviteOutInfo = DialogUtils::wrappedLabel(QString(), box);
    m_copyInvite = new QPushButton(tr("招待文字列をコピー"), box);
    m_copyInvite->setObjectName(QStringLiteral("p2pCopyInvite"));
    connect(m_copyInvite, &QPushButton::clicked, this, &P2pInvitePanel::onCopyInvite);
    outRow->addWidget(m_inviteOutInfo, 1);
    outRow->addWidget(m_copyInvite, 0, Qt::AlignTop);
    layout->addLayout(outRow);
    m_inviteNotes = DialogUtils::wrappedLabel(QString(), box);
    m_inviteNotes->setObjectName(QStringLiteral("p2pInviteNotes"));
    layout->addWidget(m_inviteNotes);
    layout->addStretch();
    return box;
}

QWidget* P2pInvitePanel::createInviteInBox()
{
    // 上から順に進める: 入力する → 確かめる → 結果を見る → 接続する
    auto* box = new QGroupBox(tr("招待を受ける"), this);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(DialogUtils::wrappedLabel(tr("受け取った合言葉を入力するか、招待文字列を貼り付けてください。"), box));
    m_inviteIn = new QPlainTextEdit(box);
    m_inviteIn->setObjectName(QStringLiteral("p2pInviteIn"));
    m_inviteIn->setMaximumHeight(56);
    layout->addWidget(m_inviteIn);
    m_inspectInvite = new QPushButton(tr("確かめる"), box);
    m_inspectInvite->setObjectName(QStringLiteral("p2pInspectInvite"));
    connect(m_inspectInvite, &QPushButton::clicked, this, &P2pInvitePanel::onInspectInvite);
    layout->addWidget(m_inspectInvite);
    m_inviteInInfo = DialogUtils::wrappedLabel(QString(), box);
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
    connect(m_connectInvite, &QPushButton::clicked, this, &P2pInvitePanel::onConnectInvite);
    layout->addWidget(m_connectInvite);
    layout->addWidget(DialogUtils::wrappedLabel(
        tr("接続しただけでは対局は始まりません。条件確認画面で相手のプレイヤーIDを確かめてから開始します。"), box));
    layout->addStretch();
    return box;
}

// ---------------------------------------------------------------- 値と設定

P2pGameCoordinator::Conditions P2pInvitePanel::conditions() const
{
    P2pGameCoordinator::Conditions c;
    c.displayName = m_displayName->text();
    c.terms = P2p::TimeControlTerms{static_cast<quint64>(m_mainMinutes->value()) * 60'000,
                                    static_cast<quint64>(m_byoyomi->value()) * 1000};
    c.side = static_cast<P2p::SidePreference>(m_side->currentData().toInt());
    if (const std::optional<EngineEntry> engine = m_engineSelector->selected()) {
        c.player.kind = P2p::PlayerKind::Engine;
        c.player.enginePath = engine->path;
        c.player.engineName = engine->name;
    }
    return c;
}

bool P2pInvitePanel::hasValidDisplayName() const
{
    return P2p::DisplayName::isValid(m_displayName->text());
}

bool P2pInvitePanel::checkDisplayName()
{
    if (hasValidDisplayName()) return true;
    QString text;
    if (!m_displayName->text().isEmpty()) {
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
    m_displayName->setFocus();
    return false;
}

void P2pInvitePanel::loadSettings()
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
}

void P2pInvitePanel::saveSettings()
{
    if (hasValidDisplayName()) NetworkSettings::p2pDisplayName(m_player).set(m_displayName->text());
    NetworkSettings::p2pMainTimeSeconds.set(m_mainMinutes->value() * 60);
    NetworkSettings::p2pByoyomiSeconds.set(m_byoyomi->value());
    NetworkSettings::p2pSidePreference.set(m_side->currentData().toInt());
    // 人間を選んでいる間は、前に選んだエンジンを残す
    const bool engine = m_engineSelector->selected().has_value();
    NetworkSettings::p2pPlayAsEngine(m_player).set(engine);
    if (engine) m_engineSelector->saveSelection();
}

void P2pInvitePanel::reloadEngines()
{
    const std::optional<EngineEntry> before = m_engineSelector->selected();
    m_engineSelector->reload();
    const std::optional<EngineEntry> after = m_engineSelector->selected();
    // 読み直しは、選んでいたエンジンが無くなると先頭の登録エンジンを選ぶ。相手に伝える対局者を黙って替えない
    if (!before || !after || before->name != after->name) m_engineSelector->selectBuiltin();
}

void P2pInvitePanel::onEngineSettingsRequested(const EngineEntry& engine)
{
    ChangeEngineSettingsDialog::showFor(engine, this);
}

// ---------------------------------------------------------------- 表示

void P2pInvitePanel::refresh()
{
    const bool online = m_lobby && m_lobby->status() == P2pLobbyService::Status::Online;
    const bool busy = m_game && m_game->isBusy();
    // 交渉・対局の間は、相手に伝えた対局者を変えられない
    m_engineSelector->setEnabled(!busy);
    m_createInvite->setEnabled(online && !busy);
    m_inspectInvite->setEnabled(online && !busy);
    if (!online || busy) m_connectInvite->setEnabled(false);
}

void P2pInvitePanel::setInviteContentsText(const QString& text)
{
    const QStringList notes{tr("・相手だけに伝え、公開の場所には貼らないでください。"),
                            tr("・10分で使えなくなります。合言葉と招待文字列のどちらかで、1回だけ使えます。"),
                            tr("・%1").arg(text)};
    DialogUtils::setWrappedText(m_inviteNotes, notes.join(QLatin1Char('\n')));
}

void P2pInvitePanel::setInviteInText(const QString& text)
{
    DialogUtils::setWrappedText(m_inviteInInfo, text);
}

void P2pInvitePanel::showConnectResult(const QString& text)
{
    m_connecting = false;
    setInviteInText(text);
    emit contentResized();
}

void P2pInvitePanel::clearConnectProgress()
{
    if (!m_connecting) return;
    m_connecting = false;
    setInviteInText(QString());
}

// ---------------------------------------------------------------- 招待する

void P2pInvitePanel::onCreateInvite()
{
    if (!m_lobby || !checkDisplayName() || !conditions().terms.isValid()) return;
    saveSettings();
    // 新しい招待ができるまで（合言葉の鍵を作る間）、前の招待を新しい招待と取り違えないように消しておく
    m_inviteOut->clear();
    m_codeOut->clear();
    m_noCode->setVisible(false);
    DialogUtils::setWrappedText(m_inviteOutInfo, QString());
    emit createInviteRequested();
}

void P2pInvitePanel::onInviteCreated(const QString& invite, const QString& code, quint64 expiresAt, int addressCount)
{
    m_inviteOut->setPlainText(invite);
    m_codeOut->setText(code);
    m_codeRow->setVisible(!code.isEmpty());
    // 合言葉の付かない招待: 行を隠すだけだと、なぜ合言葉が出ないのかが分からない
    const bool noCode = code.isEmpty() && !invite.isEmpty();
    if (noCode) {
        // 協力ノードを使う招待（「インターネット」・中継だけ）は、合言葉に対応した協力ノードがあるときだけ合言葉が付く
        const bool viaNode = m_lobby && (!m_lobby->isLocalProfile() || m_lobby->relayOnly());
        const QString reason = viaNode && m_lobby->meetNodes() == 0
            ? tr("この招待には合言葉が付きません（登録した協力ノードが、合言葉に対応していません）。"
                 "下の招待文字列を、メッセージなどで相手に渡してください。")
            : tr("この招待には合言葉が付きません。下の招待文字列を、メッセージなどで相手に渡してください。");
        DialogUtils::setWrappedText(m_noCode, reason);
    }
    m_noCode->setVisible(noCode);
    m_inviteExpiresAt = expiresAt;
    // 接続先の入っていない招待文字列では、相手はつなげない
    const QString info = addressCount > 0
        ? tr("期限: %1（到達候補 %2 件）").arg(P2pWindowText::dateTimeText(expiresAt)).arg(addressCount)
        : tr("この招待文字列には接続先が入っていないので、相手は接続できません。"
             "ウィンドウの上の「招待の経路」を確かめてください。");
    DialogUtils::setWrappedText(m_inviteOutInfo, info);
    emit contentResized();
}

void P2pInvitePanel::onCopyInvite()
{
    copyInviteText(m_inviteOut->toPlainText());
}

void P2pInvitePanel::onCopyCode()
{
    copyInviteText(m_codeOut->text());
}

void P2pInvitePanel::copyInviteText(const QString& text)
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
    constexpr qint64 kLongestInviteSeconds = 15 * 60;
    const qint64 left = std::clamp<qint64>(static_cast<qint64>(m_inviteExpiresAt) - now, 0, kLongestInviteSeconds);
    QTimer::singleShot(static_cast<int>(left * 1000), this, &P2pInvitePanel::clearCopiedInvite);
}

void P2pInvitePanel::clearCopiedInvite()
{
    if (m_copiedInvite.isEmpty()) return;
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard->text() == m_copiedInvite) clipboard->clear();
    m_copiedInvite.clear();
}

// ---------------------------------------------------------------- 招待を受ける

void P2pInvitePanel::onInspectInvite()
{
    if (!m_lobby) return;
    m_connectInvite->setEnabled(false);
    m_allowPrivate->setVisible(false);
    setInviteInText(tr("確かめています…"));
    const QString input = m_inviteIn->toPlainText();
    m_inspectingCode = !P2pWindowText::isInviteString(input);
    m_inspectedKind.reset();
    emit inspectInviteRequested(P2pWindowText::inviteInput(input));
}

void P2pInvitePanel::onInviteInspected(const P2pLobbyService::InviteInfo& info)
{
    m_inspectedKind = info.kind;
    QStringList lines;
    if (info.kind == PeerIpc::InviteKind::LanCode) {
        // 同じ LAN の合言葉には、相手の PeerId と期限が入っていない（期限は招待した側が確かめる）
        lines << tr("合言葉を読みました（同じLANの相手の招待）。");
    } else {
        // 協力ノードを使う合言葉は、協力ノードから引き取った招待の中身が分かる
        if (info.kind == PeerIpc::InviteKind::NodeCode) lines << tr("合言葉の招待を、協力ノードから受け取りました。");
        lines << tr("招待した相手のPeerId: %1…").arg(info.peerId.left(16));
        lines << tr("期限: %1").arg(P2pWindowText::dateTimeText(info.expiresAt));
    }
    lines << tr("到達候補: %1 件（使える候補 %2 件）").arg(info.addressCount).arg(info.usableAddressCount);
    if (!info.privateAddresses.isEmpty()) {
        lines << tr("私設アドレス（同じLANの相手）:");
        lines << info.privateAddresses;
    }
    // つなげる接続先が無ければ「接続する」を押せない。その理由と直し方を添える
    const bool connectable = info.usableAddressCount > 0 || !info.privateAddresses.isEmpty();
    if (!connectable) lines << P2pWindowText::unusableInviteText(info.kind, m_lobby && m_lobby->relayOnly());
    setInviteInText(lines.join(QLatin1Char('\n')));
    m_allowPrivate->setChecked(false);
    m_allowPrivate->setVisible(!info.privateAddresses.isEmpty());
    m_connectInvite->setEnabled(connectable);
    emit contentResized();
}

void P2pInvitePanel::onInspectRejected(PeerIpc::InviteRejection rejection)
{
    setInviteInText(P2pWindowText::inviteRejectionText(rejection, m_inspectingCode));
    emit contentResized();
}

void P2pInvitePanel::onConnectInvite()
{
    if (!m_lobby || !checkDisplayName()) return;
    saveSettings();
    m_connectInvite->setEnabled(false);
    const QString input = m_inviteIn->toPlainText();
    // 合言葉は、確かめた結果で、同じ LAN の合言葉（接続先が入っている）か、協力ノードを使う合言葉かが分かる
    using Attempt = P2pLobbyService::ConnectAttempt;
    const Attempt attempt = P2pWindowText::isInviteString(input) ? Attempt::InviteString
                            : m_inspectedKind == PeerIpc::InviteKind::LanCode ? Attempt::LanCode
                                                                              : Attempt::NodeCode;
    emit connectInviteRequested(P2pWindowText::inviteInput(input), attempt, m_allowPrivate->isChecked());
    // 招待文字列・合言葉はもう使わない（ログ・設定に残さない）
    m_inviteIn->clear();
    setInviteInText(tr("接続しています…"));
    m_connecting = true;
}
