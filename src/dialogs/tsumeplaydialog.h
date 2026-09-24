#ifndef TSUMEPLAYDIALOG_H
#define TSUMEPLAYDIALOG_H

#include <QDialog>
#include <QTimer>
#include "tsumecollection.h"
#include "tsumegamesession.h"
#include "fontsizehelper.h"

class ShogiGameController;
class ShogiView;
class BoardInteractionController;
class TsumeProgressStore;
class TsumeSolutionReplay;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QToolButton;
class QPlainTextEdit;

/// 一覧で選んだ1問を、既存の ShogiView で解く画面。
class TsumePlayDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TsumePlayDialog(QWidget* parent = nullptr);
    ~TsumePlayDialog() override;
    void setProblem(const TsumeProblem& problem, int number, const QString& enginePath,
                    TsumeProgressStore* store, int timeoutSec);

protected:
    void done(int result) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void selectProblem();
    void requestMove(const QPoint& from, const QPoint& to);
    void updatePosition(const QString& sfen, const QString& move);
    void updateState();
    void showOutcome(TsumeGameSession::Outcome outcome, int remaining);
    void showPendingOutcome();
    void rejectMove();
    void updateTimeLimit();
    void solutionFirst();
    void solutionPrevious();
    void solutionNext();
    void solutionLast();
    void returnToGame();
    void stopSearch();
    void retrySearch();
    void onFontIncrease();
    void onFontDecrease();
    void onEnlargeBoard();
    void onReduceBoard();
    void onFlipBoard();

private:
    void buildUi();
    void buildReplayUi(QVBoxLayout* layout);
    void buildBoardControls(QVBoxLayout* layout);
    void applyBoardOrientation();
    void cancelBoardSelection();
    void showSolution(int ply);
    void updateReplayControls();
    void cancelPendingOutcome();
    void applyFontSize();
    FontSizeHelper m_fontHelper;
    QToolButton* m_fontDecrease = nullptr;
    QToolButton* m_fontIncrease = nullptr;
    QTimer m_outcomeTimer;
    QString m_pendingOutcome;
    TsumeGameSession* m_session = nullptr;
    TsumeSolutionReplay* m_solution = nullptr;
    bool m_reviewing = false;
    bool m_boardRotated = false;
    QString m_savedStatus;
    ShogiGameController* m_game = nullptr;
    ShogiView* m_view = nullptr;
    BoardInteractionController* m_interaction = nullptr;
    TsumeProblem m_problem;
    int m_number = 0;
    int m_totalPlies = 0;
    TsumeProgressStore* m_store = nullptr;
    bool m_attemptRecorded = false;
    bool m_solvedRecorded = false;
    QLabel* m_header = nullptr;
    QLabel* m_history = nullptr;
    QLabel* m_status = nullptr;
    QSpinBox* m_timeout = nullptr;
    QPushButton* m_restart = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_retry = nullptr;
    QPushButton* m_showSolution = nullptr;
    QPlainTextEdit* m_solutionText = nullptr;
    QWidget* m_replayControls = nullptr;
    QLabel* m_solutionStatus = nullptr;
    QPushButton* m_solutionFirst = nullptr;
    QPushButton* m_solutionPrevious = nullptr;
    QPushButton* m_solutionNext = nullptr;
    QPushButton* m_solutionLast = nullptr;
    QPushButton* m_returnToGame = nullptr;
};

#endif
