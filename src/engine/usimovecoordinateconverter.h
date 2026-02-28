#ifndef USIMOVECOORDINATECONVERTER_H
#define USIMOVECOORDINATECONVERTER_H

/// @file usimovecoordinateconverter.h
/// @brief USI座標変換ユーティリティの定義

#include <QChar>
#include <QMap>
#include <QPoint>
#include <QString>
#include <optional>

/**
 * @brief USI形式の座標・指し手変換を行うユーティリティクラス
 *
 * 責務:
 * - USI座標文字（'a'〜'i'）と段番号（1〜9）の相互変換
 * - 駒文字と駒台段番号の相互変換
 * - USI形式の指し手文字列の座標パース
 * - 人間操作の座標からUSI指し手文字列への変換
 *
 * 全メソッドがstaticで、状態を持たない純粋なユーティリティクラス。
 */
class UsiMoveCoordinateConverter
{
public:
    static constexpr int SENTE_HAND_FILE = 10;
    static constexpr int GOTE_HAND_FILE = 11;
    static constexpr int BOARD_SIZE = 9;

    // --- 座標変換 ---

    /// 段番号（1〜9）をUSIアルファベット（'a'〜'i'）に変換
    static QChar rankToAlphabet(int rank);

    /// USIアルファベット（'a'〜'i'）を段番号（1〜9）に変換
    [[nodiscard]] static std::optional<int> alphabetToRank(QChar c);

    // --- 駒変換 ---

    /// 先手の駒台段番号を駒打ち文字列（例: "P*"）に変換
    static QString convertFirstPlayerPieceSymbol(int rankFrom);

    /// 後手の駒台段番号を駒打ち文字列（例: "R*"）に変換
    static QString convertSecondPlayerPieceSymbol(int rankFrom);

    /// 駒文字を後手側の駒台段番号に変換
    static int pieceToRankWhite(QChar c);

    /// 駒文字を先手側の駒台段番号に変換
    static int pieceToRankBlack(QChar c);

    // --- 指し手パース ---

    /// USI指し手文字列の移動元座標をパースする
    /// @param move USI形式の指し手（例: "7g7f", "P*5e"）
    /// @param fileFrom [out] 移動元の筋
    /// @param rankFrom [out] 移動元の段
    /// @param isFirstPlayer 先手番かどうか（駒打ち時の駒台判定に使用）
    /// @param errorMsg [out] エラー時のメッセージ
    /// @return 成功時true、エラー時false
    [[nodiscard]] static bool parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom,
                              bool isFirstPlayer, QString& errorMsg);

    /// USI指し手文字列の移動先座標をパースする
    /// @param move USI形式の指し手（4文字以上）
    /// @param fileTo [out] 移動先の筋
    /// @param rankTo [out] 移動先の段
    /// @param errorMsg [out] エラー時のメッセージ
    /// @return 成功時true、エラー時false
    [[nodiscard]] static bool parseMoveTo(const QString& move, int& fileTo, int& rankTo,
                            QString& errorMsg);

    /// 人間の盤面操作をUSI形式の指し手文字列に変換する
    /// @param from 移動元の座標（x:筋、y:段）
    /// @param to 移動先の座標
    /// @param promote 成りフラグ
    /// @param errorMsg [out] エラー時のメッセージ
    /// @return USI形式の指し手文字列（エラー時は空文字列）
    static QString convertHumanMoveToUsi(const QPoint& from, const QPoint& to,
                                         bool promote, QString& errorMsg);

private:
    static const QMap<int, QString>& firstPlayerPieceMap();
    static const QMap<int, QString>& secondPlayerPieceMap();
    static const QMap<QChar, int>& pieceRankWhiteMap();
    static const QMap<QChar, int>& pieceRankBlackMap();
};

#endif // USIMOVECOORDINATECONVERTER_H
