#ifndef STARTGAMEDATABRIDGE_H
#define STARTGAMEDATABRIDGE_H

/// @file startgamedatabridge.h
/// @brief StartGameDialog から game 層へデータを渡す DTO 構造体

#include <QString>
#include <QList>

/**
 * @brief StartGameDialog から対局開始に必要なすべてのデータを保持する値型構造体
 *
 * dialogs/ 層の StartGameDialog への依存を game/ 層から排除するため、
 * ダイアログの入力値をフラットな値型として保持する。
 * 変換は app/ 層（GameSessionOrchestrator 等）でのみ行い、
 * game/ 層は本構造体のみを受け取る。
 */
struct StartGameDialogData {
    // --- 対局者情報 ---
    bool isHuman1  = false;    ///< 先手が人間か
    bool isHuman2  = false;    ///< 後手が人間か
    bool isEngine1 = false;    ///< 先手がエンジンか
    bool isEngine2 = false;    ///< 後手がエンジンか

    QString engineName1;       ///< 先手エンジン表示名
    QString engineName2;       ///< 後手エンジン表示名
    QString humanName1;        ///< 先手人間対局者名
    QString humanName2;        ///< 後手人間対局者名
    int engineNumber1 = 0;     ///< 先手エンジンインデックス
    int engineNumber2 = 0;     ///< 後手エンジンインデックス

    // --- エンジンリスト ---
    struct Engine {
        QString name;  ///< エンジン表示名
        QString path;  ///< エンジン実行ファイルのパス
    };
    QList<Engine> engineList;  ///< 登録エンジン一覧

    // --- 持ち時間設定 ---
    int basicTimeHour1    = 0;  ///< 先手持ち時間（時間）
    int basicTimeMinutes1 = 0;  ///< 先手持ち時間（分）
    int byoyomiSec1       = 0;  ///< 先手秒読み（秒）
    int addEachMoveSec1   = 0;  ///< 先手フィッシャー加算（秒）

    int basicTimeHour2    = 0;  ///< 後手持ち時間（時間）
    int basicTimeMinutes2 = 0;  ///< 後手持ち時間（分）
    int byoyomiSec2       = 0;  ///< 後手秒読み（秒）
    int addEachMoveSec2   = 0;  ///< 後手フィッシャー加算（秒）

    bool isLoseOnTimeout = false;  ///< 時間切れ負けフラグ

    // --- 開始局面 ---
    int startingPositionNumber = 1;  ///< 開始局面番号（0=現在局面, 1..N=手合割）

    // --- 対局オプション ---
    int     maxMoves          = 0;      ///< 最大手数（0=無制限）
    bool    autoSaveKifu      = false;  ///< 棋譜自動保存フラグ
    QString kifuSaveDir;               ///< 棋譜保存ディレクトリ
    bool    isShowHumanInFront = false; ///< 人間を手前に表示するか
    int     consecutiveGames  = 1;     ///< 連続対局数
    bool    isSwitchTurnEachGame = false; ///< 1局ごとに手番入替フラグ
    int     jishogiRule       = 0;     ///< 持将棋ルール（0:なし, 1:24点法, 2:27点法）
};

#endif // STARTGAMEDATABRIDGE_H
