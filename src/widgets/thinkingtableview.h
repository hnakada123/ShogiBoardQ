#ifndef THINKINGTABLEVIEW_H
#define THINKINGTABLEVIEW_H

/// @file thinkingtableview.h
/// @brief 思考の表（対局のエンジン2つと検討タブで共用）

#include "analysissettings.h"
#include "pvtableview.h"
#include "shogienginethinkingmodel.h"

/**
 * @brief エンジンの読み筋を1行ずつ出す表
 *
 * 読み筋の表（PvTableView）の時間・深さ・ノード数・評価値を数値の列にしたもの。列幅は思考の表の組（table）
 * ごとに、利用者が見出しの境目をドラッグしたときだけ保存し、表を作るときに戻す。読み筋の列は表示領域
 * いっぱいまで伸ばす。
 */
class ThinkingTableView : public PvTableView
{
    Q_OBJECT

public:
    explicit ThinkingTableView(AnalysisSettings::ThinkingTable table, QWidget* parent = nullptr);

    void setThinkingModel(ShogiEngineThinkingModel* model);
    ShogiEngineThinkingModel* thinkingModel() const { return m_model; }

signals:
    /// 行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);
    /// 行を右クリックした（受け手が menu に項目を足す。PvTableView::rowContextMenuAboutToShow に表を添えたもの）
    void thinkingRowContextMenuAboutToShow(ShogiEngineThinkingModel* model, int row, QMenu* menu);

protected:
    /// 保存した列幅（無ければ見出しと数値の見本が収まる幅）を当てる
    void fitColumns() override;

private slots:
    void onBoardRequested(int row);
    void onRowContextMenuAboutToShow(int row, QMenu* menu);
    void onSectionResizedByUser(int logicalIndex);

private:

    AnalysisSettings::ThinkingTable m_table;
    ShogiEngineThinkingModel* m_model = nullptr;
};

#endif // THINKINGTABLEVIEW_H
