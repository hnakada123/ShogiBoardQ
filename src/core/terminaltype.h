#ifndef TERMINALTYPE_H
#define TERMINALTYPE_H

/// @file terminaltype.h
/// @brief 終局手の種類と、棋譜に書く終局語の対応表

#include <QString>
#include <QStringView>

/**
 * @brief 終局手の種類
 */
enum class TerminalType {
    None,           ///< 通常の指し手（終局手ではない）
    Resign,         ///< 投了
    Checkmate,      ///< 詰み
    Repetition,     ///< 千日手
    PerpetualCheck, ///< 王手千日手（棋譜ファイルの終局語。対局では王手を続けた側の反則として記録する）
    Impasse,        ///< 持将棋
    Timeout,        ///< 時間切れ
    IllegalWin,     ///< 反則勝ち
    IllegalLoss,    ///< 反則負け
    Forfeit,        ///< 不戦敗
    Interrupt,      ///< 中断
    NoCheckmate,    ///< 不詰（詰将棋用）
    DeclarationWin, ///< 入玉宣言勝ち
    Draw,           ///< 引き分け
    MaxMoves,       ///< 最大手数到達
    Error,          ///< エラー
    ForfeitWin,     ///< 不戦勝
    Try,            ///< トライルールによる勝ち
    Custom          ///< 独自の終局理由（原文は RecordEntry が保持）
};

/**
 * @brief 終局語の対応表（この表だけが終局の種類と日本語の語を結ぶ）
 *
 * 棋譜の行・読み込み・表示の訳は、すべてこの表を通す。
 */
namespace TerminalTypes {

/// 棋譜の行に書く終局語（翻訳しない。None は空）
QString label(TerminalType type);

/// 表示用の終局語（表示言語に訳す。None は空）
QString displayLabel(TerminalType type);

/// 終局語を読む。棋譜ファイルの別表記（「切れ負け」「詰」「宣言勝ち」「最大手数」など）も受ける。
/// 前後の空白は無視し、語全体が一致しなければ None
TerminalType fromWord(QStringView word);

/// 文中の終局語を探す（長い語を先に調べる。「不詰」を「詰」と取り違えない）。
/// 見つかれば種類を返し、matched にその語を入れる。なければ None
TerminalType findWord(QStringView text, QString* matched = nullptr);

} // namespace TerminalTypes

#endif // TERMINALTYPE_H
