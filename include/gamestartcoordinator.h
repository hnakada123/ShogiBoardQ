#ifndef GAMESTARTCOORDINATOR_H
#define GAMESTARTCOORDINATOR_H

#include <QObject>
#include <QString>
#include <QDialog>

// 既存の司令塔
#include "matchcoordinator.h"

class QWidget;
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

    // ★ MainWindow から「開始前の適用（時計/人手前など）」を依頼するための軽量入力
    struct Request {
        int       mode = 0;           // PlayMode を int で受ける（enum 依存を避ける）
        QString   startSfen;          // "startpos ..." or "<sfen> [b|w] ..."
        bool      bottomIsP1 = true;  // 「人を手前」初期希望
        QWidget*  startDialog = nullptr; // 持ち時間UI（QWidgetなら型不問：objectNameとproperty参照）
        ShogiClock* clock = nullptr;     // 任意（参照のみ）
    };

    struct Ctx {
        ShogiView*             view = nullptr;
        ShogiGameController*   gc = nullptr;
        ShogiClock*            clock = nullptr;
        QDialog*               startDlg = nullptr;
        QString*               startSfenStr = nullptr;
        QString*               currentSfenStr = nullptr;

        // 追加（元MainWindowの状態を渡すため）
        int   selectedPly = -1;       // 例: qMax(0, m_currentMoveIndex)
        bool  resumeFromCurrent = true;

        // ★ 追加：棋譜欄モデル／SFEN履歴（MainWindow が持っていたものを注入）
        KifuRecordListModel* kifuModel  = nullptr;
        QStringList*         sfenRecord = nullptr;

        // 追加：この関数で使う情報
        bool                 isReplayMode = false;     // 再生モード中は時計を動かさない

        // MainWindow 側へ初期msを戻したい場合（NULLなら無視）
        qint64*                 initialTimeP1MsOut = nullptr;
        qint64*                 initialTimeP2MsOut = nullptr;
    };


public:
    explicit GameStartCoordinator(const Deps& deps, QObject* parent = nullptr);

    // メイン API: StartOptions を受け取り対局を開始
    void start(const StartParams& params);

    // 開始前の時計/表示ポリシー適用（MainWindow::startGameBasedOnMode 内から呼ぶ）
    void prepare(const Request& req);

    // ★ 段階実行 API（MainWindow から逐次呼び出し可能）
    void prepareDataCurrentPosition(const Ctx& c);
    void prepareInitialPosition(const Ctx& c);
    void initializeGame(const Ctx& c);
    void setTimerAndStart(const Ctx& c);

    // ダイアログ状態を読み、対局モードを返す（MainWindow::setPlayMode の移管）
    PlayMode setPlayMode(const Ctx& c) const;

signals:
    // （開始前フック）UI/状態を初期化してほしい
    void requestPreStartCleanup();

    // 時計適用の依頼（MainWindow 側の既存ロジックへ接続してください）
    void requestApplyTimeControl(const GameStartCoordinator::TimeControl& tc);

    // 互換用エイリアス（どちらか一方だけ接続してください：ラムダ不要）
    void applyTimeControlRequested(const GameStartCoordinator::TimeControl& tc);

    // 進捗通知
    void willStart(const MatchCoordinator::StartOptions& opt);
    void started(const MatchCoordinator::StartOptions& opt);
    void startFailed(const QString& reason);

    void requestUpdateTurnDisplay();

private:
    bool validate_(const StartParams& params, QString& whyNot) const;

    // ダイアログからの抽出ヘルパ（型非依存：objectName + property 読み）
    static int  readIntProperty (const QObject* root, const char* objectName,
                               const char* prop = "value",   int  def = 0);
    static bool readBoolProperty(const QObject* root, const char* objectName,
                                 const char* prop = "checked", bool def = false);
    static TimeControl extractTimeControlFromDialog(const QWidget* dlg);

private:
    MatchCoordinator*    m_match = nullptr;
    ShogiClock*          m_clock = nullptr;
    ShogiGameController* m_gc    = nullptr;
    ShogiView*           m_view  = nullptr;

    // 対局モード判定（MainWindow から移管）
    PlayMode determinePlayMode(int initPositionNumber,
                               bool isPlayer1Human,
                               bool isPlayer2Human) const;

signals:
    // エラー表示を UI に委譲（MainWindow::displayErrorMessage 相当）
    void requestDisplayError(const QString& message) const;
};

Q_DECLARE_METATYPE(GameStartCoordinator::TimeControl)
Q_DECLARE_METATYPE(GameStartCoordinator::Request)

#endif // GAMESTARTCOORDINATOR_H
