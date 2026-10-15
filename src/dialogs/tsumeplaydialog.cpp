#include "tsumeplaydialog.h"
#include "appsettings.h"
#include "boardinteractioncontroller.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "shogiboard.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "boardviewerwidget.h"
#include "tsumeshogisettings.h"
#include "tsumeprogressstore.h"
#include "tsumesolutionreplay.h"
#include "boardcoordinates.h"

#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include "applicationfonts.h"
#include "tsumeshogikanjibuilder.h"
#include "kifupresentation.h"
#include <limits>

TsumePlayDialog::TsumePlayDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper(TsumeshogiSettings::tsumePlayFontSize, 8, 24, 1)
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
    m_problemUi.timeout->setValue(std::clamp(preferences.timeoutSec, 1, 600));
    m_view->setSquareSize(std::clamp(preferences.squareSize, 20, 150));
    m_boardRotated = preferences.boardRotated;
    updateTimeLimit();
    connect(m_session, &TsumeGameSession::positionChanged, this, &TsumePlayDialog::onSessionPositionChanged);
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
    preferences.timeoutSec = m_problemUi.timeout->value();
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
    m_problemUi.header = new QLabel(this);
    m_problemUi.header->setObjectName(QStringLiteral("tsumeHeader"));
    m_problemUi.header->setWordWrap(true);
    controls->addWidget(m_problemUi.header, 1);
    m_problemUi.previous = new QPushButton(tr("前の問題"), this);
    m_problemUi.next = new QPushButton(tr("次の問題"), this);
    m_problemUi.previous->setObjectName(QStringLiteral("tsumePreviousProblem"));
    m_problemUi.next->setObjectName(QStringLiteral("tsumeNextProblem"));
    connect(m_problemUi.previous, &QPushButton::clicked, this, &TsumePlayDialog::previousProblemRequested);
    connect(m_problemUi.next, &QPushButton::clicked, this, &TsumePlayDialog::nextProblemRequested);
    for (auto* button : {m_problemUi.previous, m_problemUi.next}) {
        button->setAutoDefault(false);
        button->setEnabled(false);
        controls->addWidget(button);
    }
    controls->addSpacing(12);
    controls->addWidget(new QLabel(tr("判定時間:"), this));
    m_problemUi.timeout = new QSpinBox(this);
    m_problemUi.timeout->setObjectName(QStringLiteral("tsumeTimeLimit"));
    m_problemUi.timeout->setRange(1, 600);
    m_problemUi.timeout->setSuffix(tr(" 秒"));
    connect(m_problemUi.timeout, &QSpinBox::valueChanged, this, &TsumePlayDialog::updateTimeLimit);
    controls->addWidget(m_problemUi.timeout);
    layout->addLayout(controls);
    m_problemUi.history = new QLabel(this);
    m_problemUi.history->setWordWrap(true);
    layout->addWidget(m_problemUi.history);

    auto* instructions = new QLabel(tr("王手を続け、表示された手数以内に詰ませてください。別解も判定します。"), this);
    instructions->setWordWrap(true);
    m_infoUi.stack = new QStackedWidget(this);
    m_infoUi.stack->setObjectName(QStringLiteral("tsumeInformation"));
    m_infoUi.stack->addWidget(instructions);
    auto* solutionPage = new QWidget(this);
    auto* solutionLayout = new QVBoxLayout(solutionPage);
    solutionLayout->setContentsMargins(0, 0, 0, 0);
    solutionLayout->setSpacing(4);
    m_infoUi.solutionStatus = new QLabel(tr("正解手順:"), this);
    m_infoUi.solutionStatus->setObjectName(QStringLiteral("tsumeSolutionStatus"));
    solutionLayout->addWidget(m_infoUi.solutionStatus);
    m_infoUi.solutionText = new QPlainTextEdit(this);
    m_infoUi.solutionText->setObjectName(QStringLiteral("tsumeSolutionText"));
    m_infoUi.solutionText->setAccessibleName(tr("正解手順"));
    m_infoUi.solutionText->setReadOnly(true);
    solutionLayout->addWidget(m_infoUi.solutionText, 1);
    m_infoUi.stack->addWidget(solutionPage);
    layout->addWidget(m_infoUi.stack);
    buildBoardControls(layout);

    m_infoUi.status = new QLabel(this);
    m_infoUi.status->setObjectName(QStringLiteral("tsumeStatus"));
    m_infoUi.status->setWordWrap(true);
    m_infoUi.status->setMinimumHeight(45);
    layout->addWidget(m_infoUi.status);
    buildActionControls(layout);
    auto* buttons = new QHBoxLayout;
    auto* fontDecrease = new QToolButton(this);
    fontDecrease->setObjectName(QStringLiteral("tsumePlayFontDecrease"));
    fontDecrease->setText(QStringLiteral("A-"));
    fontDecrease->setToolTip(tr("文字サイズを縮小"));
    fontDecrease->setStyleSheet(ButtonStyles::fontButton());
    auto* fontIncrease = new QToolButton(this);
    fontIncrease->setObjectName(QStringLiteral("tsumePlayFontIncrease"));
    fontIncrease->setText(QStringLiteral("A+"));
    fontIncrease->setToolTip(tr("文字サイズを拡大"));
    fontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(fontDecrease, &QToolButton::clicked, this, &TsumePlayDialog::onFontDecrease);
    connect(fontIncrease, &QToolButton::clicked, this, &TsumePlayDialog::onFontIncrease);
    buttons->addWidget(fontDecrease);
    buttons->addWidget(fontIncrease);
    buttons->addStretch();
    auto* close = new QPushButton(tr("一覧に戻る"), this);
    close->setObjectName(QStringLiteral("tsumeBackToCollection"));
    close->setAutoDefault(false);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(close);
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
    m_problemUi.timeout->setValue(timeoutSec);
    m_session->configureEngine(enginePath, store);
    selectProblem();
}

