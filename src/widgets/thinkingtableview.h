#ifndef THINKINGTABLEVIEW_H
#define THINKINGTABLEVIEW_H

/// @file thinkingtableview.h
/// @brief 思考の表（対局のエンジン2つと検討タブで共用）

#include "analysissettings.h"
#include "pvtableview.h"
#include "shogienginethinkingmodel.h"

class ColumnWidthMemory;

/**
 * @brief エンジンの読み筋を1行ずつ出す表
 *
 * 読み筋の表（PvTableView）の時間・深さ・ノード数・評価値を数値の列にしたもの。列幅は思考の表の組（table）
 * ごとに、利用者が見出しの境目をドラッグした列だけ保存し（ColumnWidthMemory）、表を作るときに戻す。
 * 対局の2つのエンジンの表は同じ組なので、並べる側が userColumnWidthChanged を reloadColumnWidths に結んで揃える。
 * 保存していない列は見出しと数値の見本が収まる幅にする。読み筋の列は表示領域いっぱいまで伸ばす。
 */
class ThinkingTableView : public PvTableView
{
    Q_OBJECT

public:
    explicit ThinkingTableView(AnalysisSettings::ThinkingTable table, QWidget* parent = nullptr);

    void setThinkingModel(ShogiEngineThinkingModel* model);
    ShogiEngineThinkingModel* thinkingModel() const { return m_model; }

public slots:
    /// 保存した列幅を読み直して当てる（同じ組の別の表で利用者が列幅を変えたとき）
    void reloadColumnWidths();

signals:
    /// 利用者がこの表の列幅を変え、保存した
    void userColumnWidthChanged(int column);
    /// 行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);
    /// 行を右クリックした（受け手が menu に項目を足す。PvTableView::rowContextMenuAboutToShow に表を添えたもの）
    void thinkingRowContextMenuAboutToShow(ShogiEngineThinkingModel* model, int row, QMenu* menu);

protected:
    /// 利用者が決めた列幅（無ければ見出しと数値の見本が収まる幅）を当てる
    void fitColumns() override;

private slots:
    void onBoardRequested(int row);
    void onRowContextMenuAboutToShow(int row, QMenu* menu);

private:
    AnalysisSettings::ThinkingTable m_table;
    ShogiEngineThinkingModel* m_model = nullptr;
    ColumnWidthMemory* m_columnWidths = nullptr;   ///< 利用者が決めた列幅（見出しが親）
};

#endif // THINKINGTABLEVIEW_H
