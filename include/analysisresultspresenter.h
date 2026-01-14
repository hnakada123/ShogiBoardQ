#ifndef ANALYSISRESULTSPRESENTER_H
#define ANALYSISRESULTSPRESENTER_H

#include <QObject>
#include <QPointer>
#include <QDialog>
#include <QTableView>
#include <QHeaderView>
class QAbstractItemModel;
class QTimer;
class QPushButton;
class KifuAnalysisListModel;

class AnalysisResultsPresenter : public QObject
{
    Q_OBJECT
public:
    explicit AnalysisResultsPresenter(QObject* parent=nullptr);

    // モデルを受け取り、非モーダルで表示（UIは都度作り直し）
    void showWithModel(KifuAnalysisListModel* model);

    QTableView* view() const { return m_view; }
    QDialog* dialog() const { return m_dlg; }
    
    // 中止ボタンの有効/無効を設定
    void setStopButtonEnabled(bool enabled);
    
    // 解析完了を通知（完了メッセージボックスを表示）
    void showAnalysisComplete(int totalMoves);

signals:
    // 中止ボタンが押されたときに発行
    void stopRequested();
    
    // 行がダブルクリックされたときに発行（読み筋表示用）
    void rowDoubleClicked(int row);

private slots:
    void reflowNow();
    void onModelReset();
    void onRowsInserted(const QModelIndex&, int, int);
    void onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&);
    void onLayoutChanged();
    void onScrollRangeChanged(int, int);
    void onTableDoubleClicked(const QModelIndex& index);
    
    // フォントサイズ変更スロット
    void onFontIncrease();
    void onFontDecrease();

private:
    void buildUi(KifuAnalysisListModel* model);
    void connectModelSignals(KifuAnalysisListModel* model);
    void applyFontSize();
    void saveFontSize();
    void loadFontSize();

    QPointer<QDialog> m_dlg;
    QPointer<QTableView> m_view;
    QPointer<QHeaderView> m_header;
    QPointer<QPushButton> m_stopButton;
    QPointer<QPushButton> m_fontIncreaseButton;
    QPointer<QPushButton> m_fontDecreaseButton;
    QTimer* m_reflowTimer;
    
    // 現在のフォントサイズ（ポイント）
    int m_fontSize;
    
    // フォントサイズのデフォルト値
    static constexpr int DefaultFontSize = 10;
    static constexpr int MinFontSize = 8;
    static constexpr int MaxFontSize = 24;
};

#endif // ANALYSISRESULTSPRESENTER_H
