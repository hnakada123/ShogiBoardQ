/// @file p2ppeerspanel.cpp
/// @brief 既知の相手の管理の実装

#include "p2ppeerspanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "dialogutils.h"
#include "p2pfriend.h"
#include "p2pfriendpresence.h"
#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2pprotocol.h"
#include "p2pwindowtext.h"
#include "plaintooltip.h"

namespace {

enum Column { PlayerId, LocalName, DisplayName, Friend, Verified, Blocked, Games, Unconfirmed, ColumnCount };

} // namespace

P2pPeersPanel::P2pPeersPanel(P2pKnownPeers* knownPeers, P2pIdentityStore* identity, QWidget* parent)
    : QWidget(parent)
    , m_knownPeers(knownPeers)
    , m_identity(identity)
{
    auto* layout = new QVBoxLayout(this);
    auto* note = new QLabel(tr("相手が同じ人かどうかは、プレイヤーIDで確かめます。表示名は誰でも同じものを名乗れます。"
                               "電話や対面など別の経路で確認コードを見比べ、一致したら「確認済み」にしてください。"
                               "相手のプレイヤーIDと対局の記録は、あなたの手元に残る他人の情報でもあります。"),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);
    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setObjectName(QStringLiteral("p2pPeerTable"));
    m_table->setHorizontalHeaderLabels({tr("プレイヤーID"), tr("手元の名前"), tr("最後の表示名"), tr("友だち"), tr("確認済み"),
                                        tr("ブロック"), tr("対局数"), tr("未確定")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 列は中身と見出しが切れない幅に合わせ、最後の列は残りの幅まで伸ばす（resizeColumnsToContents は最後の列の伸びを外す）
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &P2pPeersPanel::onSelectionChanged);
    layout->addWidget(m_table, 1);
    auto* buttons = new QHBoxLayout;
    m_rename = new QPushButton(tr("名前を付ける…"), this);
    m_rename->setObjectName(QStringLiteral("p2pPeerRename"));
    m_code = new QPushButton(tr("確認コード…"), this);
    m_code->setObjectName(QStringLiteral("p2pPeerCode"));
    m_verified = new QPushButton(this);
    m_verified->setObjectName(QStringLiteral("p2pPeerVerified"));
    m_blocked = new QPushButton(this);
    m_blocked->setObjectName(QStringLiteral("p2pPeerBlocked"));
    m_friend = new QPushButton(this);
    m_friend->setObjectName(QStringLiteral("p2pPeerFriend"));
    m_apply = new QPushButton(tr("対局を申し込む"), this);
    m_apply->setObjectName(QStringLiteral("p2pPeerApply"));
    m_remove = new QPushButton(tr("記録を削除"), this);
    m_remove->setObjectName(QStringLiteral("p2pPeerRemove"));
    connect(m_rename, &QPushButton::clicked, this, &P2pPeersPanel::onRename);
    connect(m_code, &QPushButton::clicked, this, &P2pPeersPanel::onShowCode);
    connect(m_verified, &QPushButton::clicked, this, &P2pPeersPanel::onToggleVerified);
    connect(m_blocked, &QPushButton::clicked, this, &P2pPeersPanel::onToggleBlocked);
    connect(m_friend, &QPushButton::clicked, this, &P2pPeersPanel::onToggleFriend);
    connect(m_apply, &QPushButton::clicked, this, &P2pPeersPanel::onApply);
    connect(m_remove, &QPushButton::clicked, this, &P2pPeersPanel::onRemove);
    for (QPushButton* b : {m_rename, m_code, m_verified, m_blocked}) buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(m_remove);
    layout->addLayout(buttons);
    // 友だち（在席を知らせ合い、一覧から対局を申し込む相手）
    auto* friendButtons = new QHBoxLayout;
    friendButtons->addWidget(m_friend);
    friendButtons->addWidget(m_apply);
    friendButtons->addStretch();
    // 在席を知らせるかは、利用者が決める（既定は無効。保存は受け手が行う）
    m_share = new QCheckBox(tr("友だちに在席を知らせる"), this);
    m_share->setObjectName(QStringLiteral("p2pSharePresence"));
    connect(m_share, &QCheckBox::toggled, this, &P2pPeersPanel::sharingToggled);
    friendButtons->addWidget(m_share);
    layout->addLayout(friendButtons);
    m_friendNote = new QLabel(this);
    m_friendNote->setObjectName(QStringLiteral("p2pFriendNote"));
    m_friendNote->setTextFormat(Qt::PlainText);
    m_friendNote->setWordWrap(true);
    layout->addWidget(m_friendNote);
    onSelectionChanged();
}

void P2pPeersPanel::setPresence(P2pFriendPresence* presence)
{
    if (m_presence == presence) return;
    if (m_presence) m_presence->disconnect(this);
    m_presence = presence;
    if (m_presence) {
        connect(m_presence, &P2pFriendPresence::presenceChanged, this, &P2pPeersPanel::reload);
        m_presence->setWatching(isVisible());
    }
    reload();
}

void P2pPeersPanel::setSharing(bool sharing)
{
    const QSignalBlocker block(m_share);
    m_share->setChecked(sharing);
}

void P2pPeersPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_presence) {
        m_presence->setWatching(true);
        m_presence->refreshNow();
    }
}