void TsumePlayDialog::setProblemNavigation(bool hasPrevious, bool hasNext)
{
    m_problemUi.previous->setEnabled(hasPrevious);
    m_problemUi.next->setEnabled(hasNext);
}

int TsumePlayDialog::timeoutSec() const
{
    return m_problemUi.timeout->value();
}

void TsumePlayDialog::buildBoardControls(QVBoxLayout* layout)
{
    // 盤と、縮小・拡大・回転の操作列（盤のモデルは対局の盤を使う）
    m_viewer = new BoardViewerWidget(BoardViewerWidget::ZoomToolbar | BoardViewerWidget::FitWindowOnZoom,
                                     BoardViewerWidget::ButtonLabels::Symbols, m_game->board(), this);
    m_view = m_viewer->view();
    m_view->setObjectName(QStringLiteral("tsumeBoard"));
    m_view->setMouseClickMode(true);
    m_viewer->reduceButton()->setObjectName(QStringLiteral("tsumeReduceBoard"));
    m_viewer->enlargeButton()->setObjectName(QStringLiteral("tsumeEnlargeBoard"));
    m_viewer->flipButton()->setObjectName(QStringLiteral("tsumeFlipBoard"));
    connect(m_viewer, &BoardViewerWidget::boardTransformAboutToChange, this, &TsumePlayDialog::cancelBoardSelection);
    connect(m_viewer, &BoardViewerWidget::flipToggled, this, &TsumePlayDialog::onFlipToggled);
    m_interaction = new BoardInteractionController(m_view, m_game, this);
    // 「表示」→「指せるマスを表示」の設定に従う（メイン画面と同じ）
    m_interaction->setLegalMovesVisible(AppSettings::legalMovesVisible());
    connect(m_view, &ShogiView::clicked, m_interaction, &BoardInteractionController::onLeftClick);
    connect(m_view, &ShogiView::rightClicked, m_interaction, &BoardInteractionController::onRightClick);
    connect(m_interaction, &BoardInteractionController::moveRequested, this, &TsumePlayDialog::requestMove);

    m_replayUi.modeButton = new QStackedWidget(this);
    m_replayUi.modeButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_replayUi.showSolution = new QPushButton(tr("正解手順"), this);
    m_replayUi.showSolution->setObjectName(QStringLiteral("tsumeShowSolution"));
    m_replayUi.showSolution->setAutoDefault(false);
    connect(m_replayUi.showSolution, &QPushButton::clicked, this, &TsumePlayDialog::solutionFirst);
    m_replayUi.modeButton->addWidget(m_replayUi.showSolution);
    m_replayUi.returnToGame = new QPushButton(tr("対局に戻る"), this);
    m_replayUi.returnToGame->setObjectName(QStringLiteral("tsumeReturnToGame"));
    m_replayUi.returnToGame->setAutoDefault(false);
    connect(m_replayUi.returnToGame, &QPushButton::clicked, this, &TsumePlayDialog::returnToGame);
    m_replayUi.modeButton->addWidget(m_replayUi.returnToGame);
    m_viewer->toolbarLayout()->addWidget(m_replayUi.modeButton);
    layout->addWidget(m_viewer, 1);
}

void TsumePlayDialog::cancelBoardSelection()
{
    m_view->endDrag();
    m_interaction->cancelPendingClick();
    m_interaction->clearSelectionHighlight();
}

void TsumePlayDialog::applyBoardOrientation()
{
    m_viewer->setFlipped(!m_session->attackerIsBlack() != m_boardRotated);
}

