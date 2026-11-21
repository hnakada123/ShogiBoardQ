/// @file p2pconnectionpanel.cpp
/// @brief 「接続」タブの実装

#include "p2pconnectionpanel.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QVBoxLayout>

#include <memory>

#include "dialogutils.h"
#include "p2pchoicecard.h"
#include "p2pprotocol.h"
#include "plaintooltip.h"

namespace {

const QLatin1String kLocalNetwork(P2p::kLocalNetworkName);
const QLatin1String kInternetNetwork(P2p::kInternetNetworkName);

/// 協力ノードのアドレスの形（/…/p2p/<PeerId>）をしているか。中身を解釈するのは通信プロセスで、ここでは、
/// アドレスでない文字列（説明の文や合言葉）を入れたことに気づけるだけの確かめをする
bool looksLikeNodeAddress(const QString& text)
{
    if (!text.startsWith(QLatin1Char('/')) || text.size() > 512) return false;
    for (const QChar c : text) {
        if (c.isSpace() || c.unicode() < 0x21 || c.unicode() > 0x7e) return false;
    }
    const qsizetype peer = text.lastIndexOf(QLatin1String("/p2p/"));
    return peer > 0 && peer + 5 < text.size();
}

} // namespace

P2pConnectionPanel::P2pConnectionPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_lockNote = DialogUtils::wrappedLabel(tr("接続している間は、つなぎ方を変えられません。変えるときは「切断」を押してください。"), this);
    m_lockNote->setObjectName(QStringLiteral("p2pConnectionLock"));
    m_lockNote->setVisible(false);
    layout->addWidget(m_lockNote);

    // --- 相手のいる場所（参加するネットワーク）
    layout->addWidget(DialogUtils::headingLabel(tr("相手のいる場所"), this));
    m_lan = new P2pChoiceCard(tr("同じPC・同じLAN"), this);
    m_lan->radio()->setObjectName(QStringLiteral("p2pPlaceLan"));
    m_lan->setRows({{QString(), tr("協力ノードなしで、招待だけでつながります。")}});
    m_internet = new P2pChoiceCard(tr("インターネット"), this);
    m_internet->radio()->setObjectName(QStringLiteral("p2pPlaceInternet"));
    m_internet->setRows({{QString(), tr("離れた相手とつなぎます。協力ノードが要ります。")}});
    auto* places = new QButtonGroup(this);
    places->addButton(m_lan->radio());
    places->addButton(m_internet->radio());
    auto* placeRow = new QHBoxLayout;
    placeRow->addWidget(m_lan, 1);
    placeRow->addWidget(m_internet, 1);
    layout->addLayout(placeRow);

    // --- 相手とのつなぎ方（チェックだと、付けない場合に何が起きるかが分からないので、2つを並べて説明する）
    layout->addWidget(DialogUtils::headingLabel(tr("相手とのつなぎ方"), this));
    m_direct = new P2pChoiceCard(tr("直接つなぐ（既定）"), this);
    m_direct->radio()->setObjectName(QStringLiteral("p2pRouteDirect"));
    m_relay = new P2pChoiceCard(tr("協力ノードの中継だけを使う"), this);
    m_relay->radio()->setObjectName(QStringLiteral("p2pRouteRelay"));
    auto* routes = new QButtonGroup(this);
    routes->addButton(m_direct->radio());
    routes->addButton(m_relay->radio());
    auto* routeRow = new QHBoxLayout;
    routeRow->addWidget(m_direct, 1);
    routeRow->addWidget(m_relay, 1);
    layout->addLayout(routeRow);

    // 協力ノードの欄は、選択肢の下に置く（選択肢を押したときに、押した所より下で出入りする）
    layout->addWidget(createNodeSection());
    layout->addWidget(createNodeCollapsed());
    layout->addWidget(createSummary());
    layout->addWidget(createWarning());
    // 余った高さは、下の空きにだけ渡す（枠の中に伸びる空きがあるので、伸びる割合を付けないと、選択肢の枠も一緒に伸びる）
    layout->addStretch(1);

    m_lan->radio()->setChecked(true);
    m_direct->radio()->setChecked(true);
    for (QRadioButton* radio : {m_lan->radio(), m_internet->radio(), m_direct->radio(), m_relay->radio()}) {
        connect(radio, &QRadioButton::toggled, this, &P2pConnectionPanel::onChoiceToggled);
    }
    updateNodeListHeight();
    updateTexts();
}

