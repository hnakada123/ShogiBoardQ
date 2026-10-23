/// @file playercard.cpp
/// @brief 対局者1人のカード（手番の札・名前・時計）の実装

#include "playercard.h"
#include "boardcolors.h"
#include "elidelabel.h"
#include "globaltooltip.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHash>
#include <QHelpEvent>
#include <QLabel>
#include <QProxyStyle>
#include <QSizePolicy>

namespace {

// 対局者名の吹き出しだけ、標準のツールチップより早く表示する。
class PlayerNameLabelStyle final : public QProxyStyle
{
public:
    explicit PlayerNameLabelStyle(QWidget* label) { setParent(label); }

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
        if (hint == SH_ToolTip_WakeUpDelay) return 200;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

// 文字サイズを合わせる条件（文字・収める大きさ・大きさ以外の書体）
struct LabelFitKey {
    QString text;
    QSize area;
    QFont font;

    bool operator==(const LabelFitKey& other) const
    {
        return text == other.text && area == other.area && font == other.font;
    }
};

size_t qHash(const LabelFitKey& key, size_t seed = 0)
{
    return qHashMulti(seed, key.text, key.area.width(), key.area.height(), key.font);
}

/// ラベルの大きさにテキストが収まるよう、フォントサイズを合わせる。
void fitLabelFont(QLabel* label, const QString& text, int paddingPx = 2)
{
    const QRect inner = label->rect().adjusted(paddingPx, paddingPx, -paddingPx, -paddingPx);
    QFont f = label->font();

    // 時計と手番の表示は1秒ごと・手番が替わるごとに合わせ直すため、同じ条件の結果を使い回す。
    // 大きさは下の探索で決めるので、条件の書体は大きさを揃えて比べる。
    static QHash<LabelFitKey, double> fittedSizes;
    QFont keyFont = f;
    keyFont.setPointSizeF(12.0);
    const LabelFitKey key{text, inner.size(), keyFont};
    if (const auto it = fittedSizes.constFind(key); it != fittedSizes.cend()) {
        f.setPointSizeF(it.value());
        label->setFont(f);
        return;
    }

    double lo = 1.0;
    double hi = 200.0;
    for (int i = 0; i < 18; ++i) {
        const double mid = (lo + hi) * 0.5;
        f.setPointSizeF(mid);
        const QFontMetrics fm(f);
        if (fm.horizontalAdvance(text) <= inner.width() && fm.height() <= inner.height()) lo = mid;
        else hi = mid;
    }

    if (fittedSizes.size() >= 512) fittedSizes.clear();
    fittedSizes.insert(key, lo);
    f.setPointSizeF(lo);
    label->setFont(f);
}

/// 文字列から ▲▼▽△ をすべて除去し、前後の空白をトリムして返す。
QString stripMarks(const QString& s)
{
    QString t = s;
    for (const QChar m : {QChar(0x25B2), QChar(0x25BC), QChar(0x25BD), QChar(0x25B3)})
        t.remove(m);
    return t.trimmed();
}

QString toRgba(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

} // namespace

int PlayerCard::Metrics::cardHeight() const
{
    // 手番でない側にも札の場所を取り、手番が替わってもカードや文字が動かないようにする
    return padding * 2 + badgeHeight + gap + nameHeight + (clockVisible ? gap + clockHeight : 0);
}

PlayerCard::PlayerCard(Turn side, QWidget* parent)
    : QFrame(parent)
    , m_side(side)
{
    const bool black = side == Turn::Black;
    setObjectName(black ? QStringLiteral("blackPlayerCard") : QStringLiteral("whitePlayerCard"));
    hide();

    m_badge = new QLabel(tr("手番"), this);
    m_badge->setObjectName(black ? QStringLiteral("turnLabelBlack") : QStringLiteral("turnLabelWhite"));
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_badge->setContentsMargins(0, 0, 0, 0);
    m_badge->setMargin(0);
    m_badge->setWordWrap(false);
    m_badge->setTextFormat(Qt::PlainText);
    m_badge->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_badge->hide();

    m_name = new ElideLabel(this);
    m_name->setObjectName(black ? QStringLiteral("blackNameLabel") : QStringLiteral("whiteNameLabel"));
    m_name->setElideMode(Qt::ElideRight);
    m_name->setWordWrap(true);
    m_name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_name->setStyle(new PlayerNameLabelStyle(m_name));
    m_name->installEventFilter(this);

    m_clock = new QLabel(QStringLiteral("00:00:00"), this);
    m_clock->setObjectName(black ? QStringLiteral("blackClockLabel") : QStringLiteral("whiteClockLabel"));
    m_clock->setAlignment(Qt::AlignCenter);
    m_clock->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QFont clockFont = font();
    clockFont.setStyleName(QString());
    clockFont.setItalic(false);
    clockFont.setWeight(QFont::DemiBold);  // 大きさは配置のたびに枠に合わせる（太さはこのまま）
    clockFont.setPointSizeF(qMax(8.0, 50 * 0.45));
    m_clock->setFont(clockFont);
}

void PlayerCard::place(const QRect& rect, const Metrics& metrics)
{
    setGeometry(rect);
    show();

    const int x = metrics.padding;
    const int contentWidth = qMax(1, rect.width() - metrics.padding * 2);
    int y = metrics.padding;
    m_badge->setFont(metrics.badgeFont);
    const int badgeWidth = qMin(contentWidth,
                                QFontMetrics(metrics.badgeFont).horizontalAdvance(m_badge->text()) + metrics.padding * 2);
    m_badge->setGeometry(x, y, badgeWidth, metrics.badgeHeight);
    fitLabelFont(m_badge, m_badge->text());
    y += metrics.badgeHeight + metrics.gap;

    m_name->setGeometry(x, y, contentWidth, metrics.nameHeight);
    y += metrics.nameHeight + metrics.gap;

    m_clock->setGeometry(x, y, contentWidth, metrics.clockHeight);
    fitLabelFont(m_clock, m_clock->text());
    m_clock->setVisible(metrics.clockVisible);
}

void PlayerCard::setName(const QString& name)
{
    m_nameBase = stripMarks(name);
    refreshName();
}

void PlayerCard::setFlipped(bool flipped)
{
    if (m_flipped == flipped) return;
    m_flipped = flipped;
    refreshName();
}

// 名前の前に向きのマークを付ける（盤を反転すると先手が上になるので向きも変える）
void PlayerCard::refreshName()
{
    const bool black = m_side == Turn::Black;
    const QString mark = black ? (m_flipped ? QStringLiteral("▼") : QStringLiteral("▲"))
                               : (m_flipped ? QStringLiteral("△") : QStringLiteral("▽"));
    // ツールチップは eventFilter が名前（向きのマークなし）を GlobalToolTip で出す
    m_name->setFullText(mark + m_nameBase);
}

void PlayerCard::setClockText(const QString& text)
{
    m_clock->setText(text);
    fitLabelFont(m_clock, text);
}

bool PlayerCard::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_name) {
        if (ev->type() == QEvent::ToolTip) {
            if (!m_tooltip) {
                m_tooltip = new GlobalToolTip(this);
                m_tooltip->setCompact(true);
                m_tooltip->setPointSizeF(12.0);
            }
            m_tooltip->showText(static_cast<QHelpEvent*>(ev)->globalPos(), m_nameBase);
            return true;
        }
        if (ev->type() == QEvent::Leave && m_tooltip) m_tooltip->hideTip();
    }
    return QFrame::eventFilter(obj, ev);
}

