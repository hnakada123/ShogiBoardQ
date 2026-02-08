#ifndef NAVIGATIONPRESENTER_H
#define NAVIGATIONPRESENTER_H

/// @file navigationpresenter.h
/// @brief ナビゲーションプレゼンタクラスの定義


#include <QObject>
#include <QPointer>

class EngineAnalysisTab;

// 分岐ツリーハイライトと通知に専念する Presenter。
// - 分岐候補表示は KifuDisplayCoordinator が管理
// - NavigationPresenter はツリーハイライトとシグナル通知のみを担当
class NavigationPresenter : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        EngineAnalysisTab*   analysisTab = nullptr;   // 分岐ツリーハイライト先（任意）
    };

    explicit NavigationPresenter(const Deps& d, QObject* parent = nullptr);

    // row/ply を軸に「候補再構築 → ハイライト」を一括実行（Coordinator を呼ぶ）
    void refreshAll(int row, int ply);

    // 分岐候補だけを再構築したいとき（refreshAll と同じで OK）
    void refreshBranchCandidates(int row, int ply);

    // 再帰対策用：Coordinator が「候補モデル更新を完了」した直後に呼ぶ“事後通知”
    //    （Coordinator 内から呼び出すのはコレだけ。refresh* は呼ばない。）
    void updateAfterBranchListChanged(int row, int ply);

    // 依存オブジェクトの差し替え
    void setDeps(const Deps& d);

signals:
    // UI が更新されたことを通知（必要に応じて受ける）
    void branchUiUpdated(int row, int ply);

private:
    void highlightBranchTree(int row, int ply);

    QPointer<EngineAnalysisTab>   m_analysisTab;
};

#endif // NAVIGATIONPRESENTER_H