QWidget* P2pConnectionPanel::createNodeSection()
{
    // 協力ノード（要る組み合わせのときと、登録があるときに出す）
    m_nodeSection = new QWidget(this);
    auto* layout = new QVBoxLayout(m_nodeSection);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* header = new QHBoxLayout;
    header->addWidget(DialogUtils::headingLabel(tr("協力ノード"), m_nodeSection), 0, Qt::AlignTop);
    m_nodeCaption = DialogUtils::wrappedLabel(QString(), m_nodeSection);
    header->addWidget(m_nodeCaption, 1);
    layout->addLayout(header);
    m_nodes = new QListWidget(m_nodeSection);
    m_nodes->setObjectName(QStringLiteral("p2pBootstrap"));
    m_nodes->setSelectionMode(QAbstractItemView::SingleSelection);
    m_nodes->setTextElideMode(Qt::ElideMiddle);
    m_nodes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nodes->installEventFilter(this);
    layout->addWidget(m_nodes);
    auto* buttons = new QHBoxLayout;
    m_addNode = new QPushButton(tr("追加…"), m_nodeSection);
    m_addNode->setObjectName(QStringLiteral("p2pNodeAdd"));
    m_pasteNodes = new QPushButton(tr("クリップボードから貼り付け"), m_nodeSection);
    m_pasteNodes->setObjectName(QStringLiteral("p2pNodePaste"));
    m_removeNode = new QPushButton(tr("削除"), m_nodeSection);
    m_removeNode->setObjectName(QStringLiteral("p2pNodeRemove"));
    for (QPushButton* button : {m_addNode, m_pasteNodes, m_removeNode}) {
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    // 接続している間は、協力ノードで中継と合言葉を使えるかを、ボタンの横に出す
    m_nodeStatus = DialogUtils::wrappedLabel(QString(), m_nodeSection);
    m_nodeStatus->setObjectName(QStringLiteral("p2pNodeStatus"));
    buttons->addWidget(m_nodeStatus, 1);
    layout->addLayout(buttons);
    connect(m_addNode, &QPushButton::clicked, this, &P2pConnectionPanel::onAddNode);
    connect(m_pasteNodes, &QPushButton::clicked, this, &P2pConnectionPanel::onPasteNodes);
    connect(m_removeNode, &QPushButton::clicked, this, &P2pConnectionPanel::onRemoveNode);
    connect(m_nodes, &QListWidget::itemSelectionChanged, this, &P2pConnectionPanel::updateNodeButtons);
    return m_nodeSection;
}

QWidget* P2pConnectionPanel::createNodeCollapsed()
{
    // 協力ノードが要らない組み合わせでは、1行だけ出す（友だち一覧などで使う人のために、開く入口は残す）
    m_nodeCollapsed = new QWidget(this);
    auto* layout = new QHBoxLayout(m_nodeCollapsed);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(DialogUtils::headingLabel(tr("協力ノード"), m_nodeCollapsed));
    layout->addWidget(
        DialogUtils::wrappedLabel(tr("この設定では要りません。協力ノードの中継や友だち一覧を使うときだけ登録します。"), m_nodeCollapsed), 1);
    m_showNodes = new QPushButton(tr("登録する…"), m_nodeCollapsed);
    m_showNodes->setObjectName(QStringLiteral("p2pNodeShow"));
    m_showNodes->setAutoDefault(false);
    layout->addWidget(m_showNodes);
    connect(m_showNodes, &QPushButton::clicked, this, &P2pConnectionPanel::onShowNodes);
    return m_nodeCollapsed;
}

QWidget* P2pConnectionPanel::createSummary()
{
    // この設定でつなぐと（選んだ組み合わせの結果）
    m_summary = new QFrame(this);
    m_summary->setObjectName(QStringLiteral("p2pConnectionSummary"));
    m_summary->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QGridLayout(m_summary);
    layout->setVerticalSpacing(2);
    layout->setColumnStretch(1, 1);
    layout->addWidget(DialogUtils::headingLabel(tr("この設定でつなぐと"), m_summary), 0, 0, 1, 2);
    const QStringList captions{tr("対局できる相手"), tr("招待の形"), tr("あなたのIPアドレスが見える人")};
    QList<QLabel*> values;
    for (int row = 0; row < captions.size(); ++row) {
        auto* caption = new QLabel(captions.at(row), m_summary);
        caption->setTextFormat(Qt::PlainText);
        layout->addWidget(caption, row + 1, 0, Qt::AlignLeft | Qt::AlignTop);
        values << DialogUtils::wrappedLabel(QString(), m_summary);
        layout->addWidget(values.last(), row + 1, 1);
    }
    m_summaryWho = values.at(0);
    m_summaryInvite = values.at(1);
    m_summarySeen = values.at(2);
    return m_summary;
}

QWidget* P2pConnectionPanel::createWarning()
{
    // つながらない組み合わせでは、結果の代わりに、理由と直し方を出す
    m_warning = new QFrame(this);
    m_warning->setObjectName(QStringLiteral("p2pConnectionWarning"));
    m_warning->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QHBoxLayout(m_warning);
    auto* icon = new QLabel(m_warning);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize) + 4;
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize, iconSize));
    layout->addWidget(icon, 0, Qt::AlignTop);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    m_warningTitle = DialogUtils::headingLabel(QString(), m_warning);
    m_warningTitle->setObjectName(QStringLiteral("p2pConnectionWarningTitle"));
    m_warningBody = DialogUtils::wrappedLabel(QString(), m_warning);
    texts->addWidget(m_warningTitle);
    texts->addWidget(m_warningBody);
    layout->addLayout(texts, 1);
    return m_warning;
}

