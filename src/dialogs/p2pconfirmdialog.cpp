/// @file p2pconfirmdialog.cpp
/// @brief P2P対局の条件確認画面の実装

#include "p2pconfirmdialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "dialogfontscale.h"
#include "dialogutils.h"
#include "networksettings.h"
#include "p2pdisplayname.h"
#include "p2pgamecoordinator.h"
#include "p2pknownpeers.h"
#include "p2pwindowtext.h"

namespace {

QLabel* plainLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

P2pConfirmDialog::P2pConfirmDialog(P2pGameCoordinator* game, P2pKnownPeers* knownPeers, bool hideUnknownNames,
                                   QWidget* parent)
    : QDialog(parent)
    , m_game(game)
    , m_knownPeers(knownPeers)
    , m_hideUnknownNames(hideUnknownNames)
    , m_timer(new QTimer(this))
{
    setWindowTitle(tr("P2P対局の条件の確認"));
    setModal(false);
    auto* layout = new QVBoxLayout(this);
    // 文が長いので、ダイアログを小さくしても読めるようにスクロールさせる
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    auto* opponentBox = new QGroupBox(tr("相手"), content);
    auto* opponentLayout = new QVBoxLayout(opponentBox);
    m_opponent = plainLabel(opponentBox);
    m_identity = plainLabel(opponentBox);
    // プレイヤーID は省略せずに出す（同一性を省略した表示だけで判断させない）
    m_playerId = plainLabel(opponentBox);
    m_playerId->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_warning = plainLabel(opponentBox);
    m_warning->setStyleSheet(QStringLiteral("color: #b71c1c;"));
    opponentLayout->addWidget(m_opponent);
    opponentLayout->addWidget(new QLabel(tr("相手のプレイヤーID"), opponentBox));
    opponentLayout->addWidget(m_playerId);
    opponentLayout->addWidget(m_identity);
    opponentLayout->addWidget(m_warning);
    contentLayout->addWidget(opponentBox);

    auto* conditionBox = new QGroupBox(tr("対局の条件と記録"), content);
    auto* conditionLayout = new QVBoxLayout(conditionBox);
    m_conditions = plainLabel(conditionBox);
    conditionLayout->addWidget(m_conditions);
    contentLayout->addWidget(conditionBox);
    contentLayout->addStretch();

    m_remaining = plainLabel(this);
    layout->addWidget(m_remaining);

    auto* buttons = new QDialogButtonBox(this);
    m_start = buttons->addButton(tr("開始"), QDialogButtonBox::AcceptRole);
    m_cancel = buttons->addButton(tr("取りやめる"), QDialogButtonBox::RejectRole);
    connect(m_start, &QPushButton::clicked, this, &P2pConfirmDialog::onStart);
    connect(m_cancel, &QPushButton::clicked, this, &P2pConfirmDialog::onCancel);
    layout->addWidget(buttons);

    DialogFontScale::Options options;
    options.size = &NetworkSettings::p2pConfirmDialogSize;
    DialogFontScale::install(this, NetworkSettings::p2pConfirmDialogFontSize, options);

    if (m_game) connect(m_game, &P2pGameCoordinator::negotiationChanged, this, &P2pConfirmDialog::refresh);
    connect(m_timer, &QTimer::timeout, this, &P2pConfirmDialog::onTick);
    m_timer->start(500);
    refresh();
}

void P2pConfirmDialog::refresh()
{
    const P2p::Negotiation* n = m_game ? m_game->negotiation() : nullptr;
    if (!n || !n->opponent()) return;
    const P2p::OpponentHello& opponent = *n->opponent();
    const bool verified = n->opponentVerified();
    const P2pKnownPeer* known = m_knownPeers ? m_knownPeers->find(opponent.playerKey) : nullptr;

    // 表示名は相手が自由に書ける文。かぎ括弧で囲む。対局したことのない相手の名前を出さない設定も守る
    const bool hideName = m_hideUnknownNames && !known;
    m_opponent->setText(hideName ? tr("表示名: 名前は非表示") : tr("表示名（自己申告）: 「%1」").arg(opponent.displayName));
    m_playerId->setText(P2pWindowText::playerIdText(opponent.playerKey));

    QStringList identity;
    if (!verified) {
        identity << tr("相手の署名はまだ確かめていません。このIDは相手の申告です。");
    } else {
        identity << tr("相手の開始署名を確かめました。このIDの鍵の持ち主です。");
    }
    if (known) {
        if (!known->localName.isEmpty()) {
            identity << (verified ? tr("手元で付けた名前: %1").arg(known->localName)
                                  : tr("申告されたIDに付けてある名前: %1").arg(known->localName));
        }
        identity << (known->verified ? (verified ? tr("確認済みの相手です（確認コードで照合済み）。")
                                                 : tr("申告されたIDは確認済みとして記録してあります。"))
                                     : tr("確認コードでは照合していません。"));
        identity << tr("この相手との対局: %1局（結果が確定しなかった対局: %2局）").arg(known->games).arg(known->unconfirmed);
    } else {
        identity << tr("初めての相手です。");
    }
    m_identity->setText(identity.join(QLatin1Char('\n')));

    // 名前を付けた相手と同じ表示名で、プレイヤーID が違う
    QStringList warnings;
    if (m_knownPeers) {
        const QList<P2pKnownPeer> sameName = m_knownPeers->sameNameDifferentId(opponent.displayName, opponent.playerKey);
        for (const P2pKnownPeer& other : sameName) {
            warnings << tr("「%1」と名付けた相手と同じ表示名ですが、プレイヤーIDが違います。別人の可能性があります。")
                            .arg(other.localName);
        }
    }
    m_warning->setText(warnings.join(QLatin1Char('\n')));
    m_warning->setVisible(!warnings.isEmpty());

    QStringList conditions;
    if (n->agreement()) {
        conditions << (n->iAmBlack() ? tr("あなたは先手です。") : tr("あなたは後手です。"))
                          + (n->sidesByCoin() ? tr("先後は双方の乱数で決めました。") : tr("先後は希望どおりです。"));
    } else {
        conditions << tr("先後を決めています…");
    }
    conditions << tr("持ち時間: %1").arg(P2pWindowText::timeControlText(n->terms()));
    conditions << P2pWindowText::rulesText();
    conditions << P2pWindowText::recordContentsText();
    m_conditions->setText(conditions.join(QLatin1Char('\n')));

    const bool canStart = n->stage() == P2p::Negotiation::Stage::Confirming && !n->confirmed();
    m_start->setEnabled(canStart);
    m_start->setText(n->confirmed() ? tr("相手を待っています…") : tr("開始"));
}

void P2pConfirmDialog::onTick()
{
    if (!m_game || m_game->phase() != P2pGameCoordinator::Phase::Negotiating) {
        m_timer->stop();
        QDialog::reject();
        return;
    }
    const qint64 seconds = m_game->reservationRemainingMs() / 1000;
    m_remaining->setText(tr("あと %1 秒で取り消されます。").arg(seconds));
}

void P2pConfirmDialog::onStart()
{
    if (m_game) m_game->confirmStart();
    refresh();
}

void P2pConfirmDialog::onCancel()
{
    reject();
}

void P2pConfirmDialog::reject()
{
    if (m_game && m_game->phase() == P2pGameCoordinator::Phase::Negotiating) m_game->cancelNegotiation();
    QDialog::reject();
}
