#include "tsumeplaydialog.h"
#include "boardinteractioncontroller.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "shogiboard.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "tsumeshogisettings.h"
#include "tsumeprogressstore.h"
#include "tsumesolutionreplay.h"
#include "usimovecoordinateconverter.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>

TsumePlayDialog::TsumePlayDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper({TsumeshogiSettings::tsumePlayFontSize(), 8, 24, 1,
                    TsumeshogiSettings::setTsumePlayFontSize})
{
    setObjectName(QStringLiteral("tsumePlayDialog"));
    setWindowTitle(tr("詰将棋対局"));
    m_outcomeTimer.setSingleShot(true);
    m_outcomeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_outcomeTimer, &QTimer::timeout, this, &TsumePlayDialog::showPendingOutcome);
    m_session = new TsumeGameSession(this);
    m_solution = new TsumeSolutionReplay(this);
    m_game = new ShogiGameController(this);
    QString initialSfen = SfenUtils::hirateSfen();
    m_game->newGame(initialSfen);
    buildUi();
    applyFontSize();
    const auto preferences = TsumeshogiSettings::playPreferences();
    DialogUtils::restoreDialogSize(this, preferences.size);
    m_timeout->setValue(std::clamp(preferences.timeoutSec, 1, 600));
    m_view->setSquareSize(std::clamp(preferences.squareSize, 20, 150));
    m_boardRotated = preferences.boardRotated;
    updateTimeLimit();
    connect(m_session, &TsumeGameSession::positionChanged, this, &TsumePlayDialog::updatePosition);
    connect(m_session, &TsumeGameSession::stateChanged, this, &TsumePlayDialog::updateState);
    connect(m_session, &TsumeGameSession::finished, this, &TsumePlayDialog::showOutcome);
    connect(m_session, &TsumeGameSession::moveRejected, this, &TsumePlayDialog::rejectMove);
    connect(m_solution, &TsumeSolutionReplay::positionChanged, this, &TsumePlayDialog::updatePosition);
    connect(m_solution, &TsumeSolutionReplay::stateChanged, this, &TsumePlayDialog::updateState);
    updateState();
}

TsumePlayDialog::~TsumePlayDialog()
{
    m_solution->cancel();
    m_session->cancel();
    auto preferences = TsumeshogiSettings::playPreferences();
    preferences.size = size();
    preferences.timeoutSec = m_timeout->value();
    preferences.squareSize = m_view->squareSize();
    preferences.boardRotated = m_boardRotated;
    TsumeshogiSettings::setPlayPreferences(preferences);
    m_interaction->clearAllHighlights();
}

void TsumePlayDialog::done(int result)
{
    cancelPendingOutcome();
    m_solution->cancel();
    m_session->cancel();
    QDialog::done(result);
}

void TsumePlayDialog::cancelPendingOutcome()
{
    m_outcomeTimer.stop();
    m_pendingOutcome.clear();
}

void TsumePlayDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout;
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("tsumeHeader"));
    m_header->setWordWrap(true);
    controls->addWidget(m_header, 1);
    m_previousProblem = new QPushButton(tr("前の問題"), this);
    m_nextProblem = new QPushButton(tr("次の問題"), this);
    m_previousProblem->setObjectName(QStringLiteral("tsumePreviousProblem"));
    m_nextProblem->setObjectName(QStringLiteral("tsumeNextProblem"));
    connect(m_previousProblem, &QPushButton::clicked, this, &TsumePlayDialog::previousProblemRequested);
    connect(m_nextProblem, &QPushButton::clicked, this, &TsumePlayDialog::nextProblemRequested);
    for (auto* button : {m_previousProblem, m_nextProblem}) {
        button->setAutoDefault(false);
        button->setEnabled(false);
        controls->addWidget(button);
    }
    controls->addSpacing(12);
    controls->addWidget(new QLabel(tr("判定時間:"), this));
    m_timeout = new QSpinBox(this);
    m_timeout->setObjectName(QStringLiteral("tsumeTimeLimit"));
    m_timeout->setRange(1, 600);
    m_timeout->setSuffix(tr(" 秒"));
    connect(m_timeout, &QSpinBox::valueChanged, this, &TsumePlayDialog::updateTimeLimit);
    controls->addWidget(m_timeout);
    layout->addLayout(controls);
    m_history = new QLabel(this);
    m_history->setWordWrap(true);
    layout->addWidget(m_history);

    auto* instructions = new QLabel(tr("王手を続け、表示された手数以内に詰ませてください。別解も判定します。"), this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    m_solutionText = new QPlainTextEdit(this);
    m_solutionText->setObjectName(QStringLiteral("tsumeSolutionText"));
    m_solutionText->setAccessibleName(tr("正解手順"));
    m_solutionText->setReadOnly(true);
    m_solutionText->setMinimumHeight(70);
    m_solutionText->setMaximumHeight(110);
    m_solutionText->hide();
    layout->addWidget(m_solutionText);
    buildBoardControls(layout);
    m_view = new ShogiView(this);
    m_view->setObjectName(QStringLiteral("tsumeBoard"));
    m_view->setBoard(m_game->board());
    m_view->setPieces();
    m_view->setClockEnabled(false);
    m_view->setMouseClickMode(true);
    m_view->setNameFontScale(0.3);
    m_view->installEventFilter(this);
    m_interaction = new BoardInteractionController(m_view, m_game, this);
    connect(m_view, &ShogiView::clicked, m_interaction, &BoardInteractionController::onLeftClick);
    connect(m_view, &ShogiView::rightClicked, m_interaction, &BoardInteractionController::onRightClick);
    connect(m_interaction, &BoardInteractionController::moveRequested, this, &TsumePlayDialog::requestMove);
    layout->addWidget(m_view, 1, Qt::AlignHCenter);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("tsumeStatus"));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(45);
    layout->addWidget(m_status);
    buildReplayUi(layout);
    auto* buttons = new QHBoxLayout;
    m_fontDecrease = new QToolButton(this);
    m_fontDecrease->setObjectName(QStringLiteral("tsumePlayFontDecrease"));
    m_fontDecrease->setText(QStringLiteral("A-"));
    m_fontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_fontDecrease->setStyleSheet(ButtonStyles::fontButton());
    m_fontIncrease = new QToolButton(this);
    m_fontIncrease->setObjectName(QStringLiteral("tsumePlayFontIncrease"));
    m_fontIncrease->setText(QStringLiteral("A+"));
    m_fontIncrease->setToolTip(tr("文字サイズを拡大"));
    m_fontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_fontDecrease, &QToolButton::clicked, this, &TsumePlayDialog::onFontDecrease);
    connect(m_fontIncrease, &QToolButton::clicked, this, &TsumePlayDialog::onFontIncrease);
    buttons->addWidget(m_fontDecrease);
    buttons->addWidget(m_fontIncrease);
    m_restart = new QPushButton(tr("最初から"), this);
    m_undo = new QPushButton(tr("一手戻す"), this);
    m_stop = new QPushButton(tr("探索中止"), this);
    m_retry = new QPushButton(tr("再判定"), this);
    m_restart->setObjectName(QStringLiteral("tsumeRestart"));
    m_undo->setObjectName(QStringLiteral("tsumeUndo"));
    m_stop->setObjectName(QStringLiteral("tsumeStop"));
    m_retry->setObjectName(QStringLiteral("tsumeRetry"));
    auto* close = new QPushButton(tr("一覧に戻る"), this);
    close->setObjectName(QStringLiteral("tsumeBackToCollection"));
    connect(m_restart, &QPushButton::clicked, this, &TsumePlayDialog::selectProblem);
    connect(m_undo, &QPushButton::clicked, m_session, &TsumeGameSession::undo);
    connect(m_stop, &QPushButton::clicked, this, &TsumePlayDialog::stopSearch);
    connect(m_retry, &QPushButton::clicked, this, &TsumePlayDialog::retrySearch);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    for (auto* button : {m_restart, m_undo, m_stop, m_retry, close}) buttons->addWidget(button);
    layout->addLayout(buttons);
}

