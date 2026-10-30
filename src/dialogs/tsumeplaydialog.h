#ifndef TSUMEPLAYDIALOG_H
#define TSUMEPLAYDIALOG_H

#include <QDialog>
#include <QTimer>
#include "boardinputroute.h"
#include "tsumecollection.h"
#include "tsumegamesession.h"

class ShogiBoard;
class ShogiView;
class BoardViewerWidget;
class BoardInteractionController;
class TsumeProgressStore;
class TsumeSolutionReplay;
class QVBoxLayout;
class QStackedWidget;
class QLabel;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;

/// 一覧で選んだ1問を、既存の ShogiView で解く画面。盤で指した手の行き先（BoardInputRoute）は解答の受け付け
class TsumePlayDialog : public QDialog, private BoardInputRoute
{
    Q_OBJECT
public:
    explicit TsumePlayDialog(QWidget* parent = nullptr);
    ~TsumePlayDialog() override;
    void setProblem(const TsumeProblem& problem, int number, const QString& enginePath,
                    TsumeProgressStore* store, int timeoutSec);
    /// 一覧の前後に問題があるときだけ「前の問題」「次の問題」を押せるようにする。
    void setProblemNavigation(bool hasPrevious, bool hasNext);
    int timeoutSec() const;

signals:
    void previousProblemRequested();
    void nextProblemRequested();
    /// 対局中の攻め方・玉方の指し手を盤に反映した（駒音を鳴らすため。正解手順の再生では出さない）
    void movePlayed();

protected:
    void done(int result) override;

private:
    // --- BoardInputRoute（盤で指した手） ---
    bool acceptsInput() const override;
    bool isEditing() const override { return false; }
    void submit(const QPoint& from, const QPoint& to) override;

private slots:
    void selectProblem();
    void updatePosition(const QString& sfen, const QString& move);
    void onSessionPositionChanged(const QString& sfen, const QString& move);
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
    void onFlipToggled(bool flipped);
    void cancelBoardSelection();

private:
    void buildUi();
    void buildActionControls(QVBoxLayout* layout);
    void buildReplayUi();
    void buildBoardControls(QVBoxLayout* layout);
    void applyBoardOrientation();
    void showSolution(int ply);
    /// 案内の欄（正解手順の表示と状態表示）を、今の動作と探索の状態に合わせる
    void updateReplayInfo();
    /// 部品の有効・無効と切り替える欄を、controlStates() の表に合わせる
    void applyControlStates();
    void cancelPendingOutcome();
    /// 案内文と状態表示の高さを今の文字に合わせる（盤を文字の大きさで動かさない）
    void fitInfoToFont();

    /// 上段: 問題番号・前後の問題・判定時間・挑戦履歴
    struct ProblemBar {
        QLabel* header = nullptr;
        QPushButton* previous = nullptr;
        QPushButton* next = nullptr;
        QSpinBox* timeout = nullptr;
        QLabel* history = nullptr;
    };
    /// 盤の上下の案内: 案内文と正解手順を切り替える領域、盤の下の状態表示
    struct InfoPanel {
        QStackedWidget* stack = nullptr;
        QLabel* solutionStatus = nullptr;
        QPlainTextEdit* solutionText = nullptr;
        QLabel* status = nullptr;
    };
    /// 盤の下の操作: 対局中の操作と、探索の中止・再判定
    struct ActionPanel {
        QStackedWidget* stack = nullptr;
        QWidget* game = nullptr;
        QPushButton* restart = nullptr;
        QPushButton* undo = nullptr;
        QPushButton* stop = nullptr;
        QPushButton* retry = nullptr;
    };
    /// 正解手順の再生: 局面送りと、対局・再生を切り替えるボタン
    struct ReplayPanel {
        QWidget* row = nullptr;
        QPushButton* first = nullptr;
        QPushButton* previous = nullptr;
        QPushButton* next = nullptr;
        QPushButton* last = nullptr;
        QStackedWidget* modeButton = nullptr;
        QPushButton* showSolution = nullptr;
        QPushButton* returnToGame = nullptr;
    };

    /// 対局（自分で解く）と正解手順の再生
    enum class Mode { Playing, Reviewing };

    /// 動作状態ごとの部品の有効・無効の表（1か所で決める）
    struct ControlStates {
        bool restart = false;
        bool undo = false;
        bool stop = false;
        bool retry = false;
        bool showSolution = false;
        bool solutionFirst = false;   ///< 「開始局面へ」「1手戻る」
        bool solutionForward = false; ///< 「1手進む」「詰み局面へ」
        bool returnToGame = false;
    };
    ControlStates controlStates() const;
    bool reviewing() const { return m_mode == Mode::Reviewing; }

    QTimer m_outcomeTimer;
    QString m_pendingOutcome;
    TsumeGameSession* m_session = nullptr;
    TsumeSolutionReplay* m_solution = nullptr;
    Mode m_mode = Mode::Playing;
    bool m_boardRotated = false;
    QString m_savedStatus;
    ShogiBoard* m_board = nullptr;          ///< 解いている局面の盤
    BoardViewerWidget* m_viewer = nullptr;  ///< 盤と縮小・拡大・回転の操作列
    ShogiView* m_view = nullptr;            ///< m_viewer の盤
    BoardInteractionController* m_interaction = nullptr;
    TsumeProblem m_problem;
    int m_number = 0;
    int m_totalPlies = 0;
    TsumeProgressStore* m_store = nullptr;  ///< 挑戦履歴の表示に読む（記録は TsumeGameSession が付ける）
    ProblemBar m_problemUi;
    InfoPanel m_infoUi;
    ActionPanel m_actionUi;
    ReplayPanel m_replayUi;
};

#endif
