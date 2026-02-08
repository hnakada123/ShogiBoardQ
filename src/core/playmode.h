#ifndef PLAYMODE_H
#define PLAYMODE_H

/// @file playmode.h
/// @brief 対局モード列挙型の定義

/**
 * @brief アプリケーションの対局モード
 *
 * 対局種別・解析種別を区別し、UIや通信の振る舞いを切り替える。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
enum class PlayMode {
    NotStarted,             ///< 起動直後、対局未開始
    HumanVsHuman,           ///< 人間 vs 人間（平手・駒落ち）
    EvenHumanVsEngine,      ///< 平手 先手:人間 vs 後手:エンジン
    EvenEngineVsHuman,      ///< 平手 先手:エンジン vs 後手:人間
    EvenEngineVsEngine,     ///< 平手・駒落ち エンジン vs エンジン
    HandicapEngineVsHuman,  ///< 駒落ち 下手:エンジン vs 上手:人間
    HandicapHumanVsEngine,  ///< 駒落ち 下手:人間 vs 上手:エンジン
    HandicapEngineVsEngine, ///< 駒落ち 下手:エンジン vs 上手:エンジン
    AnalysisMode,           ///< 棋譜解析モード
    ConsiderationMode,      ///< 検討モード
    TsumiSearchMode,        ///< 詰将棋探索モード
    CsaNetworkMode,         ///< CSA通信対局モード
    PlayModeError           ///< エラー（不正な設定値の検出用）
};

#endif // PLAYMODE_H
