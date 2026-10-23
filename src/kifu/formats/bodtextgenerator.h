#ifndef BODTEXTGENERATOR_H
#define BODTEXTGENERATOR_H

/// @file bodtextgenerator.h
/// @brief BOD形式テキスト生成クラスの定義

#include <QString>

#include "shogiposition.h"

/**
 * @brief BOD形式（盤面テキスト表示）の棋譜テキストを生成するクラス
 *
 * 局面からBOD形式の盤面テキストを生成する。
 * BOD形式は盤面・持ち駒・手番・手数を日本語テキストで表現する形式。
 */
class BodTextGenerator
{
public:
    /**
     * @brief 局面からBOD形式のテキストを生成
     * @param position 局面
     * @param moveIndex 手数インデックス（0なら初期局面）
     * @param lastMoveStr 最終手の表示文字列（空でも可）
     * @param handicapNames true なら持駒・手番の見出しを駒落ちの「下手」「上手」で書く
     *                      （false は「先手」「後手」）
     * @return BOD形式のテキスト
     */
    static QString generate(const Position& position, int moveIndex, const QString& lastMoveStr,
                            bool handicapNames = false);
};

#endif // BODTEXTGENERATOR_H
