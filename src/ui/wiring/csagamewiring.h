#ifndef CSAGAMEWIRING_H
#define CSAGAMEWIRING_H

/// @file csagamewiring.h
/// @brief CSA通信対局の配線と画面への反映を担当するクラスの定義


#include <QObject>
#include <QString>
#include <QList>
#include <functional>

#include "csagamecoordinator.h"
#include "appactivitystate.h"

class ShogiGameController;
class ShogiView;
class BoardInteractionController;
class QStatusBar;
class CsaGameDialog;
class EngineAnalysisTab;
class BoardSetupController;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class TimeControlController;
class UiStatePolicyManager;
class MatchRecordSink;
class UiNotificationService;

/**
 * @brief CSA通信対局の配線と画面への反映を担当するクラス
 *
 * 責務:
 * - CsaGameCoordinator を最初の通信対局で作り、そのシグナルを実際の受け手へ直接つなぐ
 *   （棋譜行 → 棋譜の受け手（MatchRecordSink）、指し手ハイライト → BoardInteractionController、
 *    エラー → UiNotificationService、通信ログ → EngineAnalysisTab）
 * - 対局の開始・終局での画面の切り替え（プレイモード・UI状態・対局者名・盤の反転・
 *   ステータスバー・結果の表示・棋譜の自動保存）
 *
 * 指し手の進行・棋譜行の組み立て（終局原因の TerminalType への対応を含む）は
 * CsaGameCoordinator / CsaGameProgress が行い、ここでは扱わない。
 */
class CsaGameWiring : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        CsaGameCoordinator* coordinator = nullptr;   ///< 通常は空（最初の通信対局で作る）
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        BoardInteractionController* boardController = nullptr;
        QStatusBar* statusBar = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        BoardSetupController* boardSetupController = nullptr;
        UsiCommLogModel* usiCommLog = nullptr;
        ShogiEngineThinkingModel* engineThinking = nullptr;
        TimeControlController* timeController = nullptr;
        AppActivityState* activity = nullptr;        ///< 通信対局中かどうか（盤の入力の振り分けに使う）
        QWidget* parentWidget = nullptr;             ///< 結果ダイアログの親
        /// 棋譜と対局情報を開始局面・対局者名で作り直し、ライブ記録を始める
        std::function<void(const QString&, const QString&, const QString&)> prepareRecord;
        /// 終局日時を記録して対局情報に反映する
        std::function<void()> recordGameEnd;
        /// 棋譜を指定パスへ保存する（拡張子で形式を決める）。失敗時は理由を返す
        std::function<bool(const QString&, QString*)> saveKifuToPath;
        /// 盤の反転表示を指定の向きにする（後手のときに自分側から見る）
        std::function<void(bool)> setBoardFlipped;
    };

    explicit CsaGameWiring(const Dependencies& deps, QObject* parent = nullptr);
    ~CsaGameWiring() override = default;

    /**
     * @brief 司令塔のシグナルを受け手へつなぐ（何度呼んでも1回だけつながる）
     *
     * 受け手が未設定（nullptr）の接続は飛ばす。後から wireExternalSignals() などで
     * 受け手が決まったときはつなぎ直す。
     */
    void wire();

    /// wire() でつないだ接続をすべて外す
    void unwire();

    /// 司令塔を差し替える（前の司令塔の接続は外す）
    void setCoordinator(CsaGameCoordinator* coordinator);

    /**
     * @brief CSA対局を開始する
     * @param dialog 設定済みのCsaGameDialog
     * @param parent 親ウィンドウ（待機ダイアログ用）
     * @return 対局開始が成功したらtrue
     *
     * 司令塔が未作成なら作って配線し、待機ダイアログを表示して対局開始かエラーまで待つ。
     */
    bool startCsaGame(CsaGameDialog* dialog, QWidget* parent);

    /// 通信対局の司令塔（最初の通信対局まで nullptr）
    CsaGameCoordinator* coordinator() const { return m_coordinator; }

    void setAnalysisTab(EngineAnalysisTab* tab);
    void setBoardSetupController(BoardSetupController* controller);

    /**
     * @brief UI状態・棋譜・エラー通知の受け手を設定する
     * @param uiPolicy UI状態ポリシーマネージャ（対局中・待機の切り替え）
     * @param recordSink 棋譜の追記先（司令塔が確定した棋譜行を書く）
     * @param notifService エラー通知サービス（司令塔の errorOccurred を直接つなぐ）
     */
    void wireExternalSignals(UiStatePolicyManager* uiPolicy,
                             MatchRecordSink* recordSink,
                             UiNotificationService* notifService);

public slots:
    /**
     * @brief 待機キャンセル時の処理
     */
    void onWaitingCancelled();

private slots:
    void onRecordEntryReady(const RecordEntry& entry);
    void onGameStarted(const QString& blackName, const QString& whiteName,
                       const QList<RecordEntry>& initialRecords);
    void onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onTurnChanged(bool isMyTurn);
    void onLogMessage(const QString& message, bool isError);

private:
    void setActivity(AppActivityState::Activity activity);
    void showGameEndDialog(const QString& title, const QString& message);
    /// 接続ダイアログの入力から対局開始オプションを組み立て、配線側の設定（自動保存・盤反転）も控える
    CsaGameCoordinator::StartOptions buildStartOptions(const CsaGameDialog* dialog);
    /// 終局した棋譜を自動保存先へ書き出す（設定が有効なとき）
    void autoSaveRecord();
    /// 後手のときに盤を反転する（設定が有効なとき）
    void applyAutoFlip();

    // 依存オブジェクト
    CsaGameCoordinator* m_coordinator = nullptr;
    ShogiGameController* m_gameController = nullptr;
    ShogiView* m_shogiView = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    QStatusBar* m_statusBar = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    BoardSetupController* m_boardSetupController = nullptr;
    UsiCommLogModel* m_usiCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;
    TimeControlController* m_timeController = nullptr;
    AppActivityState* m_activity = nullptr;
    QWidget* m_parentWidget = nullptr;
    UiStatePolicyManager* m_uiPolicy = nullptr;
    MatchRecordSink* m_recordSink = nullptr;
    UiNotificationService* m_notifService = nullptr;
    std::function<void(const QString&, const QString&, const QString&)> m_prepareRecord;
    std::function<void()> m_recordGameEnd;
    std::function<bool(const QString&, QString*)> m_saveKifuToPath;
    std::function<void(bool)> m_setBoardFlipped;

    // 接続ダイアログの詳細設定（配線側で扱うもの）
    bool m_autoSaveEnabled = false;      ///< 終局時に棋譜を自動保存する
    QString m_autoSaveDir;               ///< 自動保存先フォルダ
    QString m_autoSaveFormat = QStringLiteral("kif"); ///< 自動保存の形式（拡張子）
    bool m_autoFlipBoard = true;         ///< 後手のときは盤を反転する
};

#endif // CSAGAMEWIRING_H
