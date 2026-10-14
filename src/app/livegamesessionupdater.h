#ifndef LIVEGAMESESSIONUPDATER_H
#define LIVEGAMESESSIONUPDATER_H

/// @file livegamesessionupdater.h
/// @brief LiveGameSession の更新ロジックを集約するクラスの定義

#include <QString>
#include "recordentry.h"
#include <QStringList>

class LiveGameSession;
class KifuBranchTree;

/**
 * @brief LiveGameSession の更新ロジックを集約するクラス
 *
 * セッション開始判定と、確定済み指し手の正本への追加を担当する。
 */
class LiveGameSessionUpdater
{
public:
    struct Deps {
        LiveGameSession* liveSession = nullptr;
        KifuBranchTree* branchTree = nullptr;
        QStringList* sfenRecord = nullptr;
        QString* currentSfenStr = nullptr;
    };

    void updateDeps(const Deps& deps);

    /**
     * @brief セッションが未開始なら開始する
     *
     * 途中局面の場合は SFEN から分岐ポイントを探し、
     * 見つかればそのノードから、見つからなければルートから開始する。
     */
    void startSessionIfNeeded();

    /// セッションが未開始なら開始し、型付きの指し手を追加する。
    void appendRecord(const RecordEntry& entry);


private:
    Deps m_deps;
};

#endif // LIVEGAMESESSIONUPDATER_H
