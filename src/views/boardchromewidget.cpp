/// @file boardchromewidget.cpp
/// @brief 盤の周りの対局者カード・時計・手番の札・編集用ボタンの実装

#include "boardchromewidget.h"
#include "boardappearance.h"
#include "elidelabel.h"
#include "logcategories.h"

#include <QButtonGroup>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegion>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace {

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

    m_cards = {new PlayerCard(Turn::Black, this), new PlayerCard(Turn::White, this)};

    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &BoardChromeWidget::refreshPlayerStyles);

    // 起動直後の見た目を整える
    refreshPlayerStyles();
    updateMask();
}

// ─────────────────────────────────────────────────────────────────────────────
// 配置
// ─────────────────────────────────────────────────────────────────────────────

void BoardChromeWidget::setBoardGeometry(const std::optional<BoardGeometry>& geometry)
{
    const bool hadBoard = m_geometry.has_value();
    m_geometry = geometry;
    for (PlayerCard* card : m_cards) card->setFlipped(m_geometry && m_geometry->flipped());
    relayoutCards();
    // 手番の札は盤が置かれているときだけ出す。盤より先に手番を受け取ったときは、盤が置かれた時点で出す
    if (m_geometry.has_value() != hadBoard) refreshPlayerStyles();
    // 「編集終了」ボタンは局面編集の間だけ表示する。表示する処理（setEditExitButtonVisible）は
    // 表示の直前に配置し直すので、隠れている間は配置と
    // 文字の大きさ合わせを省く（手番が替わるたびにも呼ばれる）
    if (m_editExitButton && !m_editExitButton->isHidden()) relayoutEditExitButton();
    updateMask();
}

void BoardChromeWidget::setNameFontScale(double scale)
{
    m_nameFontScale = std::clamp(scale, 0.2, 1.0);
    relayoutCards();
    updateMask();
}

// 名前・時計・手番の札はカードに並べ、カードは駒台の外側（盤の上下）に置く。
void BoardChromeWidget::relayoutCards()
{
    if (!m_geometry) {
        for (PlayerCard* card : m_cards) card->hide();
        m_cardHeight = 0;
        return;
    }
    const QSize fs = m_geometry->fieldSize();
    PlayerCard::Metrics metrics;
    metrics.padding = qMax(3, qRound(fs.width() * 0.10));
    metrics.gap = qMax(2, qRound(fs.height() * 0.04));
    metrics.clockVisible = m_clockEnabled;

    QFont nameFont = font();
    nameFont.setPixelSize(qMax(8, qRound(fs.height() * m_nameFontScale * 0.8)));
    metrics.badgeFont = font();
    metrics.badgeFont.setPixelSize(qMax(8, qRound(fs.height() * 0.16)));
    metrics.badgeFont.setBold(true);
    // 両者とも2行分を確保し、長い名前でも残り時間と重ならないようにする。
    // macOS ではラベルの書体が盤面の書体と異なる（行の高さも違う）ため、
    // 設定後にラベルが実際に使う書体で測る。
    int nameLineHeight = 0;
    for (PlayerCard* card : m_cards) {
        card->nameLabel()->setFont(nameFont);
        nameLineHeight = qMax(nameLineHeight, card->nameLabel()->fontMetrics().height());
    }
    metrics.nameHeight = nameLineHeight * 2 + 2;
    metrics.badgeHeight = QFontMetrics(metrics.badgeFont).height() + 4;
    metrics.clockHeight = qMax(nameLineHeight + 4, qRound(fs.height() * 0.42));

    m_cardHeight = metrics.cardHeight();
    for (PlayerCard* card : m_cards)
        card->place(m_geometry->playerCard(card->side(), m_cardHeight, height()), metrics);
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

    // ── 右側の対局者カード（駒台と同じ幅）に揃える ──
    const bool blackOnRight = !(m_geometry && m_geometry->flipped());
    const PlayerCard* card = m_cards[blackOnRight ? 0 : 1];
    const QRect baseGeo = card->geometry();

    // 書体とサイズを名前ラベルに合わせ、太字にする。
    QFont buttonFont = card->nameLabel()->font();
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
    const int y = m_geometry ? m_geometry->board().top() : qMax(0, baseGeo.y() - hBtn - 4);
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

void BoardChromeWidget::setClockEnabled(bool enabled)
{
    m_clockEnabled = enabled;
    relayoutCards();
    updateMask();
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
    for (PlayerCard* card : m_cards) {
        const bool active = m_turnHighlightActive && m_activeSide == card->side();
        QColor clockColor = colors.clockText;
        if (active && m_urgency == Urgency::Warn10) clockColor = colors.clockWarningText;
        if (active && m_urgency == Urgency::Warn5) clockColor = colors.clockCriticalText;
        card->applyColors(colors, active, clockColor, active && m_geometry.has_value());
    }
}
