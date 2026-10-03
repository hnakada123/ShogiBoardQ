#include "tsumeplaydialog.h"
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
    m_replayControls = new QWidget(this);
    m_replayControls->setObjectName(QStringLiteral("tsumeReplayControls"));
    m_replayControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(m_replayControls);
    row->setContentsMargins(0, 0, 0, 0);
    m_solutionFirst = new QPushButton(tr("開始局面へ"), this);
    m_solutionPrevious = new QPushButton(tr("1手戻る"), this);
    m_solutionNext = new QPushButton(tr("1手進む"), this);
    m_solutionLast = new QPushButton(tr("詰み局面へ"), this);
    m_solutionFirst->setObjectName(QStringLiteral("tsumeSolutionFirst"));
    m_solutionPrevious->setObjectName(QStringLiteral("tsumeSolutionPrevious"));
    m_solutionNext->setObjectName(QStringLiteral("tsumeSolutionNext"));
    m_solutionLast->setObjectName(QStringLiteral("tsumeSolutionLast"));
    connect(m_solutionFirst, &QPushButton::clicked, this, &TsumePlayDialog::solutionFirst);
    connect(m_solutionPrevious, &QPushButton::clicked, this, &TsumePlayDialog::solutionPrevious);
    connect(m_solutionNext, &QPushButton::clicked, this, &TsumePlayDialog::solutionNext);
    connect(m_solutionLast, &QPushButton::clicked, this, &TsumePlayDialog::solutionLast);
    for (auto* button : {m_solutionFirst, m_solutionPrevious, m_solutionNext, m_solutionLast}) {
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    row->addStretch();
    m_actionControls->addWidget(m_replayControls);
}

void TsumePlayDialog::buildActionControls(QVBoxLayout* layout)
{
    auto* actions = new QHBoxLayout;
    m_actionControls = new QStackedWidget(this);
    m_actionControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_gameControls = new QWidget(this);
    m_gameControls->setObjectName(QStringLiteral("tsumeGameControls"));
    m_gameControls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* gameRow = new QHBoxLayout(m_gameControls);
    gameRow->setContentsMargins(0, 0, 0, 0);
    m_restart = new QPushButton(tr("最初から解き直す"), this);
    m_undo = new QPushButton(tr("一手戻す"), this);
    m_restart->setObjectName(QStringLiteral("tsumeRestart"));
    m_undo->setObjectName(QStringLiteral("tsumeUndo"));
    connect(m_restart, &QPushButton::clicked, this, &TsumePlayDialog::selectProblem);
    connect(m_undo, &QPushButton::clicked, m_session, &TsumeGameSession::undo);
    for (auto* button : {m_restart, m_undo}) {
        button->setAutoDefault(false);
        gameRow->addWidget(button);
    }
    gameRow->addStretch();
    m_actionControls->addWidget(m_gameControls);
    buildReplayUi();
    actions->addWidget(m_actionControls, 1);
    m_stop = new QPushButton(tr("探索中止"), this);
    m_retry = new QPushButton(tr("再判定"), this);
    m_stop->setObjectName(QStringLiteral("tsumeStop"));
    m_retry->setObjectName(QStringLiteral("tsumeRetry"));
    connect(m_stop, &QPushButton::clicked, this, &TsumePlayDialog::stopSearch);
    connect(m_retry, &QPushButton::clicked, this, &TsumePlayDialog::retrySearch);
    for (auto* button : {m_stop, m_retry}) {
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
    if (!m_reviewing) m_savedStatus = m_status->text();
    m_reviewing = true;
    cancelPendingOutcome();
    m_solution->seek(ply, m_timeout->value() * 1000);
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
    m_status->setText(m_savedStatus);
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
    m_showSolution->setEnabled(enabled && !m_reviewing);
    // 非表示ページもサイズ計算に含め、切替前後で操作欄の幅と高さを共有する。
    m_modeButton->setCurrentWidget(m_reviewing ? m_returnToGame : m_showSolution);
    m_information->setCurrentIndex(m_reviewing ? 1 : 0);
    m_actionControls->setCurrentWidget(m_reviewing ? m_replayControls : m_gameControls);
    m_solutionFirst->setEnabled(enabled && m_reviewing && m_solution->available() && ply > 0);
    m_solutionPrevious->setEnabled(enabled && m_reviewing && m_solution->available() && ply > 0);
    m_solutionNext->setEnabled(enabled && m_reviewing && m_solution->available() && !atEnd);
    m_solutionLast->setEnabled(enabled && m_reviewing && m_solution->available() && !atEnd);
    m_returnToGame->setEnabled(m_reviewing);
    if (!m_reviewing) {
        m_solutionText->clear();
        m_solutionStatus->setText(tr("正解手順:"));
    } else if (m_solution->loading()) {
        m_solutionText->setPlainText(tr("正解手順を確認中…"));
        m_solutionStatus->setText(tr("正解手順を確認中…"));
        m_status->setText(tr("正解手順を取得しています。探索中止または対局に戻る操作で中断できます。"));
    } else if (m_solution->available()) {
        const QString text = KifuPresentation::pv(m_problem.sfen, m_solution->moves().join(QLatin1Char(' ')),
            TsumeshogiKanjiBuilder::buildKanjiPv(m_problem.sfen, m_solution->moves()));
        if (m_solutionText->toPlainText() != text) m_solutionText->setPlainText(text);
        m_solutionStatus->setText(tr("正解手順: %1 / %2手").arg(ply).arg(m_solution->totalPlies()));
        m_status->setText(tr("正解手順の一例を再生中です。「対局に戻る」で元の局面から続けられます。"));
    } else {
        m_solutionText->setPlainText(m_solution->detail());
        m_solutionStatus->setText(tr("正解手順: 未確認"));
        m_status->setText(m_solution->detail());
    }
}
