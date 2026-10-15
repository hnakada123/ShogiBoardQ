#ifndef TERMINALTYPE_H
#define TERMINALTYPE_H
#include <QString>

/**
 * @brief 終局手の種類
 *
 */
enum class TerminalType {
    None,           ///< 通常の指し手（終局手ではない）
    Resign,         ///< 投了
    Checkmate,      ///< 詰み
    Repetition,     ///< 千日手
    PerpetualCheck, ///< 王手千日手（棋譜ファイルの終局語。対局では王手を続けた側の反則として記録する）
    Impasse,        ///< 持将棋
    Timeout,        ///< 切れ負け
    IllegalWin,     ///< 反則勝ち
    IllegalLoss,    ///< 反則負け
    Forfeit,        ///< 不戦敗
    Interrupt,      ///< 中断
    NoCheckmate,    ///< 不詰（詰将棋用）
    DeclarationWin, ///< 入玉宣言勝ち
    Draw,          ///< 引き分け
    MaxMoves,      ///< 最大手数到達
    Error,         ///< エラー
    ForfeitWin     ///< 不戦勝
};

/**
 * @brief 終局手の種類を文字列から検出する
 * @param displayText 表示テキスト（例: "△投了"）
 * @return 検出された終局手の種類
 */
TerminalType detectTerminalType(const QString& displayText);


#endif // TERMINALTYPE_H
