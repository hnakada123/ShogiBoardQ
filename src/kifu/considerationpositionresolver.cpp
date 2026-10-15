/// @file considerationpositionresolver.cpp
/// @brief 検討モード向け局面解決ロジックの実装

#include "considerationpositionresolver.h"

#include "sfenutils.h"

ConsiderationPositionResolver::UpdateParams
ConsiderationPositionResolver::resolve(const RecordLine& line, int row, const QString& fallbackSfen)
{
    UpdateParams params;
    if (line.isEmpty()) {
        const QString sfen = SfenUtils::normalizePositionLikeSfen(fallbackSfen);
        params.position = (sfen.isEmpty() || SfenUtils::isHirateStart(sfen))
            ? QStringLiteral("position startpos")
            : QStringLiteral("position sfen ") + sfen;
        return params;
    }

    const int ply = qBound(0, row, line.moveCount());
    params.position = line.positionCommand(ply);
    // 読み筋の1手目が直前の手と同じマスなら「同」と書くので、その手の移動先を渡す
    params.previousTo = line.destinationOf(ply);
    params.lastUsiMove = line.usiMove(ply).toString();
    return params;
}
