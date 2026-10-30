#ifndef BOARDCHROMEWIDGET_H
#define BOARDCHROMEWIDGET_H

/// @file boardchromewidget.h
/// @brief 盤の周りの対局者カード（名前・時計・手番の札）と編集用ボタンを表示するウィジェットの定義

#include <QColor>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>
#include <optional>

#include "boardgeometry.h"
#include "playercard.h"
#include "shogitypes.h"

class QButtonGroup;
class QPushButton;
class QToolButton;

/**
 * @brief 盤（ShogiView）に重ねる対局者カード・時計・手番の札・「編集終了」ボタン・駒箱の先後切り替え
 *
 * ShogiView の子として盤と同じ大きさで重ね、表示している部品の範囲だけをマスクで残す
 * （それ以外のクリックは下の盤に届く）。盤の描画や入力は扱わず、ShogiView から
 * 盤の位置（BoardGeometry）を受け取って部品を置く。対局者のカードは1人1枚（card()）。
 */
class BoardChromeWidget : public QWidget
{
    Q_OBJECT

public:
    /// 手番側の残り時間に応じた時計の警告（どれにするかは TimeDisplayPresenter が決める）
    enum class Urgency { Normal, Warn10, Warn5 };

    explicit BoardChromeWidget(QWidget* parent = nullptr);

    // ──────────────── 配置 ────────────────
    /// 盤の位置に合わせてカードと「編集終了」ボタンを置き直す（盤が無ければカードを隠す）
    void setBoardGeometry(const std::optional<BoardGeometry>& geometry);
    /// 対局者名の大きさ（マスの高さに対する比）
    void setNameFontScale(double scale);
    /// 対局者のカード
    PlayerCard* card(Turn side) const { return m_cards[side == Turn::Black ? 0 : 1]; }
    /// 対局者カードの高さ（BoardGeometry::pieceBox に渡す。カードを置いていなければ 0）
    int cardHeight() const { return m_cardHeight; }
    /// 駒箱の先後切り替えボタンを置く（空なら隠す）
    void placePieceBoxSelector(const QRect& selector);
    void setPieceBoxSide(Turn side);
    /// 「編集終了」ボタンを出す・隠す（局面編集の間だけ出す。押すと editExitRequested）
    void setEditExitButtonVisible(bool visible);

    // ──────────────── 時計 ────────────────
    /// 時計を出すか（出さなければカードを低くする）
    void setClockEnabled(bool enabled);

    // ──────────────── 手番・緊急度 ────────────────
    /// 手番の側を強調する。手番の側が替わったら時計の警告を消す（同じ側なら残す）
    void setActiveSide(Turn side);
    Turn activeSide() const { return m_activeSide; }
    /// 手番の側の時計の警告
    void setUrgency(Urgency u);
    Urgency urgency() const { return m_urgency; }

signals:
    void pieceBoxSideClicked(Turn side);
    /// 「編集終了」ボタンを押した
    void editExitRequested();

private slots:
    void onPieceBoxButtonClicked(int id);

private:
    void relayoutCards();
    void refreshPlayerStyles();
    void updateMask();
    QPushButton* ensureEditExitButton();
    /// 「編集終了」ボタンを作って置き直す
    void relayoutEditExitButton();
    void fitEditExitButtonFont(QPushButton* btn, int maxWidth);

    std::array<PlayerCard*, 2> m_cards{};       ///< 先手・後手の順
    QPushButton* m_editExitButton = nullptr;
    QButtonGroup* m_pieceBoxGroup = nullptr;
    std::array<QToolButton*, 2> m_pieceBoxButtons{};

    std::optional<BoardGeometry> m_geometry;    ///< 盤の位置（盤が無ければ空）
    double m_nameFontScale = 0.36;
    int m_cardHeight = 0;   ///< 対局者カードの高さ（名前と時計の書体で決まる。カードを出していなければ 0）
    bool m_clockEnabled = true;

    // 手番表示の状態
    Turn m_activeSide = Turn::Black;
    bool m_turnHighlightActive = false;          ///< 手番の側を一度でも示したか（起動直後は示さない）
    Urgency m_urgency = Urgency::Normal;

};

#endif // BOARDCHROMEWIDGET_H
