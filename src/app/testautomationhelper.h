#ifndef TESTAUTOMATIONHELPER_H
#define TESTAUTOMATIONHELPER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>

class RecordPane;
class EngineAnalysisTab;
class KifuBranchTree;
class KifuNavigationState;
class KifuDisplayCoordinator;
class KifuRecordListModel;
class KifuBranchListModel;
class ShogiGameController;
class ShogiView;
struct ShogiMove;

/**
 * @brief テスト自動化用メソッドを集約するヘルパークラス
 *
 * MainWindowから分離されたテスト自動化メソッド群。
 * UI操作のシミュレーションと状態検証を担当する。
 */
class TestAutomationHelper : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Deps {
        RecordPane* recordPane = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        KifuBranchListModel* kifuBranchModel = nullptr;
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        bool* skipBoardSyncForBranchNav = nullptr;
    };

    explicit TestAutomationHelper(QObject* parent = nullptr);

    /**
     * @brief 依存オブジェクトを更新
     */
    void updateDeps(const Deps& deps);

    // ★ テスト自動化: UI操作シミュレーション
    void setTestMode(bool enabled);
    bool isTestMode() const { return m_testMode; }

    void navigateToPly(int ply);
    void clickBranchCandidate(int index);
    void clickNextButton();
    void clickPrevButton();
    void clickFirstButton();
    void clickLastButton();
    void clickKifuRow(int row);
    void clickBranchTreeNode(int row, int ply);

    // ★ テスト自動化: 状態ダンプと検証
    void dumpTestState();
    bool verify4WayConsistency();
    int getBranchTreeNodeCount();
    bool verifyBranchTreeNodeCount(int minExpected);

    // ★ ユーティリティ
    QString extractSfenBase(const QString& sfen) const;

private:
    Deps m_deps;
    bool m_testMode = false;
};

#endif // TESTAUTOMATIONHELPER_H
