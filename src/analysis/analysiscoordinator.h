#ifndef ANALYSISCOORDINATOR_H
#define ANALYSISCOORDINATOR_H

/// @file analysiscoordinator.h
/// @brief 局面解析コーディネータクラスの定義


#include <QObject>
#include <QStringList>
#include <QList>
#include <QTimer>
#include <limits>

#include "usiinfo.h"

class BranchTreeManager;

/**
 * @brief 指定局面群のUSI解析実行を管理するコーディネータ
 *
 * 各手数に対応する局面コマンド列を順に処理し、エンジンからの
 * `info`/`bestmove` を受けて解析結果を通知する。
 * エンジンへの送信は用途ごとの要求シグナル（requestSetOption / requestSearch / requestStop）へ委譲し、
 * 実際の送信実装とは疎結合に保つ。info は解析済みの UsiInfo で受け取る。
 *
 */
class AnalysisCoordinator : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        QStringList* sfenRecord = nullptr;  ///< 各plyの`position ...`コマンド列（非所有）
    };

    /// 解析オプション
    struct Options {
        int  startPly   = 0;     ///< 解析開始手数（0以上）
        int  endPly     = -1;    ///< 解析終了手数（-1は末尾まで）
        int  movetimeMs = 1000;  ///< 1局面あたりの思考時間（ms）
        int  multiPV    = 1;     ///< MultiPVの本数（`setoption`で設定）
        bool centerTree = true;  ///< 進捗時に分岐ツリーをセンタリングするか
    };

    /**
     * @brief 解析実行モード
     *
     */
    enum Mode {
        Idle,            ///< 待機中
        SinglePosition,  ///< 単一局面解析
        RangePositions   ///< 指定範囲の連続解析
    };
    Q_ENUM(Mode)

    explicit AnalysisCoordinator(const Deps& d, QObject* parent = nullptr);

    /// 依存オブジェクトを更新する
    void setDeps(const Deps& d);

    /// 分岐ツリーマネージャー参照を設定する（非所有）
    void setBranchTreeManager(BranchTreeManager* manager);

    /// 解析オプションを設定する
    void setOptions(const Options& opt);

    // --- 操作 API ---

    /// 設定済み範囲（startPly..endPly）を連続解析する
    void startAnalyzeRange();

    /// 指定手数の局面を1回だけ解析する
    /// @param ply 解析対象の手数
    void startAnalyzeSingle(int ply);

    /// 現在の解析を中断する
    void stop();

    /// 現在解析中の手数を返す
    int currentPly() const { return m_currentPly; }

signals:
    /// setoption の送信を要求する（→ Usi::sendSetOptionCommand）
    void requestSetOption(const QString& name, const QString& value);

    /// 局面の探索開始（position + go infinite）を要求する（→ Usi::sendPositionAndGoInfinite）
    void requestSearch(const QString& positionCommand);

    /// 探索の停止を要求する（→ Usi::sendStopCommand）
    void requestStop();

    /// 解析終了を通知する（→ AnalysisFlowController::onAnalysisFinished）
    void analysisFinished(AnalysisCoordinator::Mode mode);

    /// 解析進捗を通知する（→ AnalysisFlowController::onAnalysisProgress）
    void analysisProgress(int ply, const UsiInfo& info);

    /// 解析対象局面の準備完了を通知する（→ AnalysisFlowController::onPositionPrepared）
    /// GUI更新後にgo送信するための2段階通知
    void positionPrepared(int ply, const QString& positionCmd);

public:
    // --- AnalysisFlowController から呼ばれるメソッド ---

    /// エンジンの info を受け取る（AnalysisFlowController::onInfoReceivedから呼出）
    void onEngineInfo(const UsiInfo& info);

    /// エンジンの`bestmove ...`を受け取る（AnalysisFlowController::onBestMoveReceivedから呼出）
    void onEngineBestmoveReceived(const QString& line);

    /// 局面表示更新完了後に`position`/`go`を送信する（AnalysisFlowController::onPositionPreparedから呼出）
    void sendGoCommand();

private:
    Deps    m_deps;                  ///< 依存オブジェクト
    Options m_opt;                   ///< 現在の解析オプション

    Mode m_mode = Idle;              ///< 現在の解析モード
    bool m_running = false;          ///< 解析実行中フラグ
    int  m_currentPly = -1;          ///< 現在解析中の手数
    QString m_pendingPosCmd;         ///< `sendGoCommand()`待機中の`position`コマンド

    BranchTreeManager* m_branchTreeManager = nullptr;  ///< 分岐ツリーマネージャー参照（非所有）
    QTimer m_stopTimer;                         ///< `go infinite`後に`stop`送信するタイマー

    // --- 内部処理 ---
    void startRange();
    void startSingle(int ply);
    void nextPlyOrFinish();
    void sendAnalyzeForPly(int ply);

private slots:
    /// stopタイマー満了時に`stop`を送信する
    void onStopTimerTimeout();
};

#endif // ANALYSISCOORDINATOR_H
