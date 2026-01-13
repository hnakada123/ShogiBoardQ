#ifndef PLAYMODE_H
#define PLAYMODE_H

enum class PlayMode {
    NotStarted,        // ソフトウェアを起動した直後、まだ対局を開始していない状態
    HumanVsHuman,      // 平手、駒落ち Player1: Human, Player2: Human
    EvenHumanVsEngine,     // 平手 Player1: Human, Player2: USI Engine
    EvenEngineVsHuman,     // 平手 Player1: USI Engine, Player2: Human
    EvenEngineVsEngine,    // 平手、駒落ち Player1: USI Engine, Player2: USI Engine
    HandicapEngineVsHuman, // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    HandicapHumanVsEngine,  // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    HandicapEngineVsEngine, // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    AnalysisMode,      // 棋譜解析モード
    ConsiderationMode, // 検討モード
    TsumiSearchMode,   // 詰将棋探索モード
    CsaNetworkMode,    // CSA通信対局モード
    PlayModeError  // エラーコード
};

#endif // PLAYMODE_H
