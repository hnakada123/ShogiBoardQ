#ifndef CONSIDERATIONPOSITIONSERVICE_H
#define CONSIDERATIONPOSITIONSERVICE_H

/// @file considerationpositionservice.h
/// @brief 検討モードでの局面解決サービスの定義

#include <QObject>
#include <QStringList>
#include <QVector>

class KifuBranchTree;
class KifuLoadCoordinator;
class KifuNavigationState;
class KifuRecordListModel;
class MatchCoordinator;
class EngineAnalysisTab;
struct ShogiMove;
enum class PlayMode;

/**
 * @brief 検討モードでの局面解決ロジックを担当するサービス
 *
 * MainWindow::onBuildPositionRequired の分岐・本譜統合局面解決ロジックを
 * 集約し、検討モード固有の局面解決責務を一元管理する。
 */
class ConsiderationPositionService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        const PlayMode* playMode = nullptr;                   ///< 現在のプレイモード（外部所有）
        const QStringList* positionStrList = nullptr;         ///< 局面文字列リスト（外部所有）
        const QStringList* gameUsiMoves = nullptr;            ///< USI手順リスト（外部所有）
        const QVector<ShogiMove>* gameMoves = nullptr;        ///< 指し手リスト（外部所有）
        const QString* startSfenStr = nullptr;                ///< 開始局面SFEN（外部所有）
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;   ///< 棋譜ロードコーディネータ（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;       ///< 棋譜レコードモデル（非所有）
        KifuBranchTree* branchTree = nullptr;                 ///< 分岐ツリー（非所有）
        KifuNavigationState* navState = nullptr;              ///< ナビゲーション状態（非所有）
        MatchCoordinator* match = nullptr;                    ///< 対局コーディネータ（非所有）
        EngineAnalysisTab* analysisTab = nullptr;             ///< 解析タブ（非所有）
    };

    explicit ConsiderationPositionService(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// 検討モードで指定行の局面を解決し、検討ポジションを更新する
    void handleBuildPositionRequired(int row);

private:
    Deps m_deps;
};

#endif // CONSIDERATIONPOSITIONSERVICE_H