void PlayerCard::applyColors(const BoardColors& colors, bool active, const QColor& clockColor, bool showBadge)
{
    setLabelStyle(m_name, colors.nameText, colors.nameBackground, colors.nameBorder, false, m_nameStyle);
    setLabelStyle(m_clock, clockColor, colors.clockBackground, colors.clockBorder, true, m_clockStyle);

    const QColor background = colors.cardBackground;
    const QColor border = active ? colors.activeCardBorder : colors.cardBorder;
    if (m_cardStyle.update({background.rgba(), border.rgba(), 0}, active ? 2 : 1)) {
        m_cardStyle.css = QStringLiteral("QFrame#%1 { background:%2; border:%3px solid %4; border-radius:6px; }")
            .arg(objectName(), toRgba(background))
            .arg(active ? 2 : 1)
            .arg(toRgba(border));
    }
    if (styleSheet() != m_cardStyle.css) setStyleSheet(m_cardStyle.css);

    if (m_badgeStyle.update({colors.turnText.rgba(), colors.turnBackground.rgba(), colors.turnBorder.rgba()}, 0)) {
        m_badgeStyle.css = QStringLiteral(
            "color:%1; background:%2; border:1px solid %3; border-radius:4px; padding:0px; font-weight:700;")
            .arg(toRgba(colors.turnText), toRgba(colors.turnBackground), toRgba(colors.turnBorder));
    }
    if (m_badge->styleSheet() != m_badgeStyle.css) m_badge->setStyleSheet(m_badgeStyle.css);
    m_badge->setVisible(showBadge);
}

bool PlayerCard::CachedStyleSheet::update(const std::array<QRgb, 3>& newColors, int newVariant)
{
    if (variant == newVariant && colors == newColors) return false;
    colors = newColors;
    variant = newVariant;
    return true;
}

void PlayerCard::setLabelStyle(QLabel* label, const QColor& fg, const QColor& bg, const QColor& border, bool bold,
                               CachedStyleSheet& cache)
{
    if (cache.update({fg.rgba(), bg.rgba(), border.rgba()}, bold ? 1 : 0)) {
        cache.css = QStringLiteral(
            "color:%1; background:%2; border:%4px solid %3; border-radius:2px; padding:0px; font-weight:%5;")
            .arg(toRgba(fg), toRgba(bg), toRgba(border))
            .arg(border.alpha() == 0 ? 0 : 1)
            .arg(bold ? QStringLiteral("700") : QStringLiteral("400"));
    }
    if (label->styleSheet() != cache.css) label->setStyleSheet(cache.css);
}
