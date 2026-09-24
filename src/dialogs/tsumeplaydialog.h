#ifndef TSUMEPLAYDIALOG_H
#define TSUMEPLAYDIALOG_H

#include <QDialog>
#include <QTimer>
#include "tsumecollection.h"
#include "tsumegamesession.h"

class ShogiGameController;
class ShogiView;
class BoardInteractionController;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

/// 同じShogiViewを使い、局面集の選択から解答までを扱う専用画面。
class TsumePlayDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TsumePlayDialog(QWidget* parent = nullptr);
    ~TsumePlayDialog() override;
    bool loadFile(const QString& path);

protected:
    void done(int result) override;

private slots:
    void openFile();
    void selectProblem();
    void restoreLastFile();
    void requestMove(const QPoint& from, const QPoint& to);
    void updatePosition(const QString& sfen, const QString& move);
    void updateState();
    void showOutcome(TsumeGameSession::Outcome outcome, int remaining);
    void showPendingOutcome();
    void rejectMove();
    void updateTimeLimit();

private:
    void buildUi();
    void cancelPendingOutcome();
    QTimer m_outcomeTimer;
    QString m_pendingOutcome;
    TsumeGameSession* m_session = nullptr;
    ShogiGameController* m_game = nullptr;
    ShogiView* m_view = nullptr;
    BoardInteractionController* m_interaction = nullptr;
    QList<TsumeProblem> m_problems;
    QString m_file;
    QComboBox* m_selector = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_status = nullptr;
    QSpinBox* m_timeout = nullptr;
    QPushButton* m_restart = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_retry = nullptr;
};

#endif
