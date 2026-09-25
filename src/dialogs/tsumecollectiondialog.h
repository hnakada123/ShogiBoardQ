#ifndef TSUMECOLLECTIONDIALOG_H
#define TSUMECOLLECTIONDIALOG_H

#include "tsumecollection.h"
#include "tsumeevaluation.h"
#include "tsumeprogressstore.h"
#include "fontsizehelper.h"
#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <memory>

class TsumePlayDialog;
class TsumePositionAnalyzer;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QGridLayout;
class QScrollArea;
class QToolButton;

class TsumeCollectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TsumeCollectionDialog(QWidget* parent = nullptr);
    ~TsumeCollectionDialog() override;
    bool loadFile(const QString& path);
protected:
    void done(int result) override;
    void resizeEvent(QResizeEvent* event) override;
private slots:
    void openFile();
    void restoreLastFile();
    void filterChanged();
    void pageChanged();
    void firstPage();
    void previousPage();
    void nextPage();
    void lastPage();
    void engineChanged();
    void startProblem();
    void playPreviousProblem();
    void playNextProblem();
    void analyzeNext();
    void analysisFinished(const TsumeEvaluation& result);
    void reanalyzePage();
    void onFontIncrease();
    void onFontDecrease();
private:
    void buildUi();
    void playProblemAt(int position, int timeoutSec);
    void refreshProgress();
    void rebuildPage();
    void arrangeCards();
    void updateCard(QPushButton* card);
    void cancelAnalysis();
    void savePreferences();
    void applyFontSize();
    FontSizeHelper m_fontHelper;
    QToolButton* m_fontDecrease = nullptr;
    QToolButton* m_fontIncrease = nullptr;
    QString lengthText(int index) const;
    QString m_file;
    QList<TsumeProblem> m_problems;
    QStringList m_ids;
    QList<int> m_filtered;
    QList<int> m_queue;
    QList<QPushButton*> m_cards;
    QHash<QString, TsumeProgressStore::Progress> m_progress;
    QHash<QString, TsumeEvaluation> m_results;
    std::unique_ptr<TsumeProgressStore> m_store;
    TsumePositionAnalyzer* m_analyzer = nullptr;
    QTimer m_analysisTimer;
    int m_pending = -1;
    bool m_playing = false;
    QPointer<TsumePlayDialog> m_play;
    int m_playPosition = -1;   // 対局中の問題の m_filtered 内での位置
    QList<int> m_played;       // 対局画面で出題した問題の添字（一覧へ戻ったときに再判定する）
    QComboBox* m_engine = nullptr;
    QComboBox* m_pageSize = nullptr;
    QComboBox* m_filter = nullptr;
    QSpinBox* m_page = nullptr;
    QSpinBox* m_timeout = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_notice = nullptr;
    QScrollArea* m_scroll = nullptr;
    QGridLayout* m_grid = nullptr;
    QPushButton* m_first = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
    QPushButton* m_last = nullptr;
};

#endif
