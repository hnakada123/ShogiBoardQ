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
    /**
     * @brief エンジン操作用のコールバック群
     *
     * 検討/詰将棋探索のライフサイクル（開始・停止）で MC のエンジン管理と
     * UI 操作を行うために使用する。
     *
     * @note MC::ensureAnalysisSession() で lambda 経由で設定される。
     *       showGameOverDialog は MC::Hooks からのパススルー。
     * @see MatchCoordinator::ensureAnalysisSession
     */
    struct Hooks {
        /// @brief 終局/結果ダイアログを表示する
        /// @note 配線元: MC→m_hooks.showGameOverDialog (パススルー)
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;

        /// @brief エンジンを破棄する（思考モデルは保持）
        /// @note 配線元: MC lambda → ELM::destroyEnginesKeepModels
        std::function<void()> destroyEnginesKeepModels;

        // --- startFullAnalysis / stopFullAnalysis 用 ---

        /// @brief アプリケーションがシャットダウン中かどうかを返す
        /// @note 配線元: MC lambda → m_isShutdownInProgress
        std::function<bool()> isShutdownInProgress;

        /// @brief プレイモードを設定する
        /// @note 配線元: MC lambda → m_playMode 書き換え
        std::function<void(PlayMode)> setPlayMode;

        /// @brief 全エンジンを破棄する（モデル含む）
        /// @note 配線元: MC lambda → ELM::destroyEnginesAll
        std::function<void()> destroyEnginesAll;

        /// @brief 解析用エンジンを生成する（Usi オブジェクトの作成・モデル設定・シグナル配線を含む）
        /// @note 配線元: MC lambda (複合ロジック: Usi 生成 → comm/think モデル設定 → エラー/投了/勝ち配線)
        std::function<Usi*(const MatchCoordinator::AnalysisOptions&)> createAnalysisEngine;

        /// @brief エンジンを初期化して通信を開始する
        /// @note 配線元: MC lambda → ELM::initializeAndStartEngineFor
        std::function<void(int, const QString&, const QString&)> initAndStartEngine;

        /// @brief エンジン名をUIに設定する
        /// @note 配線元: MC lambda → MC::Hooks::setEngineNames 呼び出し
        std::function<void(const QString&, const QString&)> setEngineNames;

        /// @brief シャットダウン中フラグを設定する
        /// @note 配線元: MC lambda → m_isShutdownInProgress 書き換え
        std::function<void(bool)> setShutdownInProgress;
    };

    explicit AnalysisSessionHandler(QObject* parent = nullptr);

    /// コールバック群を設定する
    void setHooks(const Hooks& hooks);

    // --- 検討フルライフサイクル ---

    /// 検討を開始する（エンジン生成からASH配線まで一括）
    bool startFullAnalysis(const MatchCoordinator::AnalysisOptions& opt);

    /// 検討エンジンを停止する（フラグ管理含む）
    void stopFullAnalysis();

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
