#ifndef THINKINGTABLEVIEW_H
#define THINKINGTABLEVIEW_H

/// @file thinkingtableview.h
/// @brief 思考の表（対局のエンジン2つと検討タブで共用）

#include "analysissettings.h"
#include "incrementaltableview.h"
#include "shogienginethinkingmodel.h"

/**
 * @brief エンジンの読み筋を1行ずつ出す表
 *
 * 時間・深さ・ノード数・評価値は右寄せの3桁区切り、「盤面」列はボタン、読み筋は幅に合わせて
 * 短くして描く。列幅は思考の表の組（table）ごとに、利用者が見出しの境目をドラッグしたときだけ
 * 保存し、表を作るときに戻す。読み筋の列は表示領域いっぱいまで伸ばす。
 */
class ThinkingTableView : public IncrementalTableView
{
    Q_OBJECT

public:
    explicit ThinkingTableView(AnalysisSettings::ThinkingTable table, QWidget* parent = nullptr);

    void setThinkingModel(ShogiEngineThinkingModel* model);
    ShogiEngineThinkingModel* thinkingModel() const { return m_model; }

    /// 文字の大きさ（ポイント）を変え、行の高さと列幅を合わせる
    void setFontPointSize(int pointSize);

    /// 選択を外す（読み筋の局面を閉じたとき）
    void clearRowSelection();

signals:
    /// 行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);

private slots:
    void onClicked(const QModelIndex& index);
    void onSectionResizedByUser(int logicalIndex);

private:
    /// 保存した列幅（無ければ見出しと数値の見本が収まる幅）を当てる
    void applyColumnWidths();

    AnalysisSettings::ThinkingTable m_table;
    ShogiEngineThinkingModel* m_model = nullptr;
};

#endif // THINKINGTABLEVIEW_H
