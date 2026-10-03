#ifndef CANDIDATEARROWCONTROLLER_H
#define CANDIDATEARROWCONTROLLER_H

#include "shogiview.h"
#include <QPointer>
#include <QTimer>
#include <optional>
#include <array>

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
    void setMatchCoordinator(MatchCoordinator* match);
    void setConsiderationState(bool active, bool show, ShogiEngineThinkingModel* model);

    static std::optional<ShogiView::Arrow> arrowForMove(const QString& move,
                                                      const QString& baseSfen, int priority);
    bool considerationActive() const;
    QString ponderDescription() const { return m_ponderDescription; }

public slots:
    void scheduleRefresh();
    void refresh();

signals:
    void displayStateChanged();

private slots:
    void bindEngines();
    void endConsideration();

private:
    void appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen);
    QPointer<ShogiView> m_view;
    QPointer<MatchCoordinator> m_match;
    std::array<QPointer<Usi>, 2> m_engines;
    QPointer<ShogiEngineThinkingModel> m_considerationModel;
    bool m_considerationActive = false;
    bool m_showConsideration = true;
    QString m_ponderDescription;
    QTimer m_refreshTimer;
};
#endif
