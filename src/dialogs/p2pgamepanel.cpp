/// @file p2pgamepanel.cpp
/// @brief 「対局中」タブの実装

#include "p2pgamepanel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "buttonstyles.h"
#include "p2pgamecoordinator.h"
#include "p2pwindowtext.h"

namespace {

QLabel* plainLabel(QWidget* parent, const QString& text = QString())
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

QLabel* headingLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    QFont bold = label->font();
    bold.setBold(true);
    label->setFont(bold);
    return label;
}

/// 終える操作の1組（見出し・ボタン・説明）の枠
QFrame* actionGroup(QWidget* parent, const QString& title, const QList<QPushButton*>& buttons, const QString& note)
{
    auto* frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(frame);
    layout->addWidget(headingLabel(title, frame));
    auto* row = new QHBoxLayout;
    for (QPushButton* button : buttons) {
        button->setParent(frame);
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    layout->addLayout(row);
    layout->addWidget(plainLabel(frame, note));
    layout->addStretch();
    return frame;
}

} // namespace

P2pGamePanel::P2pGamePanel(P2pGameCoordinator* game, QWidget* parent)
    : QWidget(parent)
    , m_game(game)
{
    auto* layout = new QVBoxLayout(this);

    // 今の段階（手番・再接続中など）は、ほかの文より大きく出す
    m_headline = plainLabel(this);
    m_headline->setObjectName(QStringLiteral("p2pGameHeadline"));
    QFont large = m_headline->font();
    large.setBold(true);
    large.setPointSize(large.pointSize() + 3);
    m_headline->setFont(large);
    // 状態の文は、このタブの配置に直接置く（枠に入れて1段深くすると、選択できるラベルは、配置のやり直しが
    // 1段上へ伝わるたびに描き直される。文は1手ごとに替わる）
    m_status = plainLabel(this);
    m_status->setObjectName(QStringLiteral("p2pGameStatus"));
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_headline);
    layout->addWidget(m_status);
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    layout->addWidget(headingLabel(tr("対局を終える"), this));
    m_resign = new QPushButton(tr("投了"), this);
    m_resign->setObjectName(QStringLiteral("p2pResign"));
    m_offerDraw = new QPushButton(tr("引き分けを申し出る"), this);
    m_offerDraw->setObjectName(QStringLiteral("p2pOfferDraw"));
    m_offerAbort = new QPushButton(tr("中断を申し出る"), this);
    m_offerAbort->setObjectName(QStringLiteral("p2pOfferAbort"));
    m_leave = new QPushButton(tr("対局をやめる（結果未確定）"), this);
    m_leave->setObjectName(QStringLiteral("p2pLeave"));
    m_leave->setStyleSheet(ButtonStyles::dangerStop());
    connect(m_resign, &QPushButton::clicked, this, &P2pGamePanel::resignRequested);
    connect(m_offerDraw, &QPushButton::clicked, this, &P2pGamePanel::offerDrawRequested);
    connect(m_offerAbort, &QPushButton::clicked, this, &P2pGamePanel::offerAbortRequested);
    connect(m_leave, &QPushButton::clicked, this, &P2pGamePanel::leaveGameRequested);
    auto* groups = new QHBoxLayout;
    groups->addWidget(actionGroup(this, tr("負けを認める"), {m_resign}, tr("あなたの負けになります。")), 2);
    groups->addWidget(actionGroup(this, tr("相手と合意して終える"), {m_offerDraw, m_offerAbort},
                                  tr("相手が承諾したときだけ成立します。")),
                      4);
    groups->addWidget(actionGroup(this, tr("一方的にやめる"), {m_leave}, tr("一方的な離脱です。結果は確定しません。")), 3);
    layout->addLayout(groups);
    // 余った高さは、下の空きにだけ渡す（伸びる割合を付けないと、操作の枠も一緒に伸びる）
    layout->addStretch(1);
    refresh();
}

void P2pGamePanel::refresh()
{
    const P2p::Match* match = m_game ? m_game->match() : nullptr;
    const bool playing = m_game && m_game->phase() == P2pGameCoordinator::Phase::Playing && match;
    m_resign->setEnabled(playing && m_game->canResign());
    for (QPushButton* b : {m_offerDraw, m_offerAbort, m_leave}) b->setEnabled(playing && m_game->isInProgress());
    if (!match) {
        m_headline->setText(tr("対局していません。"));
        m_status->setText(tr("「招待」タブで相手を招待するか、受け取った招待で接続すると、ここに対局の状態が出ます。"));
        return;
    }
    const bool engine = m_game->playsWithEngine();
    QString headline;
    QStringList lines;
    lines << tr("相手: 「%1」").arg(m_game->opponentDisplayName());
    if (m_game->opponentKind() == P2p::PlayerKind::Engine) lines << tr("相手の手はエンジンが指します（相手の申告）");
    lines << (match->iAmBlack() ? tr("あなたは先手") : tr("あなたは後手"));
    if (engine) lines << tr("あなたの手はエンジン「%1」が指します").arg(m_game->player().engineName);
    lines << tr("経路: %1").arg(m_game->viaRelay() ? tr("中継") : tr("直接"));
    switch (match->phase()) {
    case P2p::Match::Phase::Opening: headline = tr("開始の手続き中…"); break;
    case P2p::Match::Phase::Playing:
        if (match->hasPendingMove()) headline = tr("着手の確認を待っています…");
        else if (!match->isMyTurn()) headline = tr("相手の手番です");
        else headline = engine ? tr("エンジンが考えています") : tr("あなたの手番です");
        break;
    case P2p::Match::Phase::Reconnecting:
        headline = tr("再接続中（残り %1 秒）").arg(match->reconnectRemainingMs() / 1000);
        break;
    case P2p::Match::Phase::Finishing: headline = tr("結果を確定しています…"); break;
    case P2p::Match::Phase::Finished:
        headline = tr("対局は終わりました");
        lines << P2pWindowText::settlementText(match->settlement(), match->endingError());
        break;
    }
    // 自分の申し出に相手がまだ答えていない間は、知らせを閉じた後もここで分かる
    if (const std::optional<P2p::Cause> offer = match->offerAwaitingAnswer()) {
        lines << (*offer == P2p::Cause::AgreedDraw ? tr("引き分けを申し出ています（相手の返事を待っています）")
                                                   : tr("中断を申し出ています（相手の返事を待っています）"));
    }
    const qint64 disconnectLeft = static_cast<qint64>(P2p::NetworkPolicy::v1().disconnectTotalMs) - match->reconnectTotalMs();
    lines << tr("切断時間の残り: %1 秒").arg(qMax<qint64>(0, disconnectLeft) / 1000);
    if (match->agreedResult()) {
        lines << tr("結果: %1").arg(P2pWindowText::outcomeText(match->agreedResult()->outcome, match->agreedResult()->cause));
    }
    m_headline->setText(headline);
    m_status->setText(lines.join(QLatin1Char('\n')));
}
