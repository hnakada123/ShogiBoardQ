#ifndef BOARDINTERACTIONCONTROLLER_H
#define BOARDINTERACTIONCONTROLLER_H

/// @file boardinteractioncontroller.h
/// @brief 盤面上のクリック・ドラッグ操作とハイライト管理を担うコントローラ

#include <QObject>
#include <QPoint>
#include <QColor>
#include <QLoggingCategory>
#include <functional>
#include "shogiview.h"

Q_DECLARE_LOGGING_CATEGORY(lcBoard)

class ShogiGameController;

/**
 * @brief 盤面上のマウス操作（駒選択・移動要求・成/不成トグル）とハイライト描画を制御する
 * @todo remove コメントスタイルガイド適用済み
 *
 * 2クリック方式の駒移動、選択ハイライト（オレンジ）、直前手ハイライト（赤/黄）を管理する。
 * 人間の手番判定はコールバックで外部から注入できる。
 */
class BoardInteractionController : public QObject
{
    Q_OBJECT
public:
    // --- モード定義 ---
    enum class Mode {
        HumanVsHuman, ///< 対人対局
        HumanVsEngine, ///< 対エンジン対局
        Edit ///< 局面編集
    };

    explicit BoardInteractionController(ShogiView* view,
                                        ShogiGameController* gc,
                                        QObject* parent = nullptr);

    void setMode(Mode m) { m_mode = m; }
    Mode mode() const { return m_mode; }

    // --- 人間の手番判定コールバック ---
    /// trueを返すと人間の手番、falseなら相手の手番としてクリックを無視する
    using IsHumanTurnCallback = std::function<bool()>;
    void setIsHumanTurnCallback(IsHumanTurnCallback cb) { m_isHumanTurnCb = std::move(cb); }

    // --- ハイライト操作 ---

    /// 選択中（オレンジ）のみを消し、直前手の赤/黄は残す
    void clearSelectionHighlight();

    /// 局面移動等で直前手ハイライトだけを反映する
    void showMoveHighlights(const QPoint& from, const QPoint& to);

    /// すべてのハイライトを削除する
    void clearAllHighlights();

public slots:
    /// 左クリック: 駒選択→移動先指定の2クリック処理
    void onLeftClick(const QPoint& pt);

    /// 右クリック: 選択キャンセルまたは成/不成トグル（編集モード時）
    void onRightClick(const QPoint& pt);

    /// 着手適用結果の通知を受けてハイライトを更新する
    void onMoveApplied(const QPoint& from, const QPoint& to, bool success);

    /// ShogiView::highlightsCleared() からの通知でポインタをnullにする（ダングリング防止）
    void onHighlightsCleared();

signals:
    void moveRequested(const QPoint& from, const QPoint& to); ///< 着手要求（→ MainWindow / Coordinator）
    void selectionChanged(const QPoint& pos); ///< 駒選択変更の通知

private:
    // --- ハイライト系ヘルパ ---
    void selectPieceAndHighlight(const QPoint& field);
    void updateHighlight(ShogiView::FieldHighlight*& hl, const QPoint& field, const QColor& color);
    void deleteHighlight(ShogiView::FieldHighlight*& hl);
    void addNewHighlight(ShogiView::FieldHighlight*& hl, const QPoint& pos, const QColor& color);
    void resetSelectionAndHighlight();
    void finalizeDrag();
    void togglePiecePromotionOnClick(const QPoint& field);

    static constexpr int kBlackStandFile = 10; ///< 先手駒台のファイル番号
    static constexpr int kWhiteStandFile = 11; ///< 後手駒台のファイル番号

    // --- メンバ変数 ---
    ShogiView* m_view = nullptr; ///< 盤面ビュー（非所有）
    ShogiGameController* m_gc = nullptr; ///< ゲームコントローラ（非所有）

    Mode m_mode = Mode::HumanVsHuman; ///< 現在の操作モード
    IsHumanTurnCallback m_isHumanTurnCb; ///< 人間の手番判定コールバック

    // --- クリック状態 ---
    QPoint m_clickPoint; ///< 現在選択中のマス座標
    bool   m_waitingSecondClick = false; ///< 2クリック目待ちフラグ
    QPoint m_firstClick; ///< 1クリック目の座標

    // --- ハイライトポインタ ---
    ShogiView::FieldHighlight* m_selectedField  = nullptr; ///< 選択中（オレンジ）
    ShogiView::FieldHighlight* m_selectedField2 = nullptr; ///< 直前の移動元（赤）
    ShogiView::FieldHighlight* m_movedField     = nullptr; ///< 移動先（黄）
};

#endif // BOARDINTERACTIONCONTROLLER_H
