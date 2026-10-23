#ifndef ANALYSISRESULTSPANEL_H
#define ANALYSISRESULTSPANEL_H

/// @file analysisresultspanel.h
/// @brief 棋譜解析の結果ドックの中身（結果の表・停止ボタン・文字の大きさ）

#include <QWidget>
#include "pvtableview.h"
#include <QHeaderView>

#include "fontsizehelper.h"
class QAbstractItemModel;
class QTimer;
class QPushButton;
class QLabel;
class QMenu;
class ElideLabel;
class QProgressBar;
class KifuAnalysisListModel;

/**
 * @brief 棋譜解析の結果ドックの中身を作り、結果の表を表示する
 *
 * 結果の表・進み具合・停止ボタン・文字の大きさの操作を持つ。部品はコンストラクタで作る。
 * 行の選択・盤面の表示・停止はシグナルで KifuAnalysisController に知らせる。ドックの表示は受け手が行う。
 */
class AnalysisResultsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisResultsPanel(QWidget* parent = nullptr);

    /// 結果の表のモデルを出す（同じモデルなら何もしない）
    void setModel(KifuAnalysisListModel* model);

    /// 結果テーブルビューを返す
    PvTableView* view() const { return m_view; }

    /// 解析の進み（進み具合の文・中止ボタン・行の選択と盤面の表示を受け付けるかを決める）
    enum class RunState {
        Idle,       ///< まだ解析していない
        Running,
        Stopped,    ///< 途中で止めた
        Completed,  ///< 最後まで解析した
    };

    /// 対象局面数と使用エンジンを表示して解析中にする
    void beginAnalysis(int totalPositions, const QString& engineName);
    /// 解析を終えた（completed なら最後まで解析した、そうでなければ途中で止めた）
    void finishAnalysis(bool completed);
    RunState runState() const { return m_runState; }

    /// 表示中の局面に合わせて行を選ぶ（通知しない。範囲外なら選択解除、解析中は変更しない）
    void selectRow(int row);

signals:
    /// 中止ボタン押下時に発行（→ KifuAnalysisController::stop）
    void stopRequested();
    
    /// 盤面列クリック時に発行（→ KifuAnalysisController が読み筋の盤を出す）
    void rowDoubleClicked(int row);
    
    /// 行選択時に発行（KifuAnalysisController が棋譜をその手へ移す）
    void rowSelected(int row);

    /// 行を右クリックした（受け手が menu に項目を足す）
    void rowContextMenuAboutToShow(int row, QMenu* menu);

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

    /// 「盤面」が押された（解析中でなければ rowDoubleClicked で知らせる）
    void onBoardRequested(int row);

    /// 選択行変更時に通知を行う
    void onTableSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

    void saveColumnWidth(int column, int oldSize, int newSize);

private:
    /// ヘッダー列設定を適用する
    void setupHeaderConfiguration();

    /// モデル更新シグナルを接続する
    void connectModelSignals(KifuAnalysisListModel* model);

    /// 表の文字の大きさを変える（A-/A+）
    void applyFontSize(int size);
    void updateProgress();

    PvTableView* m_view = nullptr;          ///< 解析結果テーブル（読み筋の表）
    QHeaderView* m_header = nullptr;        ///< テーブルヘッダー
    QPushButton* m_stopButton = nullptr;    ///< 中止ボタン
    QPushButton* m_fontDecrease = nullptr;
    QPushButton* m_fontIncrease = nullptr;
    QLabel* m_statusLabel = nullptr;
    ElideLabel* m_engineLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QTimer* m_reflowTimer;              ///< 再レイアウト遅延実行タイマー（thisが所有）
    FontSizeHelper m_fontSize;          ///< 表の文字の大きさ（変更のたびに保存する）

    QList<int> m_columnWidths;     ///< ユーザー指定の列幅（0は自動）
    bool m_reflowAllRows = true;   ///< 次の列幅合わせで全行を測り直す（リセット・書体の変更）
    int m_reflowFromRow = -1;      ///< 次の列幅合わせで測り始める行（足した・変わった行。-1 は無し）
    int m_totalPositions = 0;

    RunState m_runState = RunState::Idle;   ///< 解析中は最後に確定した行を選び、利用者の選択・盤面の表示は無視する
};

#endif // ANALYSISRESULTSPANEL_H
