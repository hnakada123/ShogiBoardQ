#ifndef SHOGIUTILS_H
#define SHOGIUTILS_H

/// @file shogiutils.h
/// @brief 将棋関連の共通ユーティリティ関数群の定義

#include <QString>
#include <QElapsedTimer>
#include <QStringView>
#include <optional>
#include <utility>

class QAbstractItemModel;
struct ShogiMove;

/// 将棋の座標変換・指し手解析等の共通ユーティリティ
namespace ShogiUtils {

    // --- 座標表記変換 ---

    /// 段番号（1〜9）を漢字表記（"一"〜"九"）に変換する
    QString transRankTo(const int rankTo);

    /// 筋番号（1〜9）を全角数字表記（"１"〜"９"）に変換する
    QString transFileTo(const int fileTo);

    // --- エラーハンドリング ---

    /// エラーメッセージをログ出力し、例外をスローする
    void logAndThrowError(const QString& errorMessage);

    // --- USI変換 ---

    /**
     * @brief ShogiMoveをUSI形式の文字列に変換する
     * @param move 変換する指し手
     * @return USI形式の文字列（例: "7g7f", "8h2b+", "P*3d"）
     *
     * 座標系:
     * - move.fromSquare/toSquare は0-indexed (0-8)
     * - 駒台: fromSquare.x() == 9 (先手駒台) or 10 (後手駒台)
     * - USI形式: file=1-9, rank=a-i
     */
    QString moveToUsi(const ShogiMove& move);

    // --- 漢字座標解析（逆変換） ---

    /**
     * @brief 全角数字「１」〜「９」を1-9に変換する
     * @param ch 全角数字文字
     * @return 1-9、無効な場合は0
     */
    int parseFullwidthFile(QChar ch);

    /**
     * @brief 漢数字「一」〜「九」を1-9に変換する
     * @param ch 漢数字文字
     * @return 1-9、無効な場合は0
     */
    int parseKanjiRank(QChar ch);

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

    // --- 対局タイマー ---

    /// 新規対局の開始時にエポックタイマーをリセットする
    void startGameEpoch();

    /// 対局開始からの経過時間をミリ秒で返す（モノトニック）
    qint64 nowMs();
}

#endif // SHOGIUTILS_H
