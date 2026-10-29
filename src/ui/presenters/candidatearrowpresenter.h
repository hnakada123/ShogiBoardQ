#ifndef CANDIDATEARROWPRESENTER_H
#define CANDIDATEARROWPRESENTER_H

/// @file candidatearrowpresenter.h
/// @brief 検討と対局のエンジンの読みと、定跡手を盤上の矢印にする

#include "shogiview.h"
#include <QPointer>
#include <QTimer>
#include <functional>
#include <optional>
#include <array>

struct ConsiderationSettings;
class AnalysisSession;
class AppActivityState;
class EngineSlots;
class HumanTurnPolicy;
class JosekiBookSession;
class MatchCoordinator;
class ShogiEngineThinkingModel;
class QAction;
class Usi;

/// 検討と対局の候補矢印と、定跡ウィンドウで開いている定跡の定跡手の矢印を一括管理する。タブの表示状態には依存しない。
/// 定跡手の矢印（緑）はエンジンの矢印の下に描き、比率の高い手ほど濃くする。
class CandidateArrowPresenter : public QObject
{
    Q_OBJECT
public:
    /// 矢印の元になる対局と検討（無いものは描かない）
    struct Deps {
        ShogiView* view = nullptr;                     ///< 矢印を描く盤（必須）
        MatchCoordinator* match = nullptr;             ///< 対局中のエンジンの読み
        EngineSlots* engines = nullptr;     ///< 読みを出すエンジンの枠
        AppActivityState* activity = nullptr;          ///< 対局中か・終局したか
        AnalysisSession* analysis = nullptr;           ///< 検討中か（正本はセッション）
        ShogiEngineThinkingModel* consideration = nullptr; ///< 検討の読み筋（検討中の矢印の元）
        /// 検討の候補手の矢印を出すか（検討タブの設定）。無ければ出す
        /// 検討の条件（検討タブの正本 ConsiderationPanel::settings()。showArrows で検討の矢印を出す。無ければいつでも出す）
        const ConsiderationSettings* considerationSettings = nullptr;
        const JosekiBookSession* josekiBook = nullptr; ///< 定跡手の矢印の元（定跡ウィンドウで開いている定跡）
        const QAction* josekiArrows = nullptr;         ///< 「盤に定跡手を表示」（無ければ定跡手の矢印を出さない）
        const QAction* josekiDisplay = nullptr;        ///< 定跡ウィンドウの定跡の表示（オフなら出さない。無ければ出す）
        const HumanTurnPolicy* humanTurn = nullptr;    ///< 対局中は人間の手番だけ出す（無ければいつでも出す）
    };

    explicit CandidateArrowPresenter(const Deps& deps, QObject* parent = nullptr);

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

private:
    void appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen);
    /// 表示中の局面 sfen（手数付き）の定跡手の矢印を足す
    void appendJosekiArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen) const;
    QPointer<ShogiView> m_view;
    Deps m_sources;
    std::array<QPointer<Usi>, 2> m_engines;
    QString m_ponderDescription;
    QTimer m_refreshTimer;
};
#endif
