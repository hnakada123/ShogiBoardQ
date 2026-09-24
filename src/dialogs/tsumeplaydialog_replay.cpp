#include "tsumeplaydialog.h"
#include "tsumesolutionreplay.h"
#include "tsumeshogikanjibuilder.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <limits>

void TsumePlayDialog::buildReplayUi(QVBoxLayout* layout)
{
    m_replayControls = new QWidget(this);
    m_replayControls->setObjectName(QStringLiteral("tsumeReplayControls"));
    auto* row = new QHBoxLayout(m_replayControls);
    row->setContentsMargins(0, 0, 0, 0);
    m_solutionStatus = new QLabel(tr("正解手順:"), this);
    m_solutionStatus->setObjectName(QStringLiteral("tsumeSolutionStatus"));
    m_solutionStatus->setWordWrap(true);
    row->addWidget(m_solutionStatus, 1);
    m_solutionFirst = new QPushButton(tr("最初に戻る"), this);
    m_solutionPrevious = new QPushButton(tr("1手戻る"), this);
    m_solutionNext = new QPushButton(tr("1手進む"), this);
    m_solutionLast = new QPushButton(tr("詰み局面へ"), this);
    m_returnToGame = new QPushButton(tr("対局に戻る"), this);
    m_solutionFirst->setObjectName(QStringLiteral("tsumeSolutionFirst"));
    m_solutionPrevious->setObjectName(QStringLiteral("tsumeSolutionPrevious"));
    m_solutionNext->setObjectName(QStringLiteral("tsumeSolutionNext"));
    m_solutionLast->setObjectName(QStringLiteral("tsumeSolutionLast"));
    m_returnToGame->setObjectName(QStringLiteral("tsumeReturnToGame"));
    connect(m_solutionFirst, &QPushButton::clicked, this, &TsumePlayDialog::solutionFirst);
    connect(m_solutionPrevious, &QPushButton::clicked, this, &TsumePlayDialog::solutionPrevious);
    connect(m_solutionNext, &QPushButton::clicked, this, &TsumePlayDialog::solutionNext);
    connect(m_solutionLast, &QPushButton::clicked, this, &TsumePlayDialog::solutionLast);
    connect(m_returnToGame, &QPushButton::clicked, this, &TsumePlayDialog::returnToGame);
    for (auto* button : {m_solutionFirst, m_solutionPrevious, m_solutionNext, m_solutionLast, m_returnToGame}) {
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    layout->addWidget(m_replayControls);
    m_replayControls->hide();
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
    m_solutionText->setVisible(m_reviewing);
    m_replayControls->setVisible(m_reviewing);
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
        const QString text = TsumeshogiKanjiBuilder::buildKanjiPv(m_problem.sfen, m_solution->moves());
        if (m_solutionText->toPlainText() != text) m_solutionText->setPlainText(text);
        m_solutionStatus->setText(tr("正解手順: %1 / %2手").arg(ply).arg(m_solution->totalPlies()));
        m_status->setText(tr("正解手順の一例を再生中です。「対局に戻る」で元の局面から続けられます。"));
    } else {
        m_solutionText->setPlainText(m_solution->detail());
        m_solutionStatus->setText(tr("正解手順: 未確認"));
        m_status->setText(m_solution->detail());
    }
}