void TsumePlayDialog::setProblem(const TsumeProblem& problem, int number, const QString& enginePath,
                                TsumeProgressStore* store, int timeoutSec)
{
    m_reviewing = false;
    m_solution->configure(problem.sfen, enginePath, store);
    m_session->cancel();
    m_problem = problem;
    m_number = number;
    m_store = store;
    m_timeout->setValue(timeoutSec);
    m_session->configureEngine(enginePath, store);
    selectProblem();
}

void TsumePlayDialog::setProblemNavigation(bool hasPrevious, bool hasNext)
{
    m_previousProblem->setEnabled(hasPrevious);
    m_nextProblem->setEnabled(hasNext);
}

int TsumePlayDialog::timeoutSec() const
{
    return m_timeout->value();
}

void TsumePlayDialog::buildBoardControls(QVBoxLayout* layout)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* reduce = new QPushButton(QStringLiteral("➖"), this);
    reduce->setObjectName(QStringLiteral("tsumeReduceBoard"));
    reduce->setToolTip(tr("将棋盤を縮小する"));
    reduce->setAccessibleName(reduce->toolTip());
    auto* enlarge = new QPushButton(QStringLiteral("➕"), this);
    enlarge->setObjectName(QStringLiteral("tsumeEnlargeBoard"));
    enlarge->setToolTip(tr("将棋盤を拡大する"));
    enlarge->setAccessibleName(enlarge->toolTip());
    auto* flip = new QPushButton(tr("盤面の回転"), this);
    flip->setObjectName(QStringLiteral("tsumeFlipBoard"));
    for (auto* button : {reduce, enlarge, flip}) {
        button->setStyleSheet(ButtonStyles::secondaryNeutral());
        button->setAutoDefault(false);
        row->addWidget(button);
    }
    connect(reduce, &QPushButton::clicked, this, &TsumePlayDialog::onReduceBoard);
    connect(enlarge, &QPushButton::clicked, this, &TsumePlayDialog::onEnlargeBoard);
    connect(flip, &QPushButton::clicked, this, &TsumePlayDialog::onFlipBoard);
    row->addStretch();
    m_showSolution = new QPushButton(tr("正解手順"), this);
    m_showSolution->setObjectName(QStringLiteral("tsumeShowSolution"));
    m_showSolution->setAutoDefault(false);
    connect(m_showSolution, &QPushButton::clicked, this, &TsumePlayDialog::solutionFirst);
    row->addWidget(m_showSolution);
    layout->addLayout(row);
}

void TsumePlayDialog::cancelBoardSelection()
{
    m_view->endDrag();
    m_interaction->cancelPendingClick();
    m_interaction->clearSelectionHighlight();
}

void TsumePlayDialog::onEnlargeBoard()
{
    cancelBoardSelection();
    m_view->enlargeBoard(false);
    adjustSize();
}

void TsumePlayDialog::onReduceBoard()
{
    cancelBoardSelection();
    m_view->reduceBoard(false);
    adjustSize();
}

void TsumePlayDialog::applyBoardOrientation()
{
    const bool flipped = !m_session->attackerIsBlack() != m_boardRotated;
    m_view->setFlipMode(flipped);
    if (flipped) m_view->setPiecesFlip();
    else m_view->setPieces();
}

void TsumePlayDialog::onFlipBoard()
{
    cancelBoardSelection();
    m_boardRotated = !m_boardRotated;
    applyBoardOrientation();
}