void P2pPeersPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 在席を調べるのは、一覧を表示している間だけ（協力ノードへの問い合わせを続けない）
    if (m_presence) m_presence->setWatching(false);
}

void P2pPeersPanel::reload()
{
    const QByteArray selected = selectedKey();
    m_table->setRowCount(0);
    if (!m_knownPeers) return;
    const QList<P2pKnownPeer> peers = m_knownPeers->all();
    for (const P2pKnownPeer& peer : peers) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* id = new QTableWidgetItem(P2pWindowText::shortPlayerId(peer.playerKey));
        id->setData(Qt::UserRole, peer.playerKey);
        id->setToolTip(plainToolTip(P2pWindowText::playerIdText(peer.playerKey)));
        m_table->setItem(row, PlayerId, id);
        m_table->setItem(row, LocalName, new QTableWidgetItem(peer.localName));
        auto* display = new QTableWidgetItem(peer.lastDisplayName);
        display->setToolTip(plainToolTip(peer.lastDisplayName));
        m_table->setItem(row, DisplayName, display);
        const bool online = peer.isFriend && m_presence && m_presence->isOnline(peer.playerKey);
        m_table->setItem(row, Friend, new QTableWidgetItem(online ? tr("オンライン") : peer.isFriend ? tr("友だち") : QString()));
        m_table->setItem(row, Verified, new QTableWidgetItem(peer.verified ? tr("確認済み") : QString()));
        m_table->setItem(row, Blocked, new QTableWidgetItem(peer.blocked ? tr("ブロック中") : QString()));
        m_table->setItem(row, Games, new QTableWidgetItem(QString::number(peer.games)));
        m_table->setItem(row, Unconfirmed, new QTableWidgetItem(QString::number(peer.unconfirmed)));
        if (peer.playerKey == selected) m_table->selectRow(row);
    }
    onSelectionChanged();
}

QByteArray P2pPeersPanel::selectedKey() const
{
    const int row = m_table->currentRow();
    return row < 0 || !m_table->item(row, PlayerId) ? QByteArray() : m_table->item(row, PlayerId)->data(Qt::UserRole).toByteArray();
}

