#ifndef ENGINEDISPLAYPRESENTER_H
#define ENGINEDISPLAYPRESENTER_H

/// @file enginedisplaypresenter.h
/// @brief 対局・検討のエンジン（枠 E1・E2）の思考情報と通信ログの表示

#include <QObject>
#include <QPointer>
#include <array>

class AnalysisSession;
class EngineLifecycleManager;
class ShogiEngineThinkingModel;
class Usi;
class UsiCommLogModel;
class UsiModelBinding;

/**
 * @brief 枠 E1・E2 のエンジンを、思考ドック・検討ドック・USI通信ログの表示のモデルと結ぶ
 *
 * エンジンの枠（EngineLifecycleManager）にエンジンが入るたびに UsiModelBinding を作る。
 * 検討の間は E1 のエンジンの読み筋を検討の表にも出す（候補手の数は検討のセッションに合わせる）。
 */
class EngineDisplayPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EngineLifecycleManager* engines = nullptr;        ///< エンジンの枠（必須）
        AnalysisSession* analysis = nullptr;              ///< 検討のセッション
        UsiCommLogModel* commLog1 = nullptr;
        UsiCommLogModel* commLog2 = nullptr;
        ShogiEngineThinkingModel* thinking1 = nullptr;
        ShogiEngineThinkingModel* thinking2 = nullptr;
        ShogiEngineThinkingModel* consideration = nullptr; ///< 検討の表
    };

    explicit EngineDisplayPresenter(const Deps& deps, QObject* parent = nullptr);

private slots:
    void bindEngines();
    void clearDisplay();
    void onConsiderationSearchStarted();
    void onConsiderationModeEnded();
    void clearConsideration();

private:
    const Deps m_deps;
    std::array<QPointer<Usi>, 2> m_engines;               ///< 結んだエンジン（E1・E2）
    std::array<QPointer<UsiModelBinding>, 2> m_bindings;
};

#endif // ENGINEDISPLAYPRESENTER_H
