#ifndef CANDIDATEARROWCONTROLLER_H
#define CANDIDATEARROWCONTROLLER_H

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
class CandidateArrowController : public QObject
{
    Q_OBJECT
public:
    static CandidateArrowController* forView(ShogiView* view);
    explicit CandidateArrowController(ShogiView* view);
    /// 矢印の元になる対局・エンジン・動作状態・検討のセッション（非所有）
    struct Sources {
        MatchCoordinator* match = nullptr;
        EngineLifecycleManager* engines = nullptr;
        AppActivityState* activity = nullptr;
        AnalysisSession* analysis = nullptr;
    };
    void setSources(const Sources& sources);
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
    Sources m_sources;
    std::array<QPointer<Usi>, 2> m_engines;
    QPointer<ShogiEngineThinkingModel> m_considerationModel;
    bool m_considerationActive = false;
    bool m_showConsideration = true;
    QString m_ponderDescription;
    QTimer m_refreshTimer;
};
#endif
