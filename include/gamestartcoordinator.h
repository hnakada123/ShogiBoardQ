#ifndef GAMESTARTCOORDINATOR_H
#define GAMESTARTCOORDINATOR_H

#include <QObject>
#include <QString>

// 既存の司令塔
#include "matchcoordinator.h"

class ShogiClock;
class ShogiGameController;
class ShogiView;

/**
 * @brief 対局開始を司るコーディネータ
 *
 * 責務:
 *  - （MainWindow 等で構築済みの）StartOptions を受け取り、対局を開始する
 *  - 将棋時計の初期化を “依頼シグナル” で通知（具体的なセットは UI 層に委譲）
 *  - 対局開始前の UI/内部状態クリアを “依頼シグナル” で通知
 *  - 必要に応じて初手 go（エンジン初手）をトリガ
 *
 * 注意:
 *  - ここでは具体的な ShogiClock API を呼びません（プロジェクト差異を吸収するため）
 *    → MainWindow 側で既存ロジックをスロット接続してください（ラムダ禁止）
 */
class GameStartCoordinator : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        MatchCoordinator*     match  = nullptr;  // 必須
        ShogiClock*           clock  = nullptr;  // 任意（時間適用リクエストを出すだけ）
        ShogiGameController*  gc     = nullptr;  // 任意（開始前の状態同期で使う場合）
        ShogiView*            view   = nullptr;  // 任意（開始前の表示更新で使う場合）
    };

    // 時間設定（必要分だけ。単位はミリ秒）
    struct TimeSide {
        qint64 baseMs      = 0;  // 持ち時間
        qint64 byoyomiMs   = 0;  // 秒読み
        qint64 incrementMs = 0;  // フィッシャー（1手加算）
    };
    struct TimeControl {
        bool    enabled = false;
        TimeSide p1;
        TimeSide p2;
    };

    // 起動パラメータ
    struct StartParams {
        MatchCoordinator::StartOptions opt;     // 既存の StartOptions をそのまま注入
        TimeControl                    tc;      // 時計適用のために併送
        bool autoStartEngineMove = true;        // 先手がエンジンなら初手 go を起動
    };

public:
    explicit GameStartCoordinator(const Deps& deps, QObject* parent = nullptr);

    // メイン API: StartOptions を受け取り対局を開始
    void start(const StartParams& params);

signals:
    // （開始前フック）UI/状態を初期化してほしい
    void requestPreStartCleanup();

    // 時計適用の依頼（MainWindow 側の既存ロジックへ接続してください）
    void requestApplyTimeControl(const GameStartCoordinator::TimeControl& tc);

    // 進捗通知
    void willStart(const MatchCoordinator::StartOptions& opt);
    void started(const MatchCoordinator::StartOptions& opt);
    void startFailed(const QString& reason);

private:
    bool validate_(const StartParams& params, QString& whyNot) const;

private:
    MatchCoordinator*    m_match = nullptr;
    ShogiClock*          m_clock = nullptr;
    ShogiGameController* m_gc    = nullptr;
    ShogiView*           m_view  = nullptr;
};

#endif // GAMESTARTCOORDINATOR_H
