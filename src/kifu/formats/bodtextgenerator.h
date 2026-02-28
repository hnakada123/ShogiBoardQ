#ifndef BODTEXTGENERATOR_H
#define BODTEXTGENERATOR_H

/// @file bodtextgenerator.h
/// @brief BOD形式テキスト生成クラスの定義

#include <QString>

/**
 * @brief BOD形式（盤面テキスト表示）の棋譜テキストを生成するクラス
 *
 * SFEN文字列からBOD形式の盤面テキストを生成する。
 * BOD形式は盤面・持ち駒・手番・手数を日本語テキストで表現する形式。
 */
class BodTextGenerator
{
public:
    /**
     * @brief SFEN文字列からBOD形式のテキストを生成
     * @param sfenStr SFEN形式の局面文字列
     * @param moveIndex 手数インデックス（0なら初期局面）
     * @param lastMoveStr 最終手の表示文字列（空でも可）
     * @return BOD形式のテキスト（空文字列の場合は生成失敗）
     */
    static QString generate(const QString& sfenStr, int moveIndex, const QString& lastMoveStr);
};

#endif // BODTEXTGENERATOR_H
