#include "tsumeplaydialog.h"
#include "applicationfonts.h"
#include "tsumesolutionreplay.h"
#include "tsumeshogikanjibuilder.h"
#include "kifupresentation.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <limits>

void TsumePlayDialog::buildReplayUi()
{
    m_replayUi.row = new QWidget(this);
    m_replayUi.row->setObjectName(QStringLiteral("tsumeReplayControls"));
    m_replayUi.row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(m_replayUi.row);
    row->setContentsMargins(0, 0, 0, 0);
    m_replayUi.first = new QPushButton(tr("開始局面へ"), this);
    m_replayUi.previous = new QPushButton(tr("1手戻る"), this);
    m_replayUi.next = new QPushButton(tr("1手進む"), this);
    m_replayUi.last = new QPushButton(tr("詰み局面へ"), this);
    m_replayUi.first->setObjectName(QStringLiteral("tsumeSolutionFirst"));
    m_replayUi.previous->setObjectName(QStringLiteral("tsumeSolutionPrevious"));
    m_replayUi.next->setObjectName(QStringLiteral("tsumeSolutionNext"));
    m_replayUi.last->setObjectName(QStringLiteral("tsumeSolutionLast"));
    connect(m_replayUi.first, &QPushButton::clicked, this, &TsumePlayDialog::solutionFirst);
    connect(m_replayUi.previous, &QPushButton::clicked, this, &TsumePlayDialog::solutionPrevious);
    connect(m_replayUi.next, &QPushButton::clicked, this, &TsumePlayDialog::solutionNext);
    connect(m_replayUi.last, &QPushButton::clicked, this, &TsumePlayDialog::solutionLast);
    for (auto* button : {m_replayUi.first, m_replayUi.previous, m_replayUi.next, m_replayUi.last}) {
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    row->addStretch();
    m_actionUi.stack->addWidget(m_replayUi.row);
}

void TsumePlayDialog::buildActionControls(QVBoxLayout* layout)
{
    auto* actions = new QHBoxLayout;
    m_actionUi.stack = new QStackedWidget(this);
    m_actionUi.stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_actionUi.game = new QWidget(this);
    m_actionUi.game->setObjectName(QStringLiteral("tsumeGameControls"));
    m_actionUi.game->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* gameRow = new QHBoxLayout(m_actionUi.game);
    gameRow->setContentsMargins(0, 0, 0, 0);
    m_actionUi.restart = new QPushButton(tr("最初から解き直す"), this);
    m_actionUi.undo = new QPushButton(tr("一手戻す"), this);
    m_actionUi.restart->setObjectName(QStringLiteral("tsumeRestart"));
    m_actionUi.undo->setObjectName(QStringLiteral("tsumeUndo"));
    connect(m_actionUi.restart, &QPushButton::clicked, this, &TsumePlayDialog::selectProblem);
    connect(m_actionUi.undo, &QPushButton::clicked, m_session, &TsumeGameSession::undo);
    for (auto* button : {m_actionUi.restart, m_actionUi.undo}) {
        button->setAutoDefault(false);
        gameRow->addWidget(button);
    }
    gameRow->addStretch();
    m_actionUi.stack->addWidget(m_actionUi.game);
    buildReplayUi();
    actions->addWidget(m_actionUi.stack, 1);
    m_actionUi.stop = new QPushButton(tr("探索中止"), this);
    m_actionUi.retry = new QPushButton(tr("再判定"), this);
    m_actionUi.stop->setObjectName(QStringLiteral("tsumeStop"));
    m_actionUi.retry->setObjectName(QStringLiteral("tsumeRetry"));
    connect(m_actionUi.stop, &QPushButton::clicked, this, &TsumePlayDialog::stopSearch);
    connect(m_actionUi.retry, &QPushButton::clicked, this, &TsumePlayDialog::retrySearch);
    for (auto* button : {m_actionUi.stop, m_actionUi.retry}) {
        button->setAutoDefault(false);
        // 表示切替で操作欄の制約が変わると、余った高さが再配分されて盤面が上下する。
        auto policy = button->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        button->setSizePolicy(policy);
        actions->addWidget(button);
    }
    layout->addLayout(actions);
}

void TsumePlayDialog::showSolution(int ply)
{
    if (m_totalPlies <= 0 || m_session->state() == TsumeGameSession::State::Thinking) return;
    if (!m_reviewing) m_savedStatus = m_infoUi.status->text();
    m_reviewing = true;
    cancelPendingOutcome();
    m_solution->seek(ply, m_problemUi.timeout->value() * 1000);
    updateState();
}

void TsumePlayDialog::solutionFirst() { showSolution(0); }
void TsumePlayDialog::solutionPrevious() { showSolution(m_solution->currentPly() - 1); }
void TsumePlayDialog::solutionNext() { showSolution(m_reviewing ? m_solution->currentPly() + 1 : 1); }
void TsumePlayDialog::solutionLast() { showSolution(std::numeric_limits<int>::max()); }

void TsumePlayDialog::returnToGame()
{
    if (!m_reviewing) return;
    m_reviewing = false;
    m_solution->cancel();
    updatePosition(m_session->sfen(), {});
    updateState();
    m_infoUi.status->setText(m_savedStatus);
}

void TsumePlayDialog::stopSearch()
{
    if (m_reviewing) m_solution->cancel();
    else m_session->cancel();
}

void TsumePlayDialog::retrySearch()
{
    if (m_reviewing) showSolution(m_solution->requestedPly());
    else m_session->retry();
}

void TsumePlayDialog::updateReplayControls()
{
    const bool enabled = m_totalPlies > 0 && m_session->state() != TsumeGameSession::State::Thinking
                         && !m_solution->loading();
    const int ply = m_reviewing ? m_solution->currentPly() : 0;
    const bool atEnd = m_reviewing && m_solution->available() && ply == m_solution->totalPlies();
    m_replayUi.showSolution->setEnabled(enabled && !m_reviewing);
    // 非表示ページもサイズ計算に含め、切替前後で操作欄の幅と高さを共有する。
    m_replayUi.modeButton->setCurrentWidget(m_reviewing ? m_replayUi.returnToGame : m_replayUi.showSolution);
    m_infoUi.stack->setCurrentIndex(m_reviewing ? 1 : 0);
    m_actionUi.stack->setCurrentWidget(m_reviewing ? m_replayUi.row : m_actionUi.game);
    m_replayUi.first->setEnabled(enabled && m_reviewing && m_solution->available() && ply > 0);
    m_replayUi.previous->setEnabled(enabled && m_reviewing && m_solution->available() && ply > 0);
    m_replayUi.next->setEnabled(enabled && m_reviewing && m_solution->available() && !atEnd);
    m_replayUi.last->setEnabled(enabled && m_reviewing && m_solution->available() && !atEnd);
    m_replayUi.returnToGame->setEnabled(m_reviewing);
    ApplicationFonts::useJapaneseFont(m_infoUi.solutionText, m_reviewing && m_solution->available()
        && !m_solution->loading() && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese);
    if (!m_reviewing) {
        m_infoUi.solutionText->clear();
        m_infoUi.solutionStatus->setText(tr("正解手順:"));
    } else if (m_solution->loading()) {
        m_infoUi.solutionText->setPlainText(tr("正解手順を確認中…"));
        m_infoUi.solutionStatus->setText(tr("正解手順を確認中…"));
        m_infoUi.status->setText(tr("正解手順を取得しています。探索中止または対局に戻る操作で中断できます。"));
    } else if (m_solution->available()) {
        const QString text = KifuPresentation::pv(m_problem.sfen, m_solution->moves().join(QLatin1Char(' ')),
            TsumeshogiKanjiBuilder::buildKanjiPv(m_problem.sfen, m_solution->moves()));
        if (m_infoUi.solutionText->toPlainText() != text) m_infoUi.solutionText->setPlainText(text);
        m_infoUi.solutionStatus->setText(tr("正解手順: %1 / %2手").arg(ply).arg(m_solution->totalPlies()));
        m_infoUi.status->setText(tr("正解手順の一例を再生中です。「対局に戻る」で元の局面から続けられます。"));
    } else {
        m_infoUi.solutionText->setPlainText(m_solution->detail());
        m_infoUi.solutionStatus->setText(tr("正解手順: 未確認"));
        m_infoUi.status->setText(m_solution->detail());
    }
}
