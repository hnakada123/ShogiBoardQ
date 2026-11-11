/// @file p2ppeerspanel.cpp
/// @brief 既知の相手の管理の実装

#include "p2ppeerspanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "dialogutils.h"
#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2pprotocol.h"
#include "p2pwindowtext.h"
#include "plaintooltip.h"

namespace {

enum Column { PlayerId, LocalName, DisplayName, Verified, Blocked, Games, Unconfirmed, ColumnCount };

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
    m_table->setHorizontalHeaderLabels({tr("プレイヤーID"), tr("手元の名前"), tr("最後の表示名"), tr("確認済み"), tr("ブロック"),
                                        tr("対局数"), tr("未確定")});
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
    m_code = new QPushButton(tr("確認コード…"), this);
    m_verified = new QPushButton(this);
    m_blocked = new QPushButton(this);
    m_remove = new QPushButton(tr("記録を削除"), this);
    connect(m_rename, &QPushButton::clicked, this, &P2pPeersPanel::onRename);
    connect(m_code, &QPushButton::clicked, this, &P2pPeersPanel::onShowCode);
    connect(m_verified, &QPushButton::clicked, this, &P2pPeersPanel::onToggleVerified);
    connect(m_blocked, &QPushButton::clicked, this, &P2pPeersPanel::onToggleBlocked);
    connect(m_remove, &QPushButton::clicked, this, &P2pPeersPanel::onRemove);
    for (QPushButton* b : {m_rename, m_code, m_verified, m_blocked}) buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(m_remove);
    layout->addLayout(buttons);
    onSelectionChanged();
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
}

void P2pPeersPanel::onRename()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    P2pKnownPeer& peer = m_knownPeers->upsert(key);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("名前を付ける"),
                                               tr("この相手に手元で付ける名前（相手と棋譜には出ません）"), QLineEdit::Normal,
                                               peer.localName, &ok);
    if (!ok) return;
    m_knownPeers->upsert(key).localName = name.trimmed().left(64);
    m_knownPeers->save();
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
    P2pKnownPeer& peer = m_knownPeers->upsert(key);
    peer.verified = !peer.verified;
    m_knownPeers->save();
    reload();
}

void P2pPeersPanel::onToggleBlocked()
{
    const QByteArray key = selectedKey();
    if (key.isEmpty() || !m_knownPeers) return;
    P2pKnownPeer& peer = m_knownPeers->upsert(key);
    peer.blocked = !peer.blocked;
    m_knownPeers->save();
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
    m_knownPeers->save();
    reload();
}
