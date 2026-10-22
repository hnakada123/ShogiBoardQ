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
class EngineLifecycleManager;
class JosekiBookSession;
class MatchCoordinator;
class ShogiEngineThinkingModel;
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
        EngineLifecycleManager* engines = nullptr;     ///< 読みを出すエンジンの枠
        AppActivityState* activity = nullptr;          ///< 対局中か・終局したか
        AnalysisSession* analysis = nullptr;           ///< 検討中か（正本はセッション）
        ShogiEngineThinkingModel* consideration = nullptr; ///< 検討の読み筋（検討中の矢印の元）
        /// 検討の候補手の矢印を出すか（検討タブの設定）。無ければ出す
        std::function<bool()> showConsiderationArrows;
        const JosekiBookSession* josekiBook = nullptr; ///< 定跡手の矢印の元（定跡ウィンドウで開いている定跡）
        /// 今、定跡手の矢印を出してよいか（定跡ウィンドウで表示を止めていない・対局中なら人間の手番）。無ければ出す
        std::function<bool()> josekiArrowsAllowed;
    };

    explicit CandidateArrowPresenter(const Deps& deps, QObject* parent = nullptr);

    static std::optional<ShogiView::Arrow> arrowForMove(const QString& move,
                                                      const QString& baseSfen, int priority);
    bool considerationActive() const;
    QString ponderDescription() const { return m_ponderDescription; }

    bool josekiArrowsVisible() const { return m_josekiArrowsVisible; }

public slots:
    void scheduleRefresh();
    void refresh();
    /// 定跡手の矢印を出すか（「表示」メニューの「盤に定跡手を表示」。設定に保存する）
    void setJosekiArrowsVisible(bool visible);

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
    bool m_josekiArrowsVisible = false;
};
#endif
