#ifndef TSUMECOLLECTIONDIALOG_H
#define TSUMECOLLECTIONDIALOG_H

#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <memory>

class TsumeCollectionSession;
class TsumePlayDialog;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QGridLayout;
class QScrollArea;
class EngineSelector;

class TsumeCollectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TsumeCollectionDialog(QWidget* parent = nullptr);
    ~TsumeCollectionDialog() override;
    bool loadFile(const QString& path);
signals:
    /// 詰将棋対局で指し手を盤に反映した（駒音を鳴らすため）
    void movePlayed();
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
    void timeoutChanged(int seconds);
    void updateCards();
    void reanalyzePage();
    void resetCollectionProgress();
    void resetAllProgress();
private:
    void buildUi();
    void playProblemAt(int position, int timeoutSec);
    void showStoreError();
    void resetProgress(bool allCollections);
    void rebuildPage();
    void arrangeCards();
    void updateCard(QPushButton* card);
    void savePreferences();
    QString lengthText(int index) const;
    /// 局面集・挑戦状況・手数の判定（QObject parent 所有）
    TsumeCollectionSession* m_session = nullptr;
    QList<int> m_filtered;      // 絞り込んだ問題の添字（表示の並び）
    QList<QPushButton*> m_cards;
    bool m_playing = false;
    QPointer<TsumePlayDialog> m_play;
    int m_playPosition = -1;   // 対局中の問題の m_filtered 内での位置
    QList<int> m_played;       // 対局画面で出題した問題の添字（一覧へ戻ったときに再判定する）
    QComboBox* m_engine = nullptr;
    /// 判定エンジンの選択（m_engine に結ぶ。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;
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
    QPushButton* m_resetCollection = nullptr;
};

#endif
