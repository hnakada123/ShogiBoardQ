#ifndef NAVIGATIONPRESENTER_H
#define NAVIGATIONPRESENTER_H

#include <QObject>
#include <QPointer>

class KifuLoadCoordinator;
class EngineAnalysisTab;

// 分岐候補 UI の更新とツリーハイライトに専念する Presenter。
// - 候補の“再構築”は Coordinator に依頼する
// - Coordinator が内部で候補モデルを更新し終えたら、Presenter の「事後通知API」を呼ぶ
class NavigationPresenter : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        KifuLoadCoordinator* coordinator = nullptr;   // 候補生成の実体
        EngineAnalysisTab*   analysisTab = nullptr;   // 分岐ツリーハイライト先（任意）
    };

    explicit NavigationPresenter(const Deps& d, QObject* parent = nullptr);

    // row/ply を軸に「候補再構築 → ハイライト」を一括実行（Coordinator を呼ぶ）
    void refreshAll(int row, int ply);

    // 分岐候補だけを再構築したいとき（refreshAll と同じで OK）
    void refreshBranchCandidates(int row, int ply);

    // ★ 再帰対策用：Coordinator が「候補モデル更新を完了」した直後に呼ぶ“事後通知”
    //    （Coordinator 内から呼び出すのはコレだけ。refresh* は呼ばない。）
    void updateAfterBranchListChanged(int row, int ply);

    // 依存オブジェクトの差し替え
    void setDeps(const Deps& d);

signals:
    // UI が更新されたことを通知（必要に応じて受ける）
    void branchUiUpdated(int row, int ply);

private:
    void highlightBranchTree_(int row, int ply);

    QPointer<KifuLoadCoordinator> m_coordinator;
    QPointer<EngineAnalysisTab>   m_analysisTab;
};

#endif // NAVIGATIONPRESENTER_H
