/// @file boardchromewidget.cpp
/// @brief 盤の周りの対局者カード・時計・手番の札・編集用ボタンの実装

#include "boardchromewidget.h"
#include "boardappearance.h"
#include "globaltooltip.h"
#include "logcategories.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QHelpEvent>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QProxyStyle>
#include <QPushButton>
#include <QRegion>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>

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

/// ラベルの表示矩形（rect）にテキストが収まるよう、フォントサイズを自動調整する。
void fitLabelFontToRect(QLabel* label, const QString& text, const QRect& rect, int paddingPx = 2)
{
    if (!label) return;
    const QRect inner = rect.adjusted(paddingPx, paddingPx, -paddingPx, -paddingPx);
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

/// 編集終了ボタンの左に置くチェック印（白）。文字の高さに合わせて描く
QIcon editExitCheckIcon(int px)
{
    const qreal dpr = 2.0;
    QPixmap pixmap(QSize(px, px) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, qMax(1.5, px * 0.15), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(QPolygonF({QPointF(px * 0.18, px * 0.54), QPointF(px * 0.41, px * 0.76),
                                    QPointF(px * 0.84, px * 0.27)}));
    return QIcon(pixmap);
}

/// チェック印の大きさ（文字の高さの8割）
int editExitIconSize(const QFont& font)
{
    return qMax(10, qRound(QFontMetrics(font).height() * 0.8));
}

// 「局面編集終了」ボタンの見た目（ダイアログの決定ボタンと同じ青系。駒台のカードと同じ角丸）
const QString& editExitButtonStyleSheet()
{
    static const QString css = QStringLiteral(
        "QPushButton#editExitButton {"
        "  color: #ffffff; border: 1px solid #155ea8; border-radius: 6px; padding: 4px 10px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a86de, stop:1 #1a6fc6); }"
        "QPushButton#editExitButton:hover { border-color: #1a69b8;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a94e8, stop:1 #237bd3); }"
        "QPushButton#editExitButton:pressed { border-color: #114c8c;"
        "  padding-top: 5px; padding-bottom: 3px; background: #165fae; }"
        "QPushButton#editExitButton[compact=\"true\"] { padding: 3px 3px; }"
        "QPushButton#editExitButton[compact=\"true\"]:pressed { padding-top: 4px; padding-bottom: 2px; }"
        "QPushButton#editExitButton:disabled { color: #eceff1; border-color: #90a4ae; background: #b0bec5; }");
    return css;
}

/// 「編集終了」ボタンの影が部品の外へはみ出す幅
constexpr int kExitButtonShadowMargin = 16;

} // namespace

BoardChromeWidget::BoardChromeWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("boardChrome"));
    setFocusPolicy(Qt::NoFocus);
    // 背景は塗らない（盤が透けて見える）。表示する部品の範囲だけをマスクで残す。
    setAttribute(Qt::WA_NoSystemBackground);

    const char* const cardNames[] = {"blackPlayerCard", "whitePlayerCard"};
    const char* const nameNames[] = {"blackNameLabel", "whiteNameLabel"};
    const char* const clockNames[] = {"blackClockLabel", "whiteClockLabel"};
    const char* const badgeNames[] = {"turnLabelBlack", "turnLabelWhite"};
    for (std::size_t side = 0; side < 2; ++side) {
        // カードは名前・時計・手番の札の背景。クリックは盤に通す
        auto* card = new QFrame(this);
        card->setObjectName(QLatin1String(cardNames[side]));
        card->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        card->hide();
        m_cards[side] = card;

        auto* clock = new QLabel(QStringLiteral("00:00:00"), this);
        clock->setObjectName(QLatin1String(clockNames[side]));
        clock->setAlignment(Qt::AlignCenter);
        clock->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QFont clockFont = font();
        clockFont.setStyleName(QString());
        clockFont.setItalic(false);
        clockFont.setWeight(QFont::DemiBold);  // 大きさは配置のたびに枠に合わせる（太さはこのまま）
        clockFont.setPointSizeF(qMax(8.0, 50 * 0.45));
        clock->setFont(clockFont);
        m_clockLabels[side] = clock;

        auto* name = new ElideLabel(this);
        name->setObjectName(QLatin1String(nameNames[side]));
        name->setElideMode(Qt::ElideRight);
        name->setWordWrap(true);
        name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        name->setStyle(new PlayerNameLabelStyle(name));
        name->installEventFilter(this);
        m_nameLabels[side] = name;

        auto* badge = new QLabel(tr("手番"), this);
        badge->setObjectName(QLatin1String(badgeNames[side]));
        badge->setAlignment(Qt::AlignCenter);
        badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        badge->setContentsMargins(0, 0, 0, 0);
        badge->setMargin(0);
        badge->setWordWrap(false);
        badge->setTextFormat(Qt::PlainText);
        badge->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        badge->hide();
        m_badges[side] = badge;
    }

    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &BoardChromeWidget::refreshPlayerStyles);

    // 起動直後の見た目を整える
    refreshPlayerStyles();
    updateMask();
}

