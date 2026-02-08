#ifndef ANALYSISRESULTSPRESENTER_H
#define ANALYSISRESULTSPRESENTER_H

/// @file analysisresultspresenter.h
/// @brief 解析結果表示プレゼンタクラスの定義


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

/**
 * @brief 棋譜解析結果ドックの構築と表示更新を担当するプレゼンタ
 *
 * 解析結果テーブル、停止ボタン、フォントサイズ操作などの
 * UI制御をまとめて管理する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class AnalysisResultsPresenter : public QObject
{
    Q_OBJECT
public:
    explicit AnalysisResultsPresenter(QObject* parent=nullptr);

    /// 結果表示先のドックウィジェットを設定する
    void setDockWidget(QDockWidget* dock);

    /// ドックに配置するコンテナウィジェットを返す
    QWidget* containerWidget();

    /// 指定モデルを表示してドックを可視化する（UI構築は初回のみ）
    void showWithModel(KifuAnalysisListModel* model);

    /// 結果テーブルビューを返す
    QTableView* view() const { return m_view; }

    /// コンテナウィジェットを返す
    QWidget* container() const { return m_container; }
    
    /// 解析中止ボタンの有効/無効を切り替える
    void setStopButtonEnabled(bool enabled);
    
    /// 解析実行中フラグを設定する（行追加時の自動選択に使用）
    void setAnalyzing(bool analyzing) { m_isAnalyzing = analyzing; }
    
    /// 解析完了メッセージを表示する
    void showAnalysisComplete(int totalMoves);

signals:
    /// 中止ボタン押下時に発行（→ AnalysisFlowController::stop）
    void stopRequested();
    
    /// 盤面列クリック時に発行（→ AnalysisFlowController::onResultRowDoubleClicked）
    void rowDoubleClicked(int row);
    
    /// 行選択時に発行（→ AnalysisFlowController::analysisResultRowSelected → DialogCoordinator → MainWindow）
    void rowSelected(int row);

private slots:
    /// 列幅再計算を遅延実行する
    void reflowNow();

    /// モデル再初期化時に表示状態を整える
    void onModelReset();

    /// 行追加時にスクロール/選択を更新する
    void onRowsInserted(const QModelIndex&, int, int);

    /// モデルデータ更新時に再描画を調整する
    void onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&);

    /// レイアウト変更時に再配置を行う
    void onLayoutChanged();

    /// スクロール範囲変更時に追従処理を行う
    void onScrollRangeChanged(int, int);

    /// テーブルクリック時に行選択通知を行う
    void onTableClicked(const QModelIndex& index);

    /// 選択行変更時に通知を行う
    void onTableSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    
    /// フォントサイズを1段階拡大する
    void increaseFontSize();

    /// フォントサイズを1段階縮小する
    void decreaseFontSize();
    
    /// ドックサイズを設定へ保存する
    void saveWindowSize();

private:
    /// 初回UI構築を行う
    void buildUi(KifuAnalysisListModel* model);

    /// ヘッダー列設定を適用する
    void setupHeaderConfiguration();

    /// モデル更新シグナルを接続する
    void connectModelSignals(KifuAnalysisListModel* model);

    /// 保存済みフォントサイズを復元する
    void restoreFontSize();

    QPointer<QDockWidget> m_dock;       ///< 結果表示ドック（非所有）
    QPointer<QWidget> m_container;      ///< ドック内コンテナ（非所有）
    QPointer<QTableView> m_view;        ///< 解析結果テーブル（非所有）
    QPointer<QHeaderView> m_header;     ///< テーブルヘッダー（非所有）
    QPointer<QPushButton> m_stopButton; ///< 中止ボタン（非所有）
    QTimer* m_reflowTimer;              ///< 再レイアウト遅延実行タイマー（thisが所有）

    bool m_isAnalyzing = false;    ///< 解析中フラグ（行追加時の自動選択制御）

    bool m_autoSelecting = false;  ///< 自動選択中フラグ（rowSelected抑止用）

    bool m_uiBuilt = false;        ///< UI構築済みフラグ
};

#endif // ANALYSISRESULTSPRESENTER_H
