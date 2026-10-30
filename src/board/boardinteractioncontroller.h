#ifndef BOARDINTERACTIONCONTROLLER_H
#define BOARDINTERACTIONCONTROLLER_H

/// @file boardinteractioncontroller.h
/// @brief 盤面上のクリック・ドラッグ操作とハイライト管理を担うコントローラ

#include <QObject>
#include <QPoint>
#include <functional>
#include <optional>
#include "shogiview.h"
#include "boardconstants.h"
#include "usimove.h"

class BoardInputRoute;
class ShogiBoard;

/**
 * @brief 盤面上のマウス操作（駒選択・移動先の指定・成/不成トグル）とハイライトを制御する
 *
 * 2クリック方式で移動元・移動先を決め、行き先（BoardInputRoute）へ渡す。つまめるか・局面編集中かは
 * 行き先に問い合わせる。選択・直前の手・合法な移動先のハイライトは盤の層（BoardHighlightLayer）に出す。
 * 移動元・移動先を手番の側の手にする処理（成り・不成の尋ね方を含む）もここに置き、主画面と詰将棋を
 * 解く画面の行き先が使う（moveFromClick）。
 */
class BoardInteractionController : public QObject
{
    Q_OBJECT
public:
    /// board は view が表示する盤（起動から終了まで同じ1つ）
    explicit BoardInteractionController(ShogiView* view, ShogiBoard* board, QObject* parent = nullptr);

    /// 盤で指した手の行き先（非所有。無ければクリックを受け付けない）
    void setInputRoute(BoardInputRoute* route);

    /// 成り・不成を対局者に選ばせる関数（成るなら true）。未設定なら成らない
    using PromotionChooser = std::function<bool()>;
    void setPromotionChooser(PromotionChooser chooser);

    /**
     * @brief 盤のクリック（表示の座標の移動元・移動先）を、手番の側の手にする（盤は変えない）
     *
     * 移動元は盤上の駒か、手番の側の駒台の駒（玉は打てない）。成り・不成のどちらが合法かで決め、
     * 両方とも合法なら成りの選び方で尋ねる（尋ねている間もドラッグの表示は保つ。終えるのは submit から
     * 戻った後）。指せない操作は nullopt。行き先（BoardInputRoute::submit）から呼ぶ
     */
    std::optional<UsiMove> moveFromClick(const QPoint& from, const QPoint& to) const;

    /// 選択した駒の合法な移動先を表示し、設定を保存する。
    void setLegalMovesVisible(bool visible);

    // --- ハイライト操作 ---

    /// 選択中のみを消し、直前手のハイライトは残す
    void clearSelectionHighlight();

    /// 局面移動等で直前手ハイライトだけを反映する
    void showMoveHighlights(const QPoint& from, const QPoint& to);

    /// すべてのハイライトを削除する
    void clearAllHighlights();

public slots:
    /// 2クリック入力の途中状態をキャンセルする（新規・動作状態や手番が変わったとき）
    void cancelPendingClick();

    /// 左クリック: 駒選択→移動先指定の2クリック処理
    void onLeftClick(const QPoint& pt);

    /// 右クリック: 選択キャンセルまたは成/不成トグル（編集モード時）
    void onRightClick(const QPoint& pt);

    /// 着手適用結果の通知を受けてハイライトを更新する
    void onMoveApplied(const QPoint& from, const QPoint& to, bool success);

private:
    bool acceptsInput() const;
    bool isEditing() const;
    void selectPieceAndHighlight(const QPoint& field);
    void finalizeDrag();
    void refreshLegalMoveHighlights();
    void clearLegalMoveHighlights();
    void togglePiecePromotionOnClick(const QPoint& field);

    static constexpr int kBlackStandFile = BoardConstants::kBlackStandFile; ///< 先手駒台のファイル番号
    static constexpr int kWhiteStandFile = BoardConstants::kWhiteStandFile; ///< 後手駒台のファイル番号

    // --- メンバ変数 ---
    ShogiView* m_view = nullptr; ///< 盤面ビュー（非所有）
    ShogiBoard* m_board = nullptr;       ///< 表示している盤（非所有）
    BoardInputRoute* m_route = nullptr;  ///< 指した手の行き先（非所有）
    PromotionChooser m_promotionChooser; ///< 成り・不成の選び方
    bool m_legalMovesVisible = true;     ///< 合法な移動先の表示

    // --- クリック状態 ---
    QPoint m_clickPoint; ///< 現在選択中のマス座標
    bool   m_waitingSecondClick = false; ///< 2クリック目待ちフラグ
};

#endif // BOARDINTERACTIONCONTROLLER_H
