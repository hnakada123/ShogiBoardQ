#ifndef CONSIDERATIONPOSITIONRESOLVER_H
#define CONSIDERATIONPOSITIONRESOLVER_H

/// @file considerationpositionresolver.h
/// @brief 検討モード向け局面解決ロジックの定義

#include <QString>
#include <QStringList>
#include <QVector>

#include "shogimove.h"

class KifuRecordListModel;
class KifuLoadCoordinator;
class KifuBranchTree;
class KifuNavigationState;

/**
 * @brief 検討モード更新用の局面/ハイライト情報を解決する
 *
 * MainWindow から UI 非依存の棋譜解決ロジックを分離するためのヘルパー。
 */
class ConsiderationPositionResolver
{
public:
    struct Inputs {
        const QStringList* positionStrList = nullptr;
        const QStringList* gameUsiMoves = nullptr;
        const QVector<ShogiMove>* gameMoves = nullptr;
        const QString* startSfenStr = nullptr;
        const QStringList* sfenRecord = nullptr;
        const KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        const KifuRecordListModel* kifuRecordModel = nullptr;
        const KifuBranchTree* branchTree = nullptr;
        const KifuNavigationState* navState = nullptr;
    };

    struct UpdateParams {
        QString position;
        int previousFileTo = 0;
        int previousRankTo = 0;
        QString lastUsiMove;
    };

    explicit ConsiderationPositionResolver(const Inputs& inputs);

    UpdateParams resolveForRow(int row) const;

private:
    QString buildPositionStringForIndex(int moveIndex) const;
    void resolvePreviousMoveCoordinates(int row, int& fileTo, int& rankTo) const;
    QString resolveLastUsiMoveForPly(int row) const;

    Inputs m_inputs;
};

#endif // CONSIDERATIONPOSITIONRESOLVER_H
