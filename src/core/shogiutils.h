#ifndef SHOGIUTILS_H
#define SHOGIUTILS_H

/// @file shogiutils.h
/// @brief 将棋関連の共通ユーティリティ関数群の定義

#include <QString>
#include <QStringView>
#include <optional>
#include <utility>

class QAbstractItemModel;
struct ShogiMove;

/// 将棋の座標変換・指し手解析等の共通ユーティリティ
namespace ShogiUtils {


    // --- エラーハンドリング ---

    /// エラーメッセージをログ出力し、例外をスローする
    void logAndThrowError(const QString& errorMessage);


    // --- 漢字座標解析（逆変換） ---


    /**
     * @brief 指し手ラベルから移動先座標を解析する
     * @param moveLabel 指し手ラベル（例: "▲７六歩(77)", "△同　銀(31)"）
     * @return {筋, 段} (各1-9)、「同」または解析失敗時は std::nullopt
     *
     * 「同」で始まる場合は nullopt を返し、呼び出し側で前の手を参照する必要がある
     */
    [[nodiscard]] std::optional<std::pair<int, int>> parseMoveLabel(QStringView moveLabel);

    /**
     * @brief 棋譜モデルから指定行の移動先座標を解析する（「同」の場合は遡る）
     * @param model 棋譜モデル（Qt::DisplayRoleで指し手ラベルを返すこと）
     * @param row 解析対象の行インデックス
     * @return {筋, 段} (各1-9)、解析失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<std::pair<int, int>> parseMoveCoordinateFromModel(
        const QAbstractItemModel* model, int row);
}

#endif // SHOGIUTILS_H