// ─────────────────────────────────────────────────────────────────────────────
// 配置
// ─────────────────────────────────────────────────────────────────────────────

void BoardChromeWidget::setBoardLayout(const BoardLayout& layout)
{
    const bool flipChanged = m_layout.flipped != layout.flipped;
    const bool hadBoard = m_layout.blackStand.isValid() || m_layout.whiteStand.isValid();
    m_layout = layout;
    if (flipChanged) refreshNameLabels();
    relayoutCards();
    // 手番の札は盤が置かれているときだけ出す。盤より先に手番を受け取ったときは、盤が置かれた時点で出す
    const bool hasBoard = m_layout.blackStand.isValid() || m_layout.whiteStand.isValid();
    if (hasBoard != hadBoard) refreshPlayerStyles();
    // 「編集終了」ボタンは局面編集の間だけ表示する。表示する処理（setEditExitButtonVisible）は
    // 表示の直前に配置し直すので、隠れている間は配置と
    // 文字の大きさ合わせを省く（手番が替わるたびにも呼ばれる）
    if (m_editExitButton && !m_editExitButton->isHidden()) relayoutEditExitButton();
    updateMask();
}

QRect BoardChromeWidget::visibleCardRect(bool black) const
{
    const QFrame* card = m_cards[black ? 0 : 1];
    return card->isHidden() ? QRect() : card->geometry();
}

// 名前・時計・手番の札は共通のカードに並べ、カードは駒台の外側（盤の上下）に置く。
void BoardChromeWidget::relayoutCards()
{
    const QSize fs = m_layout.fieldSize.isValid() ? m_layout.fieldSize : QSize(50, 55);
    const int padding = qMax(3, qRound(fs.width() * 0.10));
    const int gap = qMax(2, qRound(fs.height() * 0.04));

    QFont nameFont = font();
    nameFont.setPixelSize(qMax(8, qRound(fs.height() * m_layout.nameFontScale * 0.8)));
    QFont badgeFont = font();
    badgeFont.setPixelSize(qMax(8, qRound(fs.height() * 0.16)));
    badgeFont.setBold(true);
    // 両者とも2行分を確保し、長い名前でも残り時間と重ならないようにする。
    // macOS ではラベルの書体が盤面の書体と異なる（行の高さも違う）ため、
    // 設定後にラベルが実際に使う書体で測る。
    for (ElideLabel* name : m_nameLabels) name->setFont(nameFont);
    const int nameLineHeight = qMax(m_nameLabels[0]->fontMetrics().height(), m_nameLabels[1]->fontMetrics().height());
    const int nameHeight = nameLineHeight * 2 + 2;
    const int badgeHeight = QFontMetrics(badgeFont).height() + 4;
    const int clockHeight = qMax(nameLineHeight + 4, qRound(fs.height() * 0.42));
    // 非手番側にもバッジの余白を確保し、手番交代でカードや文字が動かないようにする。
    const int cardHeight = padding * 2 + badgeHeight + gap + nameHeight
        + (m_clockEnabled ? gap + clockHeight : 0);

    placeCard(0, m_layout.blackStand, !m_layout.flipped, badgeFont, padding, gap, nameHeight, badgeHeight,
              clockHeight, cardHeight);
    placeCard(1, m_layout.whiteStand, m_layout.flipped, badgeFont, padding, gap, nameHeight, badgeHeight,
              clockHeight, cardHeight);
}