bool TsumePlayDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_view && event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            if (wheel->angleDelta().y() > 0) onEnlargeBoard();
            else if (wheel->angleDelta().y() < 0) onReduceBoard();
            wheel->accept();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void TsumePlayDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void TsumePlayDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void TsumePlayDialog::applyFontSize()
{
    QFont f = font();
    f.setPointSize(m_fontHelper.fontSize());
    setFont(f);
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        // 盤上の文字・対局者名は ShogiView がマスの大きさに合わせて調整する。
        if (widget != m_view && !m_view->isAncestorOf(widget)) widget->setFont(f);
    }
    m_fontDecrease->setEnabled(m_fontHelper.fontSize() > 8);
    m_fontIncrease->setEnabled(m_fontHelper.fontSize() < 24);
}

void TsumePlayDialog::selectProblem()
{
    if (m_problem.sfen.isEmpty()) return;
    cancelPendingOutcome();
    m_reviewing = false;
    m_solution->cancel();
    m_attemptRecorded = false;
    m_solvedRecorded = false;
    m_totalPlies = 0;
    m_header->setText(tr("第%1問 — 詰み手数を確認しています…").arg(m_number));
    m_interaction->cancelPendingClick();
    m_interaction->clearAllHighlights();
    m_view->endDrag();
    m_session->start(m_problem.sfen);
    const bool black = m_session->attackerIsBlack();
    applyBoardOrientation();
    m_view->setBlackPlayerName(black ? tr("あなた") : QStringLiteral("Hayanagi"));
    m_view->setWhitePlayerName(black ? QStringLiteral("Hayanagi") : tr("あなた"));
}

void TsumePlayDialog::requestMove(const QPoint& from, const QPoint& to)
{
    if (m_reviewing) {
        m_view->endDrag();
        return;
    }
    // 通常対局と同様、成りの選択が終わるまでは移動先のドラッグ表示を保つ。
    QString error;
    QString move = UsiMoveCoordinateConverter::convertHumanMoveToUsi(from, to, false, error);
    const auto legal = m_session->legalMoves();
    const QString promoted = move + QLatin1Char('+');
    if (!move.isEmpty() && legal.contains(promoted)) {
        if (!legal.contains(move)) move = promoted;
        else {
            QMessageBox box(QMessageBox::Question, tr("成りの選択"), tr("成りますか？"), QMessageBox::NoButton, this);
            auto* promote = box.addButton(tr("成る"), QMessageBox::AcceptRole);
            auto* plain = box.addButton(tr("成らない"), QMessageBox::RejectRole);
            box.exec();
            if (box.clickedButton() == promote) move = promoted;
            else if (box.clickedButton() != plain) {
                m_interaction->onMoveApplied(from, to, false);
                return;
            }
        }
    }
    m_view->endDrag();
    const bool accepted = m_session->play(move);
    m_interaction->onMoveApplied(from, to, accepted);
}

void TsumePlayDialog::updatePosition(const QString& sfen, const QString& move)
{
    cancelPendingOutcome();
    m_view->endDrag();
    m_interaction->cancelPendingClick();
    m_game->board()->setSfen(sfen);
    const bool blackTurn = sfen.section(QLatin1Char(' '), 1, 1) == QStringLiteral("b");
    m_game->setCurrentPlayer(blackTurn ? ShogiGameController::Player1 : ShogiGameController::Player2);
    m_view->setActiveSide(blackTurn);
    if (move.isEmpty()) m_interaction->clearAllHighlights();
    else {
        const auto from = UsiMoveCoordinateConverter::parseMoveFrom(move, !blackTurn);
        const auto to = UsiMoveCoordinateConverter::parseMoveTo(move);
        if (from && to) m_interaction->showMoveHighlights({from->file, from->rank}, {to->file, to->rank});
    }
    m_view->update();
}

