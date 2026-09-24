#include "tsumeplaydialog.h"
#include "boardinteractioncontroller.h"
#include "dialogutils.h"
#include "shogiboard.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "tsumeshogisettings.h"
#include "usimovecoordinateconverter.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

TsumePlayDialog::TsumePlayDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("tsumePlayDialog"));
    setWindowTitle(tr("詰将棋対局 — Hayanagi"));
    m_outcomeTimer.setSingleShot(true);
    m_outcomeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_outcomeTimer, &QTimer::timeout, this, &TsumePlayDialog::showPendingOutcome);
    m_session = new TsumeGameSession(this);
    m_game = new ShogiGameController(this);
    QString initialSfen = SfenUtils::hirateSfen();
    m_game->newGame(initialSfen);
    buildUi();
    const auto preferences = TsumeshogiSettings::playPreferences();
    DialogUtils::restoreDialogSize(this, preferences.size);
    m_timeout->setValue(std::clamp(preferences.timeoutSec, 1, 600));
    m_view->setSquareSize(std::clamp(preferences.squareSize, 20, 100));
    updateTimeLimit();
    connect(m_session, &TsumeGameSession::positionChanged, this, &TsumePlayDialog::updatePosition);
    connect(m_session, &TsumeGameSession::stateChanged, this, &TsumePlayDialog::updateState);
    connect(m_session, &TsumeGameSession::finished, this, &TsumePlayDialog::showOutcome);
    connect(m_session, &TsumeGameSession::moveRejected, this, &TsumePlayDialog::rejectMove);
    updateState();
    QTimer::singleShot(0, this, &TsumePlayDialog::restoreLastFile);
}

TsumePlayDialog::~TsumePlayDialog()
{
    m_session->cancel();
    TsumeshogiSettings::setPlayPreferences({size(), m_file, m_selector->currentIndex(),
                                          m_timeout->value(), m_view->squareSize()});
    m_interaction->clearAllHighlights();
}

void TsumePlayDialog::done(int result)
{
    cancelPendingOutcome();
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
    auto* files = new QHBoxLayout;
    auto* open = new QPushButton(tr("局面集を開く…"), this);
    open->setObjectName(QStringLiteral("tsumeOpenFile"));
    connect(open, &QPushButton::clicked, this, &TsumePlayDialog::openFile);
    m_fileLabel = new QLabel(tr("局面集を選択してください。"), this);
    m_fileLabel->setTextFormat(Qt::PlainText);
    files->addWidget(open);
    files->addWidget(m_fileLabel, 1);
    layout->addLayout(files);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("問題:"), this));
    m_selector = new QComboBox(this);
    m_selector->setObjectName(QStringLiteral("tsumeProblemSelector"));
    connect(m_selector, &QComboBox::currentIndexChanged, this, &TsumePlayDialog::selectProblem);
    controls->addWidget(m_selector, 1);
    controls->addWidget(new QLabel(tr("判定時間:"), this));
    m_timeout = new QSpinBox(this);
    m_timeout->setRange(1, 600);
    m_timeout->setSuffix(tr(" 秒"));
    connect(m_timeout, &QSpinBox::valueChanged, this, &TsumePlayDialog::updateTimeLimit);
    controls->addWidget(m_timeout);
    layout->addLayout(controls);

    auto* instructions = new QLabel(tr("王手を続け、表示された手数以内に詰ませてください。別解も判定します。"), this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    m_view = new ShogiView(this);
    m_view->setObjectName(QStringLiteral("tsumeBoard"));
    m_view->setBoard(m_game->board());
    m_view->setPieces();
    m_view->setClockEnabled(false);
    m_view->setMouseClickMode(true);
    m_view->setNameFontScale(0.3);
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
    auto* buttons = new QHBoxLayout;
    m_restart = new QPushButton(tr("最初から"), this);
    m_undo = new QPushButton(tr("一手戻す"), this);
    m_stop = new QPushButton(tr("探索中止"), this);
    m_retry = new QPushButton(tr("再判定"), this);
    auto* close = new QPushButton(tr("閉じる"), this);
    connect(m_restart, &QPushButton::clicked, this, &TsumePlayDialog::selectProblem);
    connect(m_undo, &QPushButton::clicked, m_session, &TsumeGameSession::undo);
    connect(m_stop, &QPushButton::clicked, m_session, &TsumeGameSession::cancel);
    connect(m_retry, &QPushButton::clicked, m_session, &TsumeGameSession::retry);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    for (auto* button : {m_restart, m_undo, m_stop, m_retry, close}) buttons->addWidget(button);
    layout->addLayout(buttons);
}

