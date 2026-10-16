#ifndef CANDIDATEARROWPRESENTER_H
#define CANDIDATEARROWPRESENTER_H

/// @file candidatearrowpresenter.h
/// @brief 検討と対局のエンジンの読みを盤上の矢印にする

#include "shogiview.h"
#include <QPointer>
#include <QTimer>
#include <optional>
#include <array>

class AnalysisSession;
class AppActivityState;
class EngineLifecycleManager;
class MatchCoordinator;
class ShogiEngineThinkingModel;
class Usi;

/// 検討と対局の候補矢印を一括管理する。タブの表示状態には依存しない。
class CandidateArrowPresenter : public QObject
{
    Q_OBJECT
public:
    /// 矢印の元になる対局と検討（無いものは描かない）
    struct Deps {
        ShogiView* view = nullptr;                     ///< 矢印を描く盤（必須）
        MatchCoordinator* match = nullptr;             ///< 対局中のエンジンの読み
        EngineLifecycleManager* engines = nullptr;     ///< 読みを出すエンジンの枠
        AppActivityState* activity = nullptr;          ///< 対局中か・終局したか
        AnalysisSession* analysis = nullptr;           ///< 検討が終わったら検討の矢印を消す
    };

    explicit CandidateArrowPresenter(const Deps& deps, QObject* parent = nullptr);
    void setConsiderationState(bool active, bool show, ShogiEngineThinkingModel* model);

    static std::optional<ShogiView::Arrow> arrowForMove(const QString& move,
                                                      const QString& baseSfen, int priority);
    bool considerationActive() const;
    QString ponderDescription() const { return m_ponderDescription; }

public slots:
    void scheduleRefresh();
    void refresh();

signals:
    /// 矢印の表示が変わった（検討中か・先読みの説明。思考タブの矢印の設定欄に出す）
    void displayStateChanged(bool considerationActive, const QString& ponderDescription);

private slots:
    void bindEngines();
    void endConsideration();

private:
    void appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen);
    QPointer<ShogiView> m_view;
    Deps m_sources;
    std::array<QPointer<Usi>, 2> m_engines;
    QPointer<ShogiEngineThinkingModel> m_considerationModel;
    bool m_considerationActive = false;
    bool m_showConsideration = true;
    QString m_ponderDescription;
    QTimer m_refreshTimer;
};
#endif
