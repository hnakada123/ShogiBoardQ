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

#include "elidelabel.h"
#include "shogitypes.h"

class GlobalToolTip;
class QButtonGroup;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;

/**
 * @brief 盤（ShogiView）に重ねる対局者カード・時計・手番の札・「編集終了」ボタン・駒箱の先後切り替え
 *
 * ShogiView の子として盤と同じ大きさで重ね、表示している部品の範囲だけをマスクで残す
 * （それ以外のクリックは下の盤に届く）。盤の描画や入力は扱わず、ShogiView から
 * 駒台・盤の位置（BoardLayout）を受け取って部品を置く。
 */
class BoardChromeWidget : public QWidget
{
    Q_OBJECT

public:
    /// 手番側の残り時間に応じた時計の警告
    enum class Urgency { Normal, Warn10, Warn5 };

    // しきい値（ミリ秒）
    static constexpr qint64 kWarn10Ms = 10'000;
    static constexpr qint64 kWarn5Ms  = 5'000;

    /// 部品の配置に使う盤・駒台の位置（ShogiView の座標）
    struct BoardLayout {
        QSize fieldSize;            ///< 1マスの大きさ
        double nameFontScale = 0.36;
        QRect blackStand;           ///< 先手の駒台（盤が無ければ無効）
        QRect whiteStand;           ///< 後手の駒台（盤が無ければ無効）
        QRect boardRect;            ///< 盤のマス目の範囲（盤が無ければ無効）
        bool flipped = false;
    };

    explicit BoardChromeWidget(QWidget* parent = nullptr);

    // ──────────────── 配置 ────────────────
    /// 盤・駒台の位置に合わせてカードと「編集終了」ボタンを置き直す
    void setBoardLayout(const BoardLayout& layout);
    /// 表示中のカードの範囲（隠れていれば空）
    QRect visibleCardRect(bool black) const;
    /// 駒箱の先後切り替えボタンを置く（空なら隠す）
    void placePieceBoxSelector(const QRect& selector);
    void setPieceBoxSide(Turn side);
    /// 「編集終了」ボタンを出す・隠す（局面編集の間だけ出す。押すと editExitRequested）
    void setEditExitButtonVisible(bool visible);

    // ──────────────── 名前・時計 ────────────────
    void setBlackPlayerName(const QString& name);
    void setWhitePlayerName(const QString& name);
    void setBlackClockText(const QString& text);
    void setWhiteClockText(const QString& text);
    void setClockEnabled(bool enabled);
    ElideLabel* blackNameLabel() const { return m_nameLabels[0]; }
    ElideLabel* whiteNameLabel() const { return m_nameLabels[1]; }
    QLabel* blackClockLabel() const { return m_clockLabels[0]; }
    QLabel* whiteClockLabel() const { return m_clockLabels[1]; }
    QLabel* turnBadge(bool black) const { return m_badges[black ? 0 : 1]; }

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

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onPieceBoxButtonClicked(int id);

private:
    void relayoutCards();
    void placeCard(int side, const QRect& stand, bool above, const QFont& badgeFont,
                   int padding, int gap, int nameHeight, int badgeHeight, int clockHeight, int cardHeight);
    void refreshNameLabels();
    void refreshPlayerStyles();
    void updateMask();
    QPushButton* ensureEditExitButton();
    /// 「編集終了」ボタンを作って置き直す
    void relayoutEditExitButton();
    void fitEditExitButtonFont(QPushButton* btn, int maxWidth);

    // 先手・後手の順
    std::array<QFrame*, 2> m_cards{};
    std::array<ElideLabel*, 2> m_nameLabels{};
    std::array<QLabel*, 2> m_clockLabels{};
    std::array<QLabel*, 2> m_badges{};
    std::array<QString, 2> m_nameBases;          ///< 装飾（▲▼▽△）を除いた名前
    QPushButton* m_editExitButton = nullptr;
    QButtonGroup* m_pieceBoxGroup = nullptr;
    std::array<QToolButton*, 2> m_pieceBoxButtons{};
    GlobalToolTip* m_tooltip = nullptr;

    BoardLayout m_layout;
    bool m_clockEnabled = true;

    // 手番表示の状態
    Turn m_activeSide = Turn::Black;
    bool m_turnHighlightActive = false;          ///< 手番の側を一度でも示したか（起動直後は示さない）
    Urgency m_urgency = Urgency::Normal;

    // 対局者の名前・時計・枠・手番の札のスタイルシート。手番の交代では何度も、時計の表示が
    // 変わるたび（1秒ごと）にも呼ばれるので、色の組が前回と同じなら前回の文字列を使う。
    struct CachedStyleSheet {
        std::array<QRgb, 3> colors{};
        int variant = -1;
        QString css;
        /// 色の組か種類が前回と違えば覚え直して true を返す（css を組み立て直す）
        bool update(const std::array<QRgb, 3>& newColors, int newVariant);
    };
    enum PlayerStyleTarget { NameStyle, ClockStyle, CardStyle, BadgeStyle, PlayerStyleCount };
    std::array<CachedStyleSheet, 2 * PlayerStyleCount> m_playerStyleSheets;
    static void setLabelStyle(QLabel* lbl, const QColor& fg, const QColor& bg, const QColor& border, bool bold,
                              CachedStyleSheet& cache);
};

#endif // BOARDCHROMEWIDGET_H
