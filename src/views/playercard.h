#ifndef PLAYERCARD_H
#define PLAYERCARD_H

/// @file playercard.h
/// @brief 対局者1人のカード（手番の札・名前・時計）

#include <QFont>
#include <QFrame>
#include <QRect>
#include <QString>

#include <array>

#include "elidelabel.h"
#include "shogitypes.h"

class GlobalToolTip;
class QLabel;
struct BoardColors;

/**
 * @brief 盤の脇に置く対局者1人のカード（上から手番の札・名前（2行）・時計）
 *
 * 札・名前・時計はカードの子で、place() で決めた大きさに並べる（表示していないときも同じ位置にする）。
 * 手番でない側にも札の場所を取っておき、手番が替わっても名前と時計が動かないようにする。
 */
class PlayerCard : public QFrame
{
    Q_OBJECT

public:
    /// 中の部品の大きさ（盤の大きさと書体から決める）
    struct Metrics {
        int padding = 3;       ///< カードの内側の余白
        int gap = 2;           ///< 部品の間
        int badgeHeight = 0;
        int nameHeight = 0;    ///< 名前（2行分）
        int clockHeight = 0;
        QFont badgeFont;
        bool clockVisible = true;

        /// カードの高さ
        int cardHeight() const;
    };

    PlayerCard(Turn side, QWidget* parent = nullptr);

    Turn side() const { return m_side; }
    ElideLabel* nameLabel() const { return m_name; }
    QLabel* clockLabel() const { return m_clock; }
    QLabel* turnBadge() const { return m_badge; }

    /// 名前（▲▼▽△ の印は外す。盤の向きの印は自分で付ける）
    void setName(const QString& name);
    /// 盤の向き（名前の前の印が変わる）
    void setFlipped(bool flipped);
    void setClockText(const QString& text);

    /// rect に置き、中の部品を並べる
    void place(const QRect& rect, const Metrics& metrics);

    /// 配色。active は手番の側か、showBadge は手番の札を出すか
    void applyColors(const BoardColors& colors, bool active, const QColor& clockColor, bool showBadge);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void refreshName();

    /// 色の組が前回と同じなら、前回のスタイルシートを使う（時計の表示が変わるたびに呼ばれる）
    struct CachedStyleSheet {
        std::array<QRgb, 3> colors{};
        int variant = -1;
        QString css;
        /// 色の組か種類が前回と違えば覚え直して true を返す（css を組み立て直す）
        bool update(const std::array<QRgb, 3>& newColors, int newVariant);
    };
    static void setLabelStyle(QLabel* label, const QColor& fg, const QColor& bg, const QColor& border, bool bold,
                              CachedStyleSheet& cache);

    Turn m_side = Turn::Black;
    bool m_flipped = false;
    QString m_nameBase;                ///< 装飾（▲▼▽△）を除いた名前
    ElideLabel* m_name = nullptr;
    QLabel* m_clock = nullptr;
    QLabel* m_badge = nullptr;
    GlobalToolTip* m_tooltip = nullptr;
    CachedStyleSheet m_nameStyle;
    CachedStyleSheet m_clockStyle;
    CachedStyleSheet m_cardStyle;
    CachedStyleSheet m_badgeStyle;
};

#endif // PLAYERCARD_H
