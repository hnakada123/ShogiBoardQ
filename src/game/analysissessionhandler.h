#ifndef ANALYSISSESSIONHANDLER_H
#define ANALYSISSESSIONHANDLER_H

/// @file analysissessionhandler.h
/// @brief 検討・詰み探索セッション管理ハンドラクラスの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

#include "matchcoordinator.h"

class Usi;
class ShogiEngineThinkingModel;

/**
 * @brief 検討モード・詰み探索モードのセッション管理ハンドラ
 *
 * MatchCoordinator から検討モード・詰み探索モード関連の状態管理と
 * ライフサイクル処理を分離する。エンジンの bestmove 受信時の
 * 再開制御、MultiPV 変更、局面変更を管理する。
 */
class AnalysisSessionHandler : public QObject
{
    Q_OBJECT

public:
    /// エンジン操作用のコールバック群
    struct Hooks {
        /// 終局/結果ダイアログ表示
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;
        /// エンジン破棄（思考内容保持）
        std::function<void()> destroyEnginesKeepModels;
    };

    explicit AnalysisSessionHandler(QObject* parent = nullptr);

    /// コールバック群を設定する
    void setHooks(const Hooks& hooks);

    // --- モード状態クエリ ---

    /// 検討モード中かどうかを返す
    bool isInConsiderationMode() const { return m_inConsiderationMode; }

    /// 詰み探索モード中かどうかを返す
    bool isInTsumeSearchMode() const { return m_inTsumeSearchMode; }

    // --- startAnalysis から呼び出されるセットアップ ---

    /**
     * @brief モード固有の配線と状態保存を行う
     *
     * MatchCoordinator::startAnalysis() のエンジン生成・初期化後に呼ばれ、
     * 検討/詰み探索モード固有のシグナル接続と状態保存を行う。
     *
     * @param engine 生成済みエンジン（非所有）
     * @param opt 検討オプション
     */
    void setupModeSpecificWiring(Usi* engine, const MatchCoordinator::AnalysisOptions& opt);

    /**
     * @brief モードに応じた解析通信を開始する
     *
     * @param engine 生成済みエンジン（非所有）
     * @param opt 検討オプション
     */
    void startCommunication(Usi* engine, const MatchCoordinator::AnalysisOptions& opt);

    // --- stopAnalysisEngine から呼び出される停止処理 ---

    /// 検討/詰み探索のモードフラグをリセットし、終了シグナルを発火する
    void handleStop();

    // --- 検討API（MatchCoordinator から委譲） ---

    /**
     * @brief 検討中にMultiPVを変更する
     * @param engine エンジンポインタ（非所有）
     * @param multiPV 新しいMultiPV値
     */
    void updateMultiPV(Usi* engine, int multiPV);

    /**
     * @brief 検討中にポジションを変更する
     * @param engine エンジンポインタ（非所有）
     * @param newPositionStr 新しいposition文字列
     * @param previousFileTo 前回の移動先の筋
     * @param previousRankTo 前回の移動先の段
     * @param lastUsiMove 最後の指し手（USI形式）
     * @return true: 再開処理を開始した
     */
    bool updatePosition(Usi* engine, const QString& newPositionStr,
                        int previousFileTo = 0, int previousRankTo = 0,
                        const QString& lastUsiMove = QString());

    // --- エラー/エンジン破棄時のリセット ---

    /**
     * @brief エンジンエラー時の復旧処理
     * @param showDialog ダイアログ表示コールバック
     * @param errorMsg エラーメッセージ
     * @return true: 検討/詰み探索モードのエラーを処理した
     */
    bool handleEngineError(const QString& errorMsg);

    /// destroyEngines() から呼ばれるフラグリセット（シグナル発火付き）
    void resetOnDestroyEngines();

signals:
    /// 検討モード終了時に発火する
    void considerationModeEnded();

    /// 詰み探索モード終了時に発火する
    void tsumeSearchModeEnded();

    /// 検討待機開始時に発火する（経過タイマー停止用）
    void considerationWaitingStarted();

private slots:
    void onCheckmateSolved(const QStringList& pv);
    void onCheckmateNoMate();
    void onCheckmateNotImplemented();
    void onCheckmateUnknown();
    void onTsumeBestMoveReceived();
    void onConsiderationBestMoveReceived();
    void restartConsiderationDeferred();

private:
    /// 詰み探索完了の共通処理（フラグリセット＋シグナル発火＋ダイアログ＋エンジン破棄）
    void finalizeTsumeSearch(const QString& resultMessage);

    Hooks m_hooks;                                ///< コールバック群

    // --- エンジン参照 ---
    Usi* m_engine = nullptr;                      ///< 現在のエンジン（非所有）

    // --- モードフラグ ---
    bool m_inTsumeSearchMode = false;             ///< 詰み探索モード中か
    bool m_inConsiderationMode = false;           ///< 検討モード中か

    // --- 検討モード状態 ---
    QString m_positionStr;                        ///< 検討中のposition文字列
    int m_byoyomiMs = 0;                          ///< 検討の時間制限（ms）
    int m_multiPV = 1;                            ///< 検討の候補手数
    ShogiEngineThinkingModel* m_modelPtr = nullptr; ///< 検討タブ用モデル（非所有）
    bool m_restartPending = false;                ///< 検討再開待ちフラグ
    bool m_restartInProgress = false;             ///< 検討再開処理中フラグ（再入防止）
    bool m_waiting = false;                       ///< 検討待機中フラグ
    QString m_enginePath;                         ///< 検討中のエンジンパス
    QString m_engineName;                         ///< 検討中のエンジン名
    int m_previousFileTo = 0;                     ///< 前回の移動先の筋
    int m_previousRankTo = 0;                     ///< 前回の移動先の段
    QString m_lastUsiMove;                        ///< 最後の指し手（USI形式）
};

#endif // ANALYSISSESSIONHANDLER_H