void BoardChromeWidget::placeCard(int side, const QRect& stand, bool above, const QFont& badgeFont,
                                  int padding, int gap, int nameHeight, int badgeHeight, int clockHeight,
                                  int cardHeight)
{
    QFrame* card = m_cards[static_cast<std::size_t>(side)];
    ElideLabel* name = m_nameLabels[static_cast<std::size_t>(side)];
    QLabel* clock = m_clockLabels[static_cast<std::size_t>(side)];
    QLabel* badge = m_badges[static_cast<std::size_t>(side)];
    if (!stand.isValid()) {
        card->hide();
        name->hide();
        clock->hide();
        badge->hide();
        return;
    }
    const int standGap = qMax(4, qRound(m_layout.fieldSize.height() * 0.08));
    const int top = above ? stand.top() - standGap - cardHeight : stand.bottom() + 1 + standGap;
    const QRect cardRect(stand.left(), qBound(0, top, qMax(0, height() - cardHeight)), stand.width(), cardHeight);
    card->setGeometry(cardRect);
    card->show();
    card->lower();

    const int x = cardRect.left() + padding;
    const int contentWidth = qMax(1, cardRect.width() - padding * 2);
    int y = cardRect.top() + padding;
    badge->setFont(badgeFont);
    const int badgeWidth = qMin(contentWidth, QFontMetrics(badgeFont).horizontalAdvance(badge->text()) + padding * 2);
    badge->setGeometry(x, y, badgeWidth, badgeHeight);
    fitLabelFontToRect(badge, badge->text(), badge->geometry());
    y += badgeHeight + gap;

    name->setGeometry(x, y, contentWidth, nameHeight);
    name->show();
    y += nameHeight + gap;

    clock->setGeometry(x, y, contentWidth, clockHeight);
    fitLabelFontToRect(clock, clock->text(), clock->geometry());
    clock->setVisible(m_clockEnabled);
    name->raise();
    clock->raise();
    badge->raise();
}

void BoardChromeWidget::placePieceBoxSelector(const QRect& selector)
{
    if (selector.isEmpty()) {
        for (QToolButton* button : m_pieceBoxButtons) {
            if (button) button->hide();
        }
        updateMask();
        return;
    }
    if (!m_pieceBoxGroup) {
        m_pieceBoxGroup = new QButtonGroup(this);
        const char* const names[] = {"pieceBoxBlackButton", "pieceBoxWhiteButton"};
        const QString texts[] = {tr("先手"), tr("後手")};
        const QString tips[] = {tr("先手の駒を配置（手番は変更しません）"), tr("後手の駒を配置（手番は変更しません）")};
        for (std::size_t side = 0; side < 2; ++side) {
            auto* button = new QToolButton(this);
            button->setObjectName(QLatin1String(names[side]));
            button->setText(texts[side]);
            button->setToolTip(tips[side]);
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            button->setStyleSheet(QStringLiteral(
                "QToolButton { border: 1px solid palette(mid); border-radius: 3px;"
                " padding: 0px; background: palette(button); color: palette(button-text); }"
                "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
                "QToolButton:focus { border: 1px solid palette(text); }"));
            m_pieceBoxGroup->addButton(button, static_cast<int>(side));
            m_pieceBoxButtons[side] = button;
        }
        connect(m_pieceBoxGroup, &QButtonGroup::idClicked, this, &BoardChromeWidget::onPieceBoxButtonClicked);
        m_pieceBoxButtons[0]->setChecked(true);
    }
    const int halfWidth = selector.width() / 2;
    m_pieceBoxButtons[0]->setGeometry(selector.left() + 1, selector.top(), halfWidth - 2, selector.height() - 1);
    m_pieceBoxButtons[1]->setGeometry(selector.left() + halfWidth + 1, selector.top(),
                                      selector.width() - halfWidth - 2, selector.height() - 1);
    for (QToolButton* button : m_pieceBoxButtons) {
        QFont buttonFont = font();
        buttonFont.setBold(true);
        int pixels = qMax(1, button->height() * 2 / 3);
        buttonFont.setPixelSize(pixels);
        while (pixels > 1 && (QFontMetrics(buttonFont).horizontalAdvance(button->text()) > button->width() - 4
                              || QFontMetrics(buttonFont).height() > button->height() - 2))
            buttonFont.setPixelSize(--pixels);
        button->setFont(buttonFont);
        button->show();
        button->raise();
    }
    updateMask();
}

void BoardChromeWidget::setPieceBoxSide(Turn side)
{
    if (m_pieceBoxButtons[0]) m_pieceBoxButtons[0]->setChecked(side == Turn::Black);
    if (m_pieceBoxButtons[1]) m_pieceBoxButtons[1]->setChecked(side == Turn::White);
}

