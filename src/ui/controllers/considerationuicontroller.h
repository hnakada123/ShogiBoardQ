#ifndef CONSIDERATIONUICONTROLLER_H
#define CONSIDERATIONUICONTROLLER_H

/// @file considerationuicontroller.h
/// @brief 検討タブの操作（開始・中止・エンジン設定・条件の変更）を検討のセッションへ伝える

#include <QObject>
#include <QString>

#include "considerationsettings.h"
#include "enginelistsettings.h"

class QWidget;
class ConsiderationTabManager;
class AnalysisSession;
class KifuNavigationState;

/**
 * @brief 検討タブの操作を検討のセッションへ伝える
 *
 * - 開始は検討タブの条件（ConsiderationTabManager::settings()）を読んで、表示中の局面で検討のセッションを始める
 * - 検討中に条件が変わったら、エンジンが変われば始め直し、候補手の数が変われば探索をやり直させる
 * - 検討の状態の表示（ボタン・経過時間・情報欄）は EngineDisplayPresenter、矢印は CandidateArrowPresenter が受け持つ
 */
class ConsiderationUiController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                             ///< エンジン設定ダイアログの親
        ConsiderationTabManager* considerationTabManager = nullptr;  ///< 検討タブ（必須、非所有）
        AnalysisSession* session = nullptr;                          ///< 検討のセッション（必須、非所有）
        const KifuNavigationState* navState = nullptr;               ///< 検討する局面（表示中の手）
    };

    explicit ConsiderationUiController(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 検討タブの条件で、表示中の局面の検討を始める
    void startConsideration();
    /// 検討タブからのエンジン設定の要求
    void onEngineSettingsRequested(const EngineListSettings::EngineEntry& engine);
    /// 検討タブの条件が変わった（検討中ならエンジン・候補手の数の変更を反映する）
    void onSettingsChanged(const ConsiderationSettings& settings);

private slots:
    void stopConsideration();

private:
    QWidget* m_parentWidget = nullptr;
    ConsiderationTabManager* m_considerationTabManager = nullptr;
    AnalysisSession* m_session = nullptr;
    const KifuNavigationState* m_navState = nullptr;
};

#endif // CONSIDERATIONUICONTROLLER_H
