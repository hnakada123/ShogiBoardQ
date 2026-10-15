#ifndef CONSIDERATIONWIRING_H
#define CONSIDERATIONWIRING_H

/// @file considerationwiring.h
/// @brief 検討モード配線クラスの定義


#include <QObject>
#include <QString>

#include "playmode.h"

class QWidget;
class ConsiderationTabManager;
class EngineInfoWidget;
class ShogiView;
class MatchCoordinator;
class DialogCoordinator;
class ShogiEngineThinkingModel;
class UsiCommLogModel;
class ConsiderationModeUIController;
class KifuRecordListModel;
struct ShogiMove;

/**
 * @brief 検討モード関連のUI配線を担当するクラス
 *
 * 責務:
 * - ConsiderationModeUIControllerの初期化と管理
 * - 検討モードの開始/停止フロー
 * - エンジン設定変更ダイアログ
 * - 矢印表示の制御
 */
class ConsiderationWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;
        ConsiderationTabManager* considerationTabManager = nullptr;
        EngineInfoWidget* thinkingInfo1 = nullptr;
        ShogiView* shogiView = nullptr;
        MatchCoordinator* match = nullptr;
        DialogCoordinator* dialogCoordinator = nullptr;
        ShogiEngineThinkingModel* considerationModel = nullptr;
        UsiCommLogModel* commLogModel = nullptr;
        const PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
    };

    explicit ConsiderationWiring(const Deps& deps, QObject* parent = nullptr);
    ~ConsiderationWiring() override = default;

    void updateDeps(const Deps& deps);

    /// ダイアログの司令塔を渡す（互いを参照するので、両方を作った後に MainWindowComposer が呼ぶ）
    void setDialogCoordinator(DialogCoordinator* dc);

    /// MatchCoordinator（起動時に一度だけ作る）を渡す。司令塔より先にこのクラスができた場合に使う
    void setMatchCoordinator(MatchCoordinator* mc);

    ConsiderationModeUIController* uiController() const { return m_uiController; }

public slots:
    /**
     * @brief 検討を開始する（検討タブの設定を使用）
     */
    void displayConsiderationDialog();

    /**
     * @brief 検討タブからのエンジン設定リクエスト
     */
    void onEngineSettingsRequested(int engineNumber, const QString& engineName);

    /**
     * @brief 検討中のエンジン変更
     */
    void onEngineChanged(int engineIndex, const QString& engineName);

    /**
     * @brief 検討モード開始時の初期化
     */
    void onModeStarted();

    /**
     * @brief 検討モードの時間設定確定時
     */
    void onTimeSettingsReady(bool unlimited, int byoyomiSec);

    /**
     * @brief 検討ダイアログでMultiPVが設定されたとき
     */
    void onDialogMultiPVReady(int multiPV);

    /**
     * @brief 検討中にMultiPV変更要求
     */
    void onMultiPVChangeRequested(int value);

    /**
     * @brief 矢印表示チェックボックスの状態変更時
     */
    void onShowArrowsChanged(bool checked);

public:

private slots:
    /**
     * @brief 検討中止要求を処理する（エンジン停止を実行）
     */
    void handleStopRequest();

private:
    void ensureUIController();

    QWidget* m_parentWidget = nullptr;
    ConsiderationTabManager* m_considerationTabManager = nullptr;
    EngineInfoWidget* m_thinkingInfo1 = nullptr;
    ShogiView* m_shogiView = nullptr;
    MatchCoordinator* m_match = nullptr;
    DialogCoordinator* m_dialogCoordinator = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    UsiCommLogModel* m_commLogModel = nullptr;
    const PlayMode* m_playMode = nullptr;
    QString* m_currentSfenStr = nullptr;

    ConsiderationModeUIController* m_uiController = nullptr;
};

#endif // CONSIDERATIONWIRING_H