void BoardChromeWidget::onPieceBoxButtonClicked(int id)
{
    emit pieceBoxSideClicked(id == 0 ? Turn::Black : Turn::White);
}

// 部品のある範囲だけを残し、それ以外（盤・駒台・駒箱）のクリックと描画は下の盤に任せる。
// 隠れている部品も含めるのは、呼び出し側が「編集終了」ボタンを表示するときに切れないようにするため。
void BoardChromeWidget::updateMask()
{
    QRegion region;
    for (QObject* child : children()) {
        auto* widget = qobject_cast<QWidget*>(child);
        if (!widget || widget->isWindow()) continue;
        QRect rect = widget->geometry();
        if (widget == m_editExitButton) {
            rect.adjust(-kExitButtonShadowMargin, -kExitButtonShadowMargin,
                        kExitButtonShadowMargin, kExitButtonShadowMargin);
        }
        region += rect;
    }
    if (region.isEmpty()) region = QRect(0, 0, 1, 1);   // 空のマスクはマスク無し（全体）になる
    setMask(region);
}

// ─────────────────────────────────────────────────────────────────────────────
// 「編集終了」ボタン
// ─────────────────────────────────────────────────────────────────────────────

QPushButton* BoardChromeWidget::ensureEditExitButton()
{
    if (m_editExitButton) return m_editExitButton;
    auto* btn = new QPushButton(tr("編集終了"), this);
    btn->setObjectName(QStringLiteral("editExitButton"));
    btn->setToolTip(tr("局面編集を終了し、この局面を開始局面にします"));
    btn->setVisible(false);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoDefault(false);
    btn->setDefault(false);
    btn->setFlat(false);
    btn->setStyleSheet(editExitButtonStyleSheet());
    // 盤の上に浮かせる薄い影
    auto* shadow = new QGraphicsDropShadowEffect(btn);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 70));
    btn->setGraphicsEffect(shadow);
    btn->raise();
    connect(btn, &QPushButton::clicked, this, &BoardChromeWidget::editExitRequested);
    m_editExitButton = btn;
    return btn;
}

void BoardChromeWidget::setEditExitButtonVisible(bool visible)
{
    if (!visible) {
        if (m_editExitButton) m_editExitButton->hide();
        return;
    }
    relayoutEditExitButton();
    m_editExitButton->show();
    m_editExitButton->raise();
}

void BoardChromeWidget::relayoutEditExitButton()
{
    QPushButton* exitBtn = ensureEditExitButton();

    // ── 右側の対局者カード（駒台と同じ幅）に揃える。カードが無いときは名前ラベル ──
    const bool blackOnRight = m_nameLabels[0]->geometry().center().x() > m_nameLabels[1]->geometry().center().x();
    const std::size_t side = blackOnRight ? 0 : 1;
    QLabel* base = m_nameLabels[side];
    const QFrame* card = m_cards[side];

    QRect baseGeo;
    if (card->isVisible() && card->geometry().isValid()) {
        baseGeo = card->geometry();
    } else {
        baseGeo = base->geometry();
    }

    // 書体とサイズを名前ラベルに合わせ、太字にする。
    QFont buttonFont = base->font();
    buttonFont.setBold(true);
    exitBtn->setFont(buttonFont);

    const int x = baseGeo.x();
    const int w = qMin(baseGeo.width(), width() - x - 4);
    fitEditExitButtonFont(exitBtn, w);

    // チェック印は文字の高さに合わせて描き直す（幅が足りず詰めた表示では外す）
    if (exitBtn->property("compact").toBool()) {
        exitBtn->setIcon(QIcon());
    } else {
        const int iconPx = editExitIconSize(exitBtn->font());
        if (exitBtn->iconSize() != QSize(iconPx, iconPx) || exitBtn->icon().isNull()) {
            exitBtn->setIcon(editExitCheckIcon(iconPx));
            exitBtn->setIconSize(QSize(iconPx, iconPx));
        }
    }

    const int hBtn = qMax(24, QFontMetrics(exitBtn->font()).height() + 14);
    // 盤があれば盤の上端に揃える。無ければカードの上に置く
    const int y = m_layout.boardRect.isValid() ? m_layout.boardRect.top() : qMax(0, baseGeo.y() - hBtn - 4);
    exitBtn->setGeometry(x, y, w, hBtn);
    updateMask();
}