// ---------------------------------------------------------------- 値

bool P2pConnectionPanel::isLan() const { return m_lan->radio()->isChecked(); }

QString P2pConnectionPanel::networkName() const
{
    return isLan() ? QString(kLocalNetwork) : QString(kInternetNetwork);
}

void P2pConnectionPanel::setNetworkName(const QString& name)
{
    m_loading = true;
    (name == kInternetNetwork ? m_internet : m_lan)->radio()->setChecked(true);
    m_loading = false;
    updateTexts();
}

QStringList P2pConnectionPanel::bootstrap() const
{
    QStringList addresses;
    for (int row = 0; row < m_nodes->count() && addresses.size() < kMaxNodes; ++row) addresses << m_nodes->item(row)->text();
    return addresses;
}

void P2pConnectionPanel::setBootstrap(const QStringList& addresses)
{
    m_nodes->clear();
    // 保存してあった行は、形を確かめずにそのまま出す（以前の版で入れた行を黙って消さない）
    for (const QString& address : addresses) {
        const QString trimmed = address.trimmed();
        if (trimmed.isEmpty() || m_nodes->count() >= kMaxNodes) continue;
        auto* item = new QListWidgetItem(trimmed, m_nodes);
        item->setToolTip(plainToolTip(trimmed));
    }
    updateTexts();
}

bool P2pConnectionPanel::relayOnly() const { return m_relay->radio()->isChecked(); }

void P2pConnectionPanel::setRelayOnly(bool relayOnly)
{
    m_loading = true;
    (relayOnly ? m_relay : m_direct)->radio()->setChecked(true);
    m_loading = false;
    updateTexts();
}

QString P2pConnectionPanel::settingSummary() const
{
    const int count = m_nodes->count();
    return tr("%1・%2（%3）")
        .arg(isLan() ? tr("同じPC・同じLAN") : tr("インターネット"),
             relayOnly() ? tr("協力ノードの中継だけを使う") : tr("直接つなぐ"),
             count > 0 ? tr("協力ノード %1件").arg(count) : tr("協力ノードなし"));
}

QString P2pConnectionPanel::inviteContentsText() const
{
    if (relayOnly()) return tr("招待には、協力ノードを通る接続先だけが入ります（あなたのIPアドレスは入りません）。");
    if (isLan()) return tr("招待には、このPCのLANの中のアドレスが入ります。");
    return tr("招待には、あなたのIPアドレスが入ります（今の設定は「直接つなぐ」）。");
}

void P2pConnectionPanel::setLocked(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    for (P2pChoiceCard* card : {m_lan, m_internet, m_direct, m_relay}) card->setLocked(locked);
    m_lockNote->setVisible(locked);
    updateNodeButtons();
    emit contentResized();
}

