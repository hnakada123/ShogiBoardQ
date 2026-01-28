#ifndef ANALYSISRESULTSPRESENTER_H
#define ANALYSISRESULTSPRESENTER_H

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
class QAbstractItemModel;
class QTimer;
class QPushButton;
class QDockWidget;
class KifuAnalysisListModel;

class AnalysisResultsPresenter : public QObject
{
    Q_OBJECT
public:
    explicit AnalysisResultsPresenter(QObject* parent=nullptr);

    // ドックウィジェットを設定（MainWindowから呼び出し）
    void setDockWidget(QDockWidget* dock);

    // コンテナウィジェットを取得（ドック作成時に使用）
    QWidget* containerWidget();

    // モデルを受け取り、ドックを表示（UIは初回のみ構築）
    void showWithModel(KifuAnalysisListModel* model);

    QTableView* view() const { return m_view; }
    QWidget* container() const { return m_container; }
    
    // 中止ボタンの有効/無効を設定
    void setStopButtonEnabled(bool enabled);
    
    // 解析中フラグを設定（行追加時の自動選択に使用）
    void setAnalyzing(bool analyzing) { m_isAnalyzing = analyzing; }
    
    // 解析完了を通知（完了メッセージボックスを表示）
    void showAnalysisComplete(int totalMoves);

signals:
    // 中止ボタンが押されたときに発行
    void stopRequested();
    
    // 行がダブルクリックされたときに発行（読み筋表示用）
    void rowDoubleClicked(int row);
    
    // 行が選択されたときに発行（棋譜欄・将棋盤・分岐ツリー連動用）
    void rowSelected(int row);

private slots:
    void reflowNow();
    void onModelReset();
    void onRowsInserted(const QModelIndex&, int, int);
    void onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&);
    void onLayoutChanged();
    void onScrollRangeChanged(int, int);
    void onTableClicked(const QModelIndex& index);
    void onTableSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    
    // フォントサイズ変更
    void increaseFontSize();
    void decreaseFontSize();
    
    // ウィンドウサイズ保存
    void saveWindowSize();

private:
    void buildUi(KifuAnalysisListModel* model);
    void setupHeaderConfiguration();  // ヘッダーの列設定を適用
    void connectModelSignals(KifuAnalysisListModel* model);
    void restoreFontSize();

    QPointer<QDockWidget> m_dock;
    QPointer<QWidget> m_container;
    QPointer<QTableView> m_view;
    QPointer<QHeaderView> m_header;
    QPointer<QPushButton> m_stopButton;
    QTimer* m_reflowTimer;

    // 解析中フラグ（行追加時に最後の行を自動選択）
    bool m_isAnalyzing = false;

    // 自動選択中フラグ（rowSelectedシグナル発行をスキップ）
    bool m_autoSelecting = false;

    // UIが既に構築済みかどうか
    bool m_uiBuilt = false;
};

#endif // ANALYSISRESULTSPRESENTER_H
