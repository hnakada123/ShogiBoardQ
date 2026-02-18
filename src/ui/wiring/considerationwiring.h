#ifndef CONSIDERATIONWIRING_H
#define CONSIDERATIONWIRING_H

/// @file considerationwiring.h
/// @brief 検討モード配線クラスの定義


#include <QObject>
#include <QString>
#include <functional>

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
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        std::function<void()> ensureDialogCoordinator;  // 遅延初期化コールバック
    };

    explicit ConsiderationWiring(const Deps& deps, QObject* parent = nullptr);
    ~ConsiderationWiring() override = default;

    void updateDeps(const Deps& deps);

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
     * @brief 検討モデル更新時に矢印を更新
     */
    void updateArrows();

    /**
     * @brief 矢印表示チェックボックスの状態変更時
     */
    void onShowArrowsChanged(bool checked);

    /**
     * @brief 検討モード中に局面が変更されたときの処理
     */
    bool updatePositionIfNeeded(int row, const QString& newPosition,
                                const QVector<ShogiMove>* gameMoves,
                                KifuRecordListModel* kifuRecordModel);

signals:
    /**
     * @brief 検討中止が要求されたとき
     */
    void stopRequested();

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
    PlayMode* m_playMode = nullptr;
    QString* m_currentSfenStr = nullptr;
    std::function<void()> m_ensureDialogCoordinator;

    ConsiderationModeUIController* m_uiController = nullptr;
};

#endif // CONSIDERATIONWIRING_H