void P2pConnectionPanel::setNodeStatus(const std::optional<NodeStatus>& status)
{
    const bool same = status.has_value() == m_status.has_value()
                      && (!status || (status->relayReservations == m_status->relayReservations
                                      && status->meetNodes == m_status->meetNodes));
    if (same) return;
    m_status = status;
    updateTexts();
}

// ---------------------------------------------------------------- 表示

void P2pConnectionPanel::updateTexts()
{
    const bool lan = isLan();
    const bool relay = relayOnly();
    const bool hasNode = m_nodes->count() > 0;
    const QString route = tr("経路");
    const QString told = tr("相手に伝わる");
    const QString needs = tr("要るもの");

    if (lan) {
        m_direct->setRows({{route, tr("同じPC・同じLANの相手と直接つなぎます。")},
                           {told, tr("LANの中のアドレスだけ")},
                           {needs, tr("なし")}});
    } else {
        m_direct->setRows({{route, tr("相手と直接つなぎます。届かないときは、協力ノードの中継を通ります。")},
                           {told, tr("あなたのIPアドレス")},
                           {needs, tr("協力ノード（アドレスの確認と中継に使います）")}});
    }
    m_relay->setRows({{route, tr("いつも協力ノードの中継を通ります。直接はつなぎません。")},
                      {told, tr("IPアドレスは伝わりません")},
                      {needs, tr("中継のできる協力ノード（相手も同じものを登録）")}});

    // 協力ノードの欄: 要る組み合わせ・登録がある・利用者が開いた、のどれかのときに出す
    const bool needsNode = !lan || relay;
    const bool showNodes = needsNode || hasNode || m_nodesOpened;
    m_nodeSection->setVisible(showNodes);
    m_nodeCollapsed->setVisible(!showNodes);
    QString caption;
    if (relay) {
        caption = tr("この設定では要ります。対局の通信をすべて中継します。相手も同じ協力ノードを登録します。");
    } else if (!lan) {
        caption = tr("この設定では要ります。あなたのアドレスの確認と、直接届かないときの中継に使います。"
                     "2人とも同じアドレスを登録します。");
    } else {
        caption = tr("この設定では要りません（任意）。登録すると、協力ノードの中継と友だち一覧を使えます。");
    }
    DialogUtils::setWrappedText(m_nodeCaption, caption);
    QString status;
    if (m_status && hasNode) {
        const QStringList parts{m_status->relayReservations > 0 ? tr("中継: 使えます") : tr("中継: 使えません"),
                                m_status->meetNodes > 0 ? tr("合言葉・友だち一覧: 使えます")
                                                        : tr("合言葉・友だち一覧: 対応した協力ノードがありません")};
        status = parts.join(QStringLiteral(" / "));
    }
    DialogUtils::setWrappedText(m_nodeStatus, status);

    // つながらない組み合わせ: 中継だけなのに協力ノードがない・「インターネット」なのに協力ノードがない
    const bool problem = needsNode && !hasNode;
    m_summary->setVisible(!problem);
    m_warning->setVisible(problem);
    if (problem) {
        if (relay) {
            m_warningTitle->setText(tr("このままでは、中継を使えません"));
            DialogUtils::setWrappedText(m_warningBody, tr("「協力ノードの中継だけを使う」には、中継のできる協力ノードが要ります。"
                                                          "「追加…」で登録するか、「直接つなぐ」を選んでください。"));
        } else {
            m_warningTitle->setText(tr("このままでは、相手は接続できません"));
            DialogUtils::setWrappedText(m_warningBody,
                                        tr("「インターネット」では、協力ノードがないと招待に接続先が入りません。「追加…」で協力ノードを"
                                           "登録してください。同じPC・同じLANの相手とは、「同じPC・同じLAN」を選べば"
                                           "協力ノードなしでつながります。"));
        }
    } else {
        const QString viaNode = tr("招待文字列。協力ノードが合言葉に対応していれば、10文字の合言葉も");
        QString who;
        QString invite;
        QString seen;
        if (relay) {
            who = lan ? tr("同じPC・同じLANの相手（協力ノード経由）") : tr("インターネット上の相手（同じ協力ノードを登録した人）");
            invite = viaNode;
            seen = tr("協力ノードの運営者だけ（相手には見えません）");
        } else if (lan) {
            who = tr("同じPC・同じLANの相手");
            invite = tr("12文字ほどの合言葉か、招待文字列");
            seen = hasNode ? tr("相手（LANの中のアドレスだけ）と、協力ノードの運営者") : tr("相手（LANの中のアドレスだけ）");
        } else {
            who = tr("インターネット上の相手");
            invite = viaNode;
            seen = tr("相手と、協力ノードの運営者");
        }
        DialogUtils::setWrappedText(m_summaryWho, who);
        DialogUtils::setWrappedText(m_summaryInvite, invite);
        DialogUtils::setWrappedText(m_summarySeen, seen);
    }
    updateNodeButtons();
    emit contentResized();
}