void P2pPeersPanel::onSelectionChanged()
{
    const QByteArray key = selectedKey();
    const P2pKnownPeer* peer = m_knownPeers && !key.isEmpty() ? m_knownPeers->find(key) : nullptr;
    for (QPushButton* b : {m_rename, m_code, m_verified, m_blocked, m_remove}) b->setEnabled(peer != nullptr);
    m_verified->setText(peer && peer->verified ? tr("確認済みを外す") : tr("確認済みにする"));
    m_blocked->setText(peer && peer->blocked ? tr("ブロックを解除") : tr("ブロック"));
    // ブロックした相手は友だちにできない。申し込めるのは、在席を知らせている友だちだけ
    m_friend->setText(peer && peer->isFriend ? tr("友だちから外す") : tr("友だちに追加"));
    m_friend->setEnabled(peer != nullptr && (peer->isFriend || !peer->blocked));
    m_apply->setEnabled(peer != nullptr && peer->isFriend && m_presence && m_presence->isOnline(key));
    const bool available = m_presence && m_presence->isAvailable();
    // 在席を知らせると何が起きるか・だれに何が分かるかは、チェックのそばにいつも出す
    const QString sharing = tr("「友だちに在席を知らせる」を有効にすると、接続している間、互いに友だちに追加した相手にだけ、"
                               "接続していること（オンライン）が伝わり、対局の申し込みを受けます。合言葉に対応した協力ノードを使い、"
                               "その運営者には、だれとだれが友だちか（IPアドレスの組）が分かります。");
    m_friendNote->setText(available
        ? sharing
        : tr("友だちの在席を調べたり、友だちに対局を申し込んだりするには、合言葉に対応した協力ノードを"
             "「設定」の「接続候補」に入れて接続します。")
              + QLatin1Char('\n') + sharing);
}

void P2pPeersPanel::onToggleFriend()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    const P2pKnownPeer* peer = m_knownPeers->find(key);
    const bool add = !(peer && peer->isFriend);
    if (!m_knownPeers->setFriend(key, add)) {
        DialogUtils::information(this, tr("友だち"), tr("友だちにできるのは %1 人までです。").arg(P2p::Friend::kMaxFriends));
        return;
    }
    if (m_presence) m_presence->reloadFriends();
    reload();
}

void P2pPeersPanel::onApply()
{
    const QByteArray key = selectedKey();
    if (!key.isEmpty()) emit applyRequested(key);
}

void P2pPeersPanel::onRename()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    const P2pKnownPeer* peer = m_knownPeers->find(key);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("名前を付ける"),
                                               tr("この相手に手元で付ける名前（相手と棋譜には出ません）"), QLineEdit::Normal,
                                               peer ? peer->localName : QString(), &ok);
    if (!ok) return;
    m_knownPeers->setLocalName(key, name);
    reload();
}

void P2pPeersPanel::onShowCode()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_identity || m_identity->publicKey().isEmpty()) return;
    const QStringList code = P2p::verificationCode(m_identity->publicKey(), key);
    DialogUtils::information(this, tr("確認コード"),
                             tr("相手と別の経路（電話や対面など）で、次の数字を見比べてください。一致したら「確認済みにする」を押してください。\n\n%1")
                                 .arg(code.join(QLatin1Char(' '))));
}

void P2pPeersPanel::onToggleVerified()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    const P2pKnownPeer* peer = m_knownPeers->find(key);
    m_knownPeers->setVerified(key, !(peer && peer->verified));
    reload();
}

void P2pPeersPanel::onToggleBlocked()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    const P2pKnownPeer* peer = m_knownPeers->find(key);
    m_knownPeers->setBlocked(key, !(peer && peer->blocked));
    // ブロックした相手は友だちから外れる
    if (m_presence) m_presence->reloadFriends();
    reload();
}

void P2pPeersPanel::onRemove()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    if (!DialogUtils::confirmAction(this, tr("記録を削除"), tr("この相手の記録（名前・確認済み・ブロック・対局数）を削除しますか？"),
                                    tr("削除"), QMessageBox::Warning)) {
        return;
    }
    m_knownPeers->remove(key);
    if (m_presence) m_presence->reloadFriends();
    reload();
}