void TsumePlayDialog::onFlipToggled(bool flipped)
{
    // 回転ボタンで変えた向きを、攻め方から見た向きとの違いとして覚える
    m_boardRotated = flipped != !m_session->attackerIsBlack();
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
    // 盤上の文字・対局者名は ShogiView がマスの大きさに合わせて調整する。
    const QFontMetrics metrics(m_fontHelper.applyTo(this, {m_view}));
    // 案内文と棋譜は同じ領域で切り替える。棋譜の行数やモードで盤面を動かさない。
    m_infoUi.stack->setFixedHeight(std::max(70, metrics.lineSpacing() * 4 + 8));
    // 大きい文字でも案内の折返しで盤下の領域が伸縮しないように余裕を確保する。
    m_infoUi.status->setMinimumHeight(std::max(45, metrics.lineSpacing() * 3));
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
    m_problemUi.header->setText(tr("第%1問 — 詰み手数を確認しています…").arg(m_number));
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
    const auto parsed = BoardCoordinates::usiMoveFromSquares(from, to, false);
    QString move = parsed ? parsed->toString() : QString();
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
    m_game->setCurrentPlayer(blackTurn ? Turn::Black : Turn::White);
    m_view->setActiveSide(blackTurn);
    if (move.isEmpty()) m_interaction->clearAllHighlights();
    else {
        // 直前の手は、今の手番の相手が指した手
        if (const auto played = UsiMove::parse(move)) {
            const Turn mover = blackTurn ? Turn::White : Turn::Black;
            m_interaction->showMoveHighlights(BoardCoordinates::fromSquare(*played, mover),
                                              BoardCoordinates::toSquare(*played));
        }
    }
    // 局面・手番・ハイライトは、盤がそれぞれ変わった所だけを描き直すので、全体の描き直しは頼まない
}

void TsumePlayDialog::onSessionPositionChanged(const QString& sfen, const QString& move)
{
    updatePosition(sfen, move);
    // 指し手のない通知は開始・待ったなどの局面の置き換えなので鳴らさない
    if (!move.isEmpty()) emit movePlayed();
}

void TsumePlayDialog::updateState()
{
    cancelPendingOutcome();
    using State = TsumeGameSession::State;
    const auto state = m_session->state();
    m_interaction->setMoveInputEnabled(!m_reviewing && state == State::Ready);
    m_actionUi.restart->setEnabled(!m_reviewing && !m_problem.sfen.isEmpty());
    m_actionUi.undo->setEnabled(!m_reviewing && m_session->canUndo());
    m_actionUi.stop->setEnabled(m_reviewing ? m_solution->loading() : state == State::Thinking);
    m_actionUi.retry->setEnabled(m_reviewing ? !m_solution->loading() && !m_solution->available() : state == State::Paused);
    m_actionUi.stop->setVisible(m_actionUi.stop->isEnabled());
    m_actionUi.retry->setVisible(m_actionUi.retry->isEnabled());
    if (!m_reviewing && state == State::Ready) {
        if (!m_totalPlies) m_totalPlies = m_session->remainingPlies();
        m_problemUi.header->setText(tr("第%1問 — %2手詰 ／ 玉方: Hayanagi").arg(m_number).arg(m_totalPlies));
        if (!m_attemptRecorded) {
            m_attemptRecorded = true;
            if (m_store) m_store->recordAttempt(TsumeCollection::positionId(m_problem.sfen));
        }
        m_infoUi.status->setText(tr("あなたの手番です。残り%1手以内で詰ませてください。").arg(m_session->remainingPlies()));
    }
    else if (state == State::Thinking) m_infoUi.status->setText(tr("詰みと玉方の応手を確認しています…"));
    else if (state == State::Paused) m_infoUi.status->setText(tr("判定を中断しました。判定時間を増やして「再判定」するか、一手戻してください。"));
    if (m_store && !m_problem.sfen.isEmpty()) {
        const auto progress = m_store->progress(TsumeCollection::positionId(m_problem.sfen));
        m_problemUi.history->setText(progress.solves > 0 ? tr("正答済み ／ 挑戦%1回・正答%2回").arg(progress.attempts).arg(progress.solves)
                                             : tr("未正答 ／ 挑戦%1回").arg(progress.attempts));
        if (!m_store->error().isEmpty()) m_problemUi.history->setText(tr("履歴を保存できません: %1").arg(m_store->error()));
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
    m_infoUi.status->setText(message);
    if (outcome == Outcome::NoMate || outcome == Outcome::TooLong) {
        // 玉方の応手を描いた盤面を1秒間確認できるようにしてから通知する（応手の描き直しは
        // 予約済みで、タイマーが鳴る前にイベントループが描くので、ここで同期描画はしない）。
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
    m_infoUi.status->setText(tr("王手になる合法手を指してください。二歩・打ち歩詰め・自玉の王手放置はできません。"));
}

void TsumePlayDialog::updateTimeLimit()
{
    m_session->setTimeLimit(m_problemUi.timeout->value() * 1000);
}

// ============================================================
// 正解手順の再生
// ============================================================

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