void P2pConnectionPanel::updateNodeButtons()
{
    const bool full = m_nodes->count() >= kMaxNodes;
    m_addNode->setEnabled(!m_locked && !full);
    m_pasteNodes->setEnabled(!m_locked && !full);
    m_removeNode->setEnabled(!m_locked && m_nodes->currentItem() != nullptr && m_nodes->currentItem()->isSelected());
    m_showNodes->setEnabled(!m_locked);
}

void P2pConnectionPanel::updateNodeListHeight()
{
    // 2行分から3行分（増えたらスクロールする。伸び縮みさせると、ウィンドウの推奨の高さが一覧の既定の高さになる）
    const int row = m_nodes->fontMetrics().height() + 8;
    const int frame = m_nodes->frameWidth() * 2;
    m_nodes->setMinimumHeight(row * 2 + frame);
    m_nodes->setMaximumHeight(row * 3 + frame);
}

bool P2pConnectionPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_nodes && event->type() == QEvent::FontChange) updateNodeListHeight();
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------- 操作

void P2pConnectionPanel::onChoiceToggled(bool checked)
{
    // 組の片方が外れ、もう片方が付く。付いた側の知らせだけで出し直す
    if (!checked || m_loading) return;
    updateTexts();
    emit settingsChanged();
}

int P2pConnectionPanel::addAddresses(const QStringList& addresses)
{
    int added = 0;
    for (const QString& address : addresses) {
        const QString trimmed = address.trimmed();
        if (trimmed.isEmpty() || m_nodes->count() >= kMaxNodes || !looksLikeNodeAddress(trimmed)) continue;
        bool listed = false;
        for (int row = 0; row < m_nodes->count() && !listed; ++row) listed = m_nodes->item(row)->text() == trimmed;
        if (listed) continue;
        auto* item = new QListWidgetItem(trimmed, m_nodes);
        item->setToolTip(plainToolTip(trimmed));
        ++added;
    }
    if (added > 0) {
        updateTexts();
        emit settingsChanged();
    }
    return added;
}

void P2pConnectionPanel::onAddNode()
{
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("協力ノードを追加"),
                                               tr("協力ノードのアドレス（/ip4/…/tcp/…/p2p/… の形）"), QLineEdit::Normal,
                                               QString(), &ok);
    // ダイアログの間に接続が始まっていたら、足さない
    if (!ok || m_locked || text.trimmed().isEmpty()) return;
    if (addAddresses({text}) > 0) return;
    DialogUtils::warning(this, tr("協力ノード"),
                         tr("追加できませんでした。アドレスは /ip4/…/tcp/…/p2p/… の形で、同じアドレスは1つだけ、"
                            "%1件まで登録できます。")
                             .arg(kMaxNodes));
}

void P2pConnectionPanel::onPasteNodes()
{
    const QStringList lines = QApplication::clipboard()->text().split(QLatin1Char('\n'));
    if (addAddresses(lines) > 0) return;
    DialogUtils::warning(this, tr("協力ノード"),
                         tr("クリップボードに、追加できる協力ノードのアドレス（/ip4/…/tcp/…/p2p/… の形）がありません。"));
}

void P2pConnectionPanel::onRemoveNode()
{
    if (m_locked) return;
    const std::unique_ptr<QListWidgetItem> removed(m_nodes->takeItem(m_nodes->currentRow()));
    updateTexts();
    emit settingsChanged();
}

void P2pConnectionPanel::onShowNodes()
{
    m_nodesOpened = true;
    updateTexts();
}
