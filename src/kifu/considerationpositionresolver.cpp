/// @file considerationpositionresolver.cpp
/// @brief 検討モード向け局面解決ロジックの実装

#include "considerationpositionresolver.h"

#include "sfenutils.h"

UsiPosition ConsiderationPositionResolver::resolve(const RecordLine& line, int row, const QString& fallbackSfen)
{
    if (line.isEmpty()) {
        const QString sfen = SfenUtils::normalizePositionLikeSfen(fallbackSfen);
        return UsiPosition::parse(sfen.isEmpty() ? QStringLiteral("startpos") : sfen).value_or(UsiPosition{});
    }
    // 開始局面からの手順で送る。読み筋の1手目が直前の手と同じマスなら「同」と書く
    return line.usiPosition(qBound(0, row, line.moveCount()));
}