// ボタンの文字列が maxWidth に必ず収まるよう、フォントサイズを自動調整（縮小のみ）。
// まずチェック印付きで縮め、入らなければ印を外して余白を詰め（compact）、さらに縮める。
void BoardChromeWidget::fitEditExitButtonFont(QPushButton* btn, int maxWidth)
{
    if (!btn || maxWidth <= 20) return;

    const QFont f = btn->font();
    int pixel = QFontInfo(f).pixelSize();
    if (pixel <= 0) pixel = 16;

    const auto withSize = [&f](int px) { QFont tf = f; tf.setPixelSize(px); return tf; };
    const auto textWidth = [btn](const QFont& tf) { return QFontMetrics(tf).horizontalAdvance(btn->text()); };
    // 通常: 左右の余白10px・枠1px、チェック印と間隔6px
    const auto fitsWithIcon = [&](const QFont& tf) { return textWidth(tf) + editExitIconSize(tf) + 6 + 22 <= maxWidth; };
    // 詰めた表示: 左右の余白3px・枠1px、印なし
    const auto fitsCompact = [&](const QFont& tf) { return textWidth(tf) + 8 <= maxWidth; };

    bool compact = false;
    QFont chosen = withSize(qMax(8, pixel * 3 / 4));
    bool found = false;
    for (int px = pixel; px >= qMax(10, pixel * 3 / 4); --px) {
        if (fitsWithIcon(withSize(px))) { chosen = withSize(px); found = true; break; }
    }
    if (!found) {
        compact = true;
        chosen = withSize(8);
        for (int px = pixel; px >= 8; --px) {
            if (fitsCompact(withSize(px))) { chosen = withSize(px); break; }
        }
    }
    btn->setFont(chosen);
    if (btn->property("compact").toBool() != compact) {
        btn->setProperty("compact", compact);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 名前・時計
// ─────────────────────────────────────────────────────────────────────────────

void BoardChromeWidget::setBlackPlayerName(const QString& name)
{
    qCDebug(lcView).noquote() << "setBlackPlayerName:" << name;
    m_nameBases[0] = stripMarks(name);
    refreshNameLabels();
}

void BoardChromeWidget::setWhitePlayerName(const QString& name)
{
    qCDebug(lcView).noquote() << "setWhitePlayerName:" << name;
    m_nameBases[1] = stripMarks(name);
    refreshNameLabels();
}

// 名前の前に向きのマークを付ける（盤を反転すると先手が上になるので向きも変える）
void BoardChromeWidget::refreshNameLabels()
{
    const QString marks[] = {m_layout.flipped ? QStringLiteral("▼") : QStringLiteral("▲"),
                             m_layout.flipped ? QStringLiteral("△") : QStringLiteral("▽")};
    for (std::size_t side = 0; side < 2; ++side) {
        m_nameLabels[side]->setFullText(marks[side] + m_nameBases[side]);
        m_nameLabels[side]->setToolTip(QStringLiteral(
            "<div style='background-color:#FFF9C4; color:#333;"
            "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
            .arg(m_nameBases[side].toHtmlEscaped()));
    }
}

void BoardChromeWidget::setBlackClockText(const QString& text)
{
    m_clockLabels[0]->setText(text);
    fitLabelFontToRect(m_clockLabels[0], text, m_clockLabels[0]->geometry());
}

void BoardChromeWidget::setWhiteClockText(const QString& text)
{
    m_clockLabels[1]->setText(text);
    fitLabelFontToRect(m_clockLabels[1], text, m_clockLabels[1]->geometry());
}

void BoardChromeWidget::setClockEnabled(bool enabled)
{
    m_clockEnabled = enabled;
    relayoutCards();
    updateMask();
}

bool BoardChromeWidget::eventFilter(QObject* obj, QEvent* ev)
{
    const bool black = obj == m_nameLabels[0];
    if (black || obj == m_nameLabels[1]) {
        if (ev->type() == QEvent::ToolTip) {
            if (!m_tooltip) {
                m_tooltip = new GlobalToolTip(this);
                m_tooltip->setCompact(true);
                m_tooltip->setPointSizeF(12.0);
            }
            m_tooltip->showText(static_cast<QHelpEvent*>(ev)->globalPos(), m_nameBases[black ? 0 : 1]);
            return true;
        }
        if (ev->type() == QEvent::Leave && m_tooltip) m_tooltip->hideTip();
    }
    return QWidget::eventFilter(obj, ev);
}

// ─────────────────────────────────────────────────────────────────────────────
// 手番・緊急度
// ─────────────────────────────────────────────────────────────────────────────

void BoardChromeWidget::setActiveSide(Turn side)
{
    if (side != m_activeSide) m_urgency = Urgency::Normal;
    m_activeSide = side;
    m_turnHighlightActive = true;
    refreshPlayerStyles();
}

void BoardChromeWidget::setUrgency(Urgency u)
{
    m_urgency = u;
    m_turnHighlightActive = true;
    refreshPlayerStyles();
}

void BoardChromeWidget::refreshPlayerStyles()
{
    const BoardColors& colors = BoardAppearance::instance().colors();
    const bool hasBoard = m_layout.blackStand.isValid() || m_layout.whiteStand.isValid();
    for (std::size_t side = 0; side < 2; ++side) {
        const bool black = side == 0;
        const bool active = m_turnHighlightActive && (m_activeSide == Turn::Black) == black;
        CachedStyleSheet* styleSheets = &m_playerStyleSheets[side * PlayerStyleCount];
        const QColor nameColor = colors.nameText;
        QColor clockColor = colors.clockText;
        if (active && m_urgency == Urgency::Warn10) clockColor = colors.clockWarningText;
        if (active && m_urgency == Urgency::Warn5) clockColor = colors.clockCriticalText;
        setLabelStyle(m_nameLabels[side], nameColor, colors.nameBackground, colors.nameBorder, false,
                      styleSheets[NameStyle]);
        setLabelStyle(m_clockLabels[side], clockColor, colors.clockBackground, colors.clockBorder, true,
                      styleSheets[ClockStyle]);

        QFrame* card = m_cards[side];
        const QColor background = colors.cardBackground;
        const QColor border = active ? colors.activeCardBorder : colors.cardBorder;
        CachedStyleSheet& cardCache = styleSheets[CardStyle];
        if (cardCache.update({background.rgba(), border.rgba(), 0}, active ? 2 : 1)) {
            cardCache.css = QStringLiteral("QFrame#%1 { background:%2; border:%3px solid %4; border-radius:6px; }")
                .arg(card->objectName(), toRgba(background))
                .arg(active ? 2 : 1)
                .arg(toRgba(border));
        }
        if (card->styleSheet() != cardCache.css) card->setStyleSheet(cardCache.css);

        QLabel* badge = m_badges[side];
        CachedStyleSheet& badgeCache = styleSheets[BadgeStyle];
        if (badgeCache.update({colors.turnText.rgba(), colors.turnBackground.rgba(), colors.turnBorder.rgba()}, 0)) {
            badgeCache.css = QStringLiteral(
                "color:%1; background:%2; border:1px solid %3; border-radius:4px; padding:0px; font-weight:700;")
                .arg(toRgba(colors.turnText), toRgba(colors.turnBackground), toRgba(colors.turnBorder));
        }
        if (badge->styleSheet() != badgeCache.css) badge->setStyleSheet(badgeCache.css);
        badge->setVisible(active && hasBoard);
    }
}

bool BoardChromeWidget::CachedStyleSheet::update(const std::array<QRgb, 3>& newColors, int newVariant)
{
    if (variant == newVariant && colors == newColors) return false;
    colors = newColors;
    variant = newVariant;
    return true;
}

void BoardChromeWidget::setLabelStyle(QLabel* lbl, const QColor& fg, const QColor& bg, const QColor& border,
                                      bool bold, CachedStyleSheet& cache)
{
    if (!lbl) return;
    if (cache.update({fg.rgba(), bg.rgba(), border.rgba()}, bold ? 1 : 0)) {
        cache.css = QStringLiteral(
            "color:%1; background:%2; border:%4px solid %3; border-radius:2px; padding:0px; font-weight:%5;")
            .arg(toRgba(fg), toRgba(bg), toRgba(border))
            .arg(border.alpha() == 0 ? 0 : 1)
            .arg(bold ? QStringLiteral("700") : QStringLiteral("400"));
    }
    if (lbl->styleSheet() != cache.css) lbl->setStyleSheet(cache.css);
}
