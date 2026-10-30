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

class AnalysisSession;
class AppActivityState;
class BoardArrowActions;
class EngineSlots;
class HumanTurnPolicy;
class JosekiBookSession;
class MatchCoordinator;
class ShogiEngineThinkingModel;
class QAction;
class Usi;

/**
 * @brief 検討と対局のエンジンの読みと、定跡ウィンドウで開いている定跡の定跡手を盤の矢印にする
 *
 * タブの表示状態には依存しない。どの矢印を出すかは BoardArrowActions の QAction だけを読む。
 * - 検討: 検討の表の各候補手の読み筋の最初の手
 * - 対局: エンジンごとの思考の表の、最新の最善の読み筋（multipv 1）の最初の手。探索の局面が表示中の局面なら
 *   手番側（赤の実線）、表示中の局面に予想した手（その局面に至った手）を指した局面なら先読み側（青の破線）
 * - 定跡手（緑）: エンジンの矢印の下に描き、出現頻度の高い手ほど濃くする
 */
class CandidateArrowPresenter : public QObject
{
    Q_OBJECT
public:
    /// 矢印の元になる対局と検討（無いものは描かない）
    struct Deps {
        ShogiView* view = nullptr;                     ///< 矢印を描く盤（必須）
        MatchCoordinator* match = nullptr;             ///< 終局したか（無ければ動作状態だけで決める）
        EngineSlots* engines = nullptr;                ///< 対局のエンジンの枠（探索中か・先読み中か）
        /// 枠 E1・E2 のエンジンの思考の表（対局の矢印の元）
        std::array<const ShogiEngineThinkingModel*, 2> thinking{};
        AppActivityState* activity = nullptr;          ///< 対局中か・終局したか
        AnalysisSession* analysis = nullptr;           ///< 検討中か（正本はセッション）
        ShogiEngineThinkingModel* consideration = nullptr; ///< 検討の読み筋（検討中の矢印の元）
        /// 矢印を出すかの切り替え（無ければ対局・定跡手の矢印は出さず、検討の矢印は出す）
        const BoardArrowActions* arrows = nullptr;
        const JosekiBookSession* josekiBook = nullptr; ///< 定跡手の矢印の元（定跡ウィンドウで開いている定跡）
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
    /// 矢印の切り替え toggle がオンか（切り替えが無ければ fallback）
    bool arrowsOn(QAction* (BoardArrowActions::*toggle)() const, bool fallback) const;
    void appendConsiderationArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen) const;
    /// 対局のエンジンの読みの矢印を足す（先読み側なら m_ponderDescription に予想した手を書く）
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