void TsumePlayDialog::restoreLastFile()
{
    if (!m_file.isEmpty()) return;
    const auto preferences = TsumeshogiSettings::playPreferences();
    if (QFileInfo::exists(preferences.lastFile) && loadFile(preferences.lastFile))
        m_selector->setCurrentIndex(std::clamp(preferences.problemIndex, 0, m_selector->count() - 1));
}

void TsumePlayDialog::openFile()
{
    cancelPendingOutcome();
    const auto path = QFileDialog::getOpenFileName(this, tr("詰将棋の局面集を開く"),
        m_file, tr("局面集 (*.txt *.sfen *.usi);;すべてのファイル (*)"));
    if (!path.isEmpty()) loadFile(path);
}

bool TsumePlayDialog::loadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, windowTitle(), tr("ファイルを開けませんでした。\n%1").arg(file.errorString()));
        return false;
    }
    const auto parsed = TsumeCollection::parse(QString::fromUtf8(file.readAll()));
    if (parsed.problems.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("有効な詰将棋局面がありません。SFEN形式と玉方の玉を確認してください。"));
        return false;
    }
    m_session->cancel();
    m_problems = parsed.problems;
    m_file = path;
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_fileLabel->setToolTip(path);
    {
        const QSignalBlocker blocker(m_selector);
        m_selector->clear();
        for (qsizetype i = 0; i < m_problems.size(); ++i) {
            m_selector->addItem(tr("第%1問（%2行目）").arg(i + 1).arg(m_problems[i].lineNumber));
            m_selector->setItemData(static_cast<int>(i), m_problems[i].sfen, Qt::ToolTipRole);
        }
    }
    if (!parsed.invalidLines.isEmpty()) {
        QStringList lines;
        for (int line : parsed.invalidLines) lines.append(QString::number(line));
        QMessageBox::warning(this, windowTitle(), tr("次の行は形式が不正なため読み込めませんでした: %1").arg(lines.join(QStringLiteral(", "))));
    }
    selectProblem();
    return true;
}

void TsumePlayDialog::selectProblem()
{
    const int index = m_selector->currentIndex();
    if (index < 0 || index >= m_problems.size()) return;
    m_interaction->cancelPendingClick();
    m_interaction->clearAllHighlights();
    m_view->endDrag();
    m_session->start(m_problems[index].sfen);
    const bool black = m_session->attackerIsBlack();
    m_view->setFlipMode(!black);
    m_view->setBlackPlayerName(black ? tr("あなた") : QStringLiteral("Hayanagi"));
    m_view->setWhitePlayerName(black ? QStringLiteral("Hayanagi") : tr("あなた"));
}

void TsumePlayDialog::requestMove(const QPoint& from, const QPoint& to)
{
    m_view->endDrag();
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
    m_interaction->setMoveInputEnabled(state == State::Ready);
    m_restart->setEnabled(!m_problems.isEmpty());
    m_undo->setEnabled(m_session->canUndo());
    m_stop->setEnabled(state == State::Thinking);
    m_retry->setEnabled(state == State::Paused);
    if (state == State::Ready) m_status->setText(tr("あなたの手番です。残り%1手以内で詰ませてください。").arg(m_session->remainingPlies()));
    else if (state == State::Thinking) m_status->setText(tr("Hayanagiが判定しています…"));
    else if (state == State::Paused) m_status->setText(tr("判定を中断しました。判定時間を増やして「再判定」するか、一手戻してください。"));
}

void TsumePlayDialog::showOutcome(TsumeGameSession::Outcome outcome, int remaining)
{
    cancelPendingOutcome();
    QString message;
    using Outcome = TsumeGameSession::Outcome;
    switch (outcome) {
    case Outcome::Solved: message = tr("正解です。玉方を詰ませました！"); break;
    case Outcome::NoMate: message = tr("この応手で詰みを防がれました。王手を続けても詰みません。"); break;
    case Outcome::TooLong: message = tr("この応手により、残り%1手以内では詰みません。一手戻して考え直してください。").arg(remaining); break;
    case Outcome::Inconclusive: message = tr("制限時間または探索上限（31手）に達したため判定できませんでした。不正解とは判定していません。"); break;
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
