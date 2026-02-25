#ifndef LIVEGAMESESSIONUPDATER_H
#define LIVEGAMESESSIONUPDATER_H

/// @file livegamesessionupdater.h
/// @brief LiveGameSession の更新ロジックを集約するクラスの定義

#include <QString>
#include <QStringList>

class LiveGameSession;
class KifuBranchTree;
class ShogiGameController;
class ShogiMove;

/**
 * @brief LiveGameSession の更新ロジックを集約するクラス
 *
 * MainWindow::updateGameRecord() から LiveGameSession 関連の処理を分離し、
 * セッション開始判定・SFEN 構築・指し手追加を担当する。
 */
class LiveGameSessionUpdater
{
public:
    struct Deps {
        LiveGameSession* liveSession = nullptr;
        KifuBranchTree* branchTree = nullptr;
        ShogiGameController* gameController = nullptr;
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
    void ensureSessionStarted();

    /**
     * @brief 指し手を LiveGameSession に追加する
     *
     * セッション未開始なら遅延開始し、現在の盤面から SFEN を構築して
     * liveSession->addMove() を実行する。
     *
     * @param move 指し手
     * @param moveText 指し手の表示テキスト
     * @param elapsedTime 消費時間
     */
    void appendMove(const ShogiMove& move, const QString& moveText, const QString& elapsedTime);

private:
    Deps m_deps;
};

#endif // LIVEGAMESESSIONUPDATER_H
