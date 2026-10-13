/// @file shogiutils.cpp
/// @brief 将棋関連の共通ユーティリティ関数群の実装

#include "shogiutils.h"
#include "shoginotation.h"
#include "shogimove.h"
#include "errorbus.h"
#include "logcategories.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QStringList>
#include "shogiboard.h"
#include <QObject>

namespace ShogiUtils {

// ============================================================
// 座標表記変換
// ============================================================

// ============================================================
// USI変換
// ============================================================

// ============================================================
// 漢字座標解析（逆変換）
// ============================================================

std::optional<std::pair<int, int>> parseMoveLabel(QStringView moveLabel)
{
    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = moveLabel.indexOf(senteMark);
    if (markPos < 0) {
        markPos = moveLabel.indexOf(goteMark);
    }
    if (markPos < 0 || moveLabel.size() <= markPos + 2) {
        return std::nullopt;
    }

    const QStringView afterMark = moveLabel.mid(markPos + 1);

    // 「同」で始まる場合は前の手を参照する必要がある
    if (afterMark.startsWith(QStringLiteral("同"))) {
        return std::nullopt;
    }

    const QChar fileChar = afterMark.at(0);
    const QChar rankChar = afterMark.at(1);

    const int file = ShogiNotation::fileFromZenkaku(fileChar);
    const int rank = ShogiNotation::rankFromKanji(rankChar);

    if (file >= 1 && file <= 9 && rank >= 1 && rank <= 9) {
        return std::make_pair(file, rank);
    }

    return std::nullopt;
}

std::optional<std::pair<int, int>> parseMoveCoordinateFromModel(
    const QAbstractItemModel* model, int row)
{
    if (!model || row <= 0 || row >= model->rowCount()) {
        return std::nullopt;
    }

    const QModelIndex idx = model->index(row, 0);
    const QString moveLabel = model->data(idx, Qt::DisplayRole).toString();

    if (auto coord = parseMoveLabel(moveLabel)) {
        return coord;
    }

    // 「同」の場合は前の手を遡って座標を取得
    for (int i = row - 1; i > 0; --i) {
        const QModelIndex prevIdx = model->index(i, 0);
        const QString prevLabel = model->data(prevIdx, Qt::DisplayRole).toString();

        if (auto coord = parseMoveLabel(prevLabel)) {
            return coord;
        }
    }

    return std::nullopt;
}

} // namespace ShogiUtils