void TsumePlayDialog::updateState()
{
    cancelPendingOutcome();
    using State = TsumeGameSession::State;
    const auto state = m_session->state();
    m_interaction->setMoveInputEnabled(!m_reviewing && state == State::Ready);
    m_restart->setEnabled(!m_problem.sfen.isEmpty());
    m_undo->setEnabled(!m_reviewing && m_session->canUndo());
    m_stop->setEnabled(m_reviewing ? m_solution->loading() : state == State::Thinking);
    m_retry->setEnabled(m_reviewing ? !m_solution->loading() && !m_solution->available() : state == State::Paused);
    if (!m_reviewing && state == State::Ready) {
        if (!m_totalPlies) m_totalPlies = m_session->remainingPlies();
        m_header->setText(tr("第%1問 — %2手詰 ／ 玉方: Hayanagi").arg(m_number).arg(m_totalPlies));
        if (!m_attemptRecorded) {
            m_attemptRecorded = true;
            if (m_store) m_store->recordAttempt(TsumeCollection::positionId(m_problem.sfen));
        }
        m_status->setText(tr("あなたの手番です。残り%1手以内で詰ませてください。").arg(m_session->remainingPlies()));
    }
    else if (state == State::Thinking) m_status->setText(tr("詰みと玉方の応手を確認しています…"));
    else if (state == State::Paused) m_status->setText(tr("判定を中断しました。判定時間を増やして「再判定」するか、一手戻してください。"));
    if (m_store && !m_problem.sfen.isEmpty()) {
        const auto progress = m_store->progress(TsumeCollection::positionId(m_problem.sfen));
        m_history->setText(progress.solves > 0 ? tr("正答済み ／ 挑戦%1回・正答%2回").arg(progress.attempts).arg(progress.solves)
                                             : tr("未正答 ／ 挑戦%1回").arg(progress.attempts));
        if (!m_store->error().isEmpty()) m_history->setText(tr("履歴を保存できません: %1").arg(m_store->error()));
    }
    updateReplayControls();
}

void TsumePlayDialog::showOutcome(TsumeGameSession::Outcome outcome, int remaining)
{
    cancelPendingOutcome();
    QString message;
    using Outcome = TsumeGameSession::Outcome;
    switch (outcome) {
    case Outcome::Solved:
        message = tr("正解です。玉方を詰ませました！");
        if (!m_solvedRecorded) {
            m_solvedRecorded = true;
            if (m_store) m_store->recordSolved(TsumeCollection::positionId(m_problem.sfen));
        }
        updateState();
        break;
    case Outcome::NoMate: message = tr("この応手で詰みを防がれました。王手を続けても詰みません。"); break;
    case Outcome::TooLong: message = tr("この応手により、残り%1手以内では詰みません。一手戻して考え直してください。").arg(remaining); break;
    case Outcome::Inconclusive:
        message = tr("判定が完了していません。不正解とは判定していません。時間を増やして再判定してください。");
        if (!m_session->detail().isEmpty()) message += QLatin1Char('\n') + m_session->detail();
        break;
    case Outcome::InvalidProblem: message = tr("この局面は王手の連続で詰ませられません。別の問題を選んでください。"); break;
    }
    m_status->setText(message);
    if (outcome == Outcome::NoMate || outcome == Outcome::TooLong) {
        // 玉方の応手を描画し、盤面を1秒間確認できるようにしてから通知する。
        m_view->repaint();
        m_pendingOutcome = message;
        m_outcomeTimer.start(1000);
        return;
    }
    QMessageBox::information(this, windowTitle(), message);
}

void TsumePlayDialog::showPendingOutcome()
{
    const QString message = m_pendingOutcome;
    m_pendingOutcome.clear();
    if (message.isEmpty() || !isVisible()) return;
    QMessageBox::information(this, windowTitle(), message);
}

void TsumePlayDialog::rejectMove()
{
    m_status->setText(tr("王手になる合法手を指してください。二歩・打ち歩詰め・自玉の王手放置はできません。"));
}

void TsumePlayDialog::updateTimeLimit()
{
    m_session->setTimeLimit(m_timeout->value() * 1000);
}
