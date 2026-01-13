#ifndef CSAGAMEWIRING_H
#define CSAGAMEWIRING_H

#include <QObject>
#include <QString>
#include <QPoint>
#include <QVector>

#include "csagamecoordinator.h"
#include "shogimove.h"

class ShogiGameController;
class ShogiView;
class KifuRecordListModel;
class RecordPane;
class BoardInteractionController;
class QStatusBar;
class CsaGameDialog;
class CsaWaitingDialog;
class EngineAnalysisTab;
class BoardSetupController;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class TimeControlController;

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
        QVector<ShogiMove>* gameMoves = nullptr;
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
    void appendKifuLineRequested(const QString& text, const QString& elapsedTime);

    /**
     * @brief 分岐ツリー更新を要求
     */
    void refreshBranchTreeRequested();

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

public slots:
    /**
     * @brief 待機キャンセル時の処理
     */
    void onWaitingCancelled();

private slots:
    /**
     * @brief 対局開始時の処理
     * @param blackName 先手名
     * @param whiteName 後手名
     */
    void onGameStarted(const QString& blackName, const QString& whiteName);

    /**
     * @brief 対局終了時の処理
     * @param result 結果
     * @param cause 終了原因
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void onGameEnded(const QString& result, const QString& cause, int consumedTimeMs);

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
     * @brief 経過時間フォーマットを生成
     * @param consumedTimeMs 消費時間（ミリ秒）
     * @param totalTimeMs 累計消費時間（ミリ秒）
     * @return フォーマットされた時間文字列
     */
    QString formatElapsedTime(int consumedTimeMs, int totalTimeMs) const;

    /**
     * @brief 終局行のテキストを生成
     * @param cause 終局原因
     * @param loserIsBlack 敗者が先手かどうか
     * @return 終局行テキスト
     */
    QString buildEndLineText(const QString& cause, bool loserIsBlack) const;

    // 依存オブジェクト
    CsaGameCoordinator* m_coordinator = nullptr;
    ShogiGameController* m_gameController = nullptr;
    ShogiView* m_shogiView = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
    RecordPane* m_recordPane = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    QStatusBar* m_statusBar = nullptr;
    QStringList* m_sfenRecord = nullptr;
    // CSA対局開始用の追加依存オブジェクト
    EngineAnalysisTab* m_analysisTab = nullptr;
    BoardSetupController* m_boardSetupController = nullptr;
    UsiCommLogModel* m_usiCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;
    TimeControlController* m_timeController = nullptr;
    QVector<ShogiMove>* m_gameMoves = nullptr;

    // 内部状態
    int m_activePly = 0;
    int m_currentSelectedPly = 0;
    bool m_coordinatorOwnedByThis = false;  // コーディネータの所有権フラグ
};

#endif // CSAGAMEWIRING_H
