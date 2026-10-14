#ifndef CSAGAMEWIRING_H
#define CSAGAMEWIRING_H

/// @file csagamewiring.h
/// @brief CSA通信対局配線クラスの定義


#include <QObject>
#include "recordentry.h"
#include <QString>
#include <QPoint>
#include <QList>
#include <functional>

#include "csagamecoordinator.h"
#include "shogimove.h"
#include "playmode.h"

class ShogiGameController;
class ShogiView;
class KifuRecordListModel;
class RecordPane;
class BoardInteractionController;
class QStatusBar;
class CsaGameDialog;
class EngineAnalysisTab;
class BoardSetupController;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class TimeControlController;
class UiStatePolicyManager;
class GameRecordUpdateService;
class UiNotificationService;

/**
 * @brief CSA通信対局とMainWindowの間のUI配線を担当するクラス
 *
 * 責務:
 * - CsaGameCoordinatorからのシグナルを受け取り、UIを更新する
 * - 棋譜欄・盤面表示・ステータスバーなどのUI更新を一元管理
 * - MainWindowの肥大化を防ぐため、CSA関連の配線処理を分離
 *
 * 本クラスはCsaGameCoordinatorのシグナルを受け取り、
 * 適切なUI要素に反映させる役割を担う。
 */
class CsaGameWiring : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        CsaGameCoordinator* coordinator = nullptr;
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        RecordPane* recordPane = nullptr;
        BoardInteractionController* boardController = nullptr;
        QStatusBar* statusBar = nullptr;
        QStringList* sfenRecord = nullptr;
        // 以下はCSA対局開始用の追加依存オブジェクト
        EngineAnalysisTab* analysisTab = nullptr;
        BoardSetupController* boardSetupController = nullptr;
        UsiCommLogModel* usiCommLog = nullptr;
        ShogiEngineThinkingModel* engineThinking = nullptr;
        TimeControlController* timeController = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        // PlayMode への直接書き込み用
        PlayMode* playMode = nullptr;
        // 親ウィジェット（ダイアログ表示用）
        QWidget* parentWidget = nullptr;
        std::function<void(const QString&, const QString&, const QString&)> prepareRecord;
        std::function<void(int)> syncPly;
        /// 終局日時を記録して対局情報に反映する
        std::function<void()> recordGameEnd;
        /// 棋譜を指定パスへ保存する（拡張子で形式を決める）。失敗時は理由を返す
        std::function<bool(const QString&, QString*)> saveKifuToPath;
        /// 盤の反転表示を指定の向きにする（後手のときに自分側から見る）
        std::function<void(bool)> setBoardFlipped;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit CsaGameWiring(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~CsaGameWiring() override = default;

    /**
     * @brief シグナル配線を行う
     *
     * CsaGameCoordinatorのシグナルとUIスロットを接続する。
     */
    void wire();

    /**
     * @brief シグナル配線を解除する
     */
    void unwire();

    /**
     * @brief コーディネータを設定する（遅延初期化用）
     * @param coordinator CSA通信対局コーディネータ
     */
    void setCoordinator(CsaGameCoordinator* coordinator);

    /**
     * @brief CSA対局を開始する
     * @param dialog 設定済みのCsaGameDialog
     * @param parent 親ウィンドウ（待機ダイアログ用）
     * @return 対局開始が成功したらtrue
     *
     * この関数は以下を行う:
     * - CsaGameCoordinatorの初期化（未作成の場合）
     * - シグナル配線
     * - 対局開始オプションの設定
     * - 待機ダイアログの表示
     */
    bool startCsaGame(CsaGameDialog* dialog, QWidget* parent);

    /**
     * @brief CsaGameCoordinatorを取得
     * @return CsaGameCoordinator*
     */
    CsaGameCoordinator* coordinator() const { return m_coordinator; }

    /**
     * @brief EngineAnalysisTabを設定
     * @param tab EngineAnalysisTab
     */
    void setAnalysisTab(EngineAnalysisTab* tab);

    /**
     * @brief BoardSetupControllerを設定
     * @param controller BoardSetupController
     */
    void setBoardSetupController(BoardSetupController* controller);

    /**
     * @brief 外部シグナルを UiStatePolicyManager / GameRecordUpdateService / UiNotificationService に接続する
     * @param uiPolicy UI状態ポリシーマネージャ
     * @param recordService 棋譜追記・ライブ更新サービス
     * @param notifService エラー通知サービス
     */
    void wireExternalSignals(UiStatePolicyManager* uiPolicy,
                             GameRecordUpdateService* recordService,
                             UiNotificationService* notifService);

signals:
    /**
     * @brief ナビゲーション無効化を要求
     */
    void disableNavigationRequested();

    /**
     * @brief ナビゲーション有効化を要求
     */
    void enableNavigationRequested();

    /**
     * @brief 棋譜行追加を要求
     * @param text 棋譜テキスト
     * @param elapsedTime 経過時間
     */
    void recordAppendRequested(const RecordEntry& entry);

    /**
     * @brief プレイモード変更を要求
     * @param mode 新しいプレイモード
     */
    void playModeChanged(int mode);

    /**
     * @brief 対局終了ダイアログ表示を要求
     * @param title タイトル
     * @param message メッセージ
     */
    void showGameEndDialogRequested(const QString& title, const QString& message);

    /**
     * @brief エラーメッセージ表示を要求
     * @param message エラーメッセージ
     */
    void errorMessageRequested(const QString& message);

    /**
     * @brief 盤面に着手が反映された（相手の指し手・自エンジンの指し手）
     *
     * CsaGameCoordinator::moveAppliedToBoard の転送シグナル。
     * 駒音再生（PieceSoundPlayer）などコーディネータの寿命に依存しない受け手向け。
     */
    void moveAppliedToBoard();

public slots:
    /**
     * @brief 待機キャンセル時の処理
     */
    void onWaitingCancelled();

private slots:
    /// PlayMode変更を内部で処理する（MainWindowへの転送不要）
    void onPlayModeChangedInternal(int mode);

    /// 対局終了ダイアログを内部で表示する（MainWindowへの転送不要）
    void showGameEndDialogInternal(const QString& title, const QString& message);

    /**
     * @brief 対局開始時の処理
     * @param blackName 先手名
     * @param whiteName 後手名
     * @param initialPrettyMoves Game_Summary に含まれる既存手順
     */
    void onGameStarted(const QString& blackName, const QString& whiteName,
                       const QStringList& initialPrettyMoves);

    /**
     * @brief 対局終了時の処理
     * @param result 結果
     * @param cause 終了原因
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);

    /**
     * @brief 指し手確定時の処理
     * @param csaMove CSA形式の指し手
     * @param usiMove USI形式の指し手
     * @param prettyMove 表示用の指し手
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void onMoveMade(const QString& csaMove, const QString& usiMove,
                    const QString& prettyMove, int consumedTimeMs);

    /**
     * @brief 手番変更時の処理
     * @param isMyTurn 自分の手番かどうか
     */
    void onTurnChanged(bool isMyTurn);

    /**
     * @brief ログメッセージ受信時の処理
     * @param message メッセージ
     * @param isError エラーかどうか
     */
    void onLogMessage(const QString& message, bool isError);

    /**
     * @brief 指し手ハイライト要求時の処理
     * @param from 移動元座標
     * @param to 移動先座標
     */
    void onMoveHighlightRequested(const QPoint& from, const QPoint& to);

private:
    /**
     * @brief 終局行のテキストを生成
     * @param cause 終局原因
     * @param loserIsBlack 敗者が先手かどうか
     * @param blackToMove 終局した局面で先手の手番かどうか
     * @return 終局行テキスト
     */
    RecordEntry buildEndRecord(CsaClient::GameEndCause cause, bool loserIsBlack, bool isDraw,
                             bool blackToMove) const;

    void appendInitialKifuLine(const QString& prettyMove, const RecordTime& time, const QString& sfen);

    /// 接続ダイアログの入力から対局開始オプションを組み立て、配線側の設定（自動保存・盤反転）も控える
    CsaGameCoordinator::StartOptions buildStartOptions(const CsaGameDialog* dialog);
    /// 終局した棋譜を自動保存先へ書き出す（設定が有効なとき）
    void autoSaveRecord();
    /// 後手のときに盤を反転する（設定が有効なとき）
    void applyAutoFlip();
    QString buildNumberedKifuLine(const QString& prettyMove) const;

    // 依存オブジェクト
    CsaGameCoordinator* m_coordinator = nullptr;
    ShogiGameController* m_gameController = nullptr;
    ShogiView* m_shogiView = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
    RecordPane* m_recordPane = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    QStatusBar* m_statusBar = nullptr;
    QStringList* m_sfenHistory = nullptr;
    // CSA対局開始用の追加依存オブジェクト
    EngineAnalysisTab* m_analysisTab = nullptr;
    BoardSetupController* m_boardSetupController = nullptr;
    UsiCommLogModel* m_usiCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;
    TimeControlController* m_timeController = nullptr;
    QList<ShogiMove>* m_gameMoves = nullptr;

    // PlayMode への直接書き込み用
    PlayMode* m_playMode = nullptr;
    // 親ウィジェット（ダイアログ表示用）
    QWidget* m_parentWidget = nullptr;
    GameRecordUpdateService* m_recordService = nullptr;
    std::function<void(const QString&, const QString&, const QString&)> m_prepareRecord;
    std::function<void(int)> m_syncPly;
    std::function<void()> m_recordGameEnd;
    std::function<bool(const QString&, QString*)> m_saveKifuToPath;
    std::function<void(bool)> m_setBoardFlipped;

    // 内部状態
    int m_activePly = 0;

    // 接続ダイアログの詳細設定（配線側で扱うもの）
    bool m_autoSaveEnabled = false;      ///< 終局時に棋譜を自動保存する
    QString m_autoSaveDir;               ///< 自動保存先フォルダ
    QString m_autoSaveFormat = QStringLiteral("kif"); ///< 自動保存の形式（拡張子）
    bool m_autoFlipBoard = true;         ///< 後手のときは盤を反転する
};

#endif // CSAGAMEWIRING_H
