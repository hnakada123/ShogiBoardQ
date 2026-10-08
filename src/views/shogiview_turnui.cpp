/// @file shogiview_turnui.cpp
/// @brief ShogiView の手番ラベル・編集終了ボタン管理

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "elidelabel.h"

#include <QLabel>
#include <QFont>
#include <QFontMetrics>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <QStyle>
#include <QFontInfo>

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

} // namespace

// カード背景と手番バッジを生成する。名前ラベルは既存のホバー操作を維持する。
void ShogiView::ensureTurnLabels()
{
    auto ensureCard = [this](QFrame*& card, const QString& name) {
        if (card) return;
        card = new QFrame(this);
        card->setObjectName(name);
        card->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        card->hide();
        card->lower();
    };
    ensureCard(m_blackPlayerCard, QStringLiteral("blackPlayerCard"));
    ensureCard(m_whitePlayerCard, QStringLiteral("whitePlayerCard"));

    for (const auto& name : {QStringLiteral("turnLabelBlack"), QStringLiteral("turnLabelWhite")}) {
        if (findChild<QLabel*>(name)) continue;
        auto* label = new QLabel(tr("手番"), this);
        label->setObjectName(name);
        label->setAlignment(Qt::AlignCenter);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->setContentsMargins(0, 0, 0, 0);
        label->setMargin(0);
        label->setWordWrap(false);
        label->setTextFormat(Qt::PlainText);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        label->hide();
    }
}

void ShogiView::relayoutTurnLabels()
{
    ensureTurnLabels();
    if (!m_blackNameLabel || !m_blackClockLabel || !m_whiteNameLabel || !m_whiteClockLabel) return;

    const QSize fs = fieldSize().isValid() ? fieldSize()
        : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
    const int padding = qMax(3, qRound(fs.width() * 0.10));
    const int gap = qMax(2, qRound(fs.height() * 0.04));
    const int standGap = qMax(4, qRound(fs.height() * 0.08));

    QFont nameFont = font();
    nameFont.setPixelSize(qMax(8, qRound(fs.height() * m_layout.nameFontScale() * 0.8)));
    QFont badgeFont = font();
    badgeFont.setPixelSize(qMax(8, qRound(fs.height() * 0.16)));
    badgeFont.setBold(true);
    // 両者とも2行分を確保し、長い名前でも残り時間と重ならないようにする。
    // macOS ではラベルの書体が盤面の書体と異なる（行の高さも違う）ため、
    // 設定後にラベルが実際に使う書体で測る。
    m_blackNameLabel->setFont(nameFont);
    m_whiteNameLabel->setFont(nameFont);
    const int nameLineHeight = qMax(m_blackNameLabel->fontMetrics().height(),
                                    m_whiteNameLabel->fontMetrics().height());
    const int nameHeight = nameLineHeight * 2 + 2;
    const int badgeHeight = QFontMetrics(badgeFont).height() + 4;
    const int clockHeight = qMax(nameLineHeight + 4, qRound(fs.height() * 0.42));
    // 非手番側にもバッジの余白を確保し、手番交代でカードや文字が動かないようにする。
    const int cardHeight = padding * 2 + badgeHeight + gap + nameHeight
        + (m_clockEnabled ? gap + clockHeight : 0);

    auto placeCard = [&](QFrame* card, ElideLabel* name, QLabel* clock, QLabel* badge,
                         const QRect& stand, bool above) {
        if (!stand.isValid()) {
            card->hide();
            name->hide();
            clock->hide();
            badge->hide();
            return;
        }
        const int top = above ? stand.top() - standGap - cardHeight
                              : stand.bottom() + 1 + standGap;
        const QRect cardRect(stand.left(), qBound(0, top, qMax(0, height() - cardHeight)),
                             stand.width(), cardHeight);
        card->setGeometry(cardRect);
        card->show();
        card->lower();

        const int x = cardRect.left() + padding;
        const int contentWidth = qMax(1, cardRect.width() - padding * 2);
        int y = cardRect.top() + padding;
        badge->setFont(badgeFont);
        const int badgeWidth = qMin(contentWidth, QFontMetrics(badgeFont).horizontalAdvance(badge->text()) + padding * 2);
        badge->setGeometry(x, y, badgeWidth, badgeHeight);
        fitLabelFontToRect(badge, badge->text(), badge->geometry(), 2);
        y += badgeHeight + gap;

        name->setGeometry(x, y, contentWidth, nameHeight);
        name->show();
        y += nameHeight + gap;

        clock->setGeometry(x, y, contentWidth, clockHeight);
        fitLabelFontToRect(clock, clock->text(), clock->geometry(), 2);
        clock->setVisible(m_clockEnabled);
        name->raise();
        clock->raise();
        badge->raise();
    };

    placeCard(m_blackPlayerCard, m_blackNameLabel, m_blackClockLabel,
              findChild<QLabel*>(QStringLiteral("turnLabelBlack")), blackStandBoundingRect(), !flipMode());
    placeCard(m_whitePlayerCard, m_whiteNameLabel, m_whiteClockLabel,
              findChild<QLabel*>(QStringLiteral("turnLabelWhite")), whiteStandBoundingRect(), flipMode());
    relayoutEditExitButton();
    relayoutPieceBoxSideSelector();
}

void ShogiView::updateTurnIndicator(ShogiGameController::Player now)
{
    relayoutTurnLabels();
    const bool black = now != ShogiGameController::Player2;
    const Urgency urgency = m_highlighting->blackActive() == black
        ? m_highlighting->urgency() : Urgency::Normal;
    m_highlighting->setActiveIsBlack(black);
    m_highlighting->setUrgencyVisuals(urgency);
}

void ShogiView::ensureAndPlaceEditExitButton()
{
    ensureTurnLabels();

    QLabel* bn = m_blackNameLabel;
    QLabel* wn = m_whiteNameLabel;
    if (!bn) bn = this->findChild<QLabel*>(QStringLiteral("blackNameLabel"));
    if (!wn) wn = this->findChild<QLabel*>(QStringLiteral("whiteNameLabel"));

    QPushButton* exitBtn = this->findChild<QPushButton*>(QStringLiteral("editExitButton"));
    if (!exitBtn) {
        exitBtn = new QPushButton(tr("編集終了"), this);
        exitBtn->setObjectName(QStringLiteral("editExitButton"));
        exitBtn->setToolTip(tr("局面編集を終了し、この局面を開始局面にします"));
        exitBtn->setVisible(false);
        exitBtn->setFocusPolicy(Qt::NoFocus);
        exitBtn->setCursor(Qt::PointingHandCursor);
        exitBtn->setAutoDefault(false);
        exitBtn->setDefault(false);
        exitBtn->setFlat(false);
        styleEditExitButton(exitBtn);
        exitBtn->raise();
    }

    // ── 右側の対局者カード（駒台と同じ幅）に揃える。カードが無いときは名前ラベル ──
    QLabel* base = nullptr;
    if (bn && wn) {
        const int bx = bn->geometry().center().x();
        const int wx = wn->geometry().center().x();
        base = (bx > wx) ? bn : wn;
    } else {
        base = bn ? bn : wn;
    }
    QFrame* card = (base && base == bn) ? m_blackPlayerCard : (base ? m_whitePlayerCard : nullptr);

    QRect baseGeo;
    if (card && card->isVisible() && card->geometry().isValid()) {
        baseGeo = card->geometry();
    } else if (base) {
        baseGeo = base->geometry();
    } else {
        if (m_board) {
            const QSize fs = fieldSize().isValid() ? fieldSize()
                                                   : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
            const QRect boardRect(m_layout.offsetX(), m_layout.offsetY(),
                                  fs.width() * m_board->files(),
                                  fs.height() * m_board->ranks());
            const int sideGap  = 8;
            const int sideWide = 160;
            baseGeo = QRect(boardRect.right() + 1 + sideGap,
                            boardRect.top(), sideWide, 1);
        } else {
            baseGeo = QRect(this->width() - 180, 10, 160, 1);
        }
    }

    // 書体とサイズを名前ラベルに合わせ、太字にする。
    QFont buttonFont = base ? base->font() : font();
    buttonFont.setBold(true);
    exitBtn->setFont(buttonFont);

    int x = baseGeo.x();
    int w = baseGeo.width();
    const int maxW = this->width() - x - 4;
    if (w > maxW) w = maxW;

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
    int  y = 0;
    bool yFixed = false;

    if (m_board) {
        const QSize fs = fieldSize().isValid() ? fieldSize()
                                               : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
        const QRect boardRect(m_layout.offsetX(), m_layout.offsetY(),
                              fs.width() * m_board->files(),
                              fs.height() * m_board->ranks());
        y = boardRect.top();
        yFixed = true;
    }

    if (!yFixed) {
        const int vGap = 4;
        y = baseGeo.y() - hBtn - vGap;
        if (y < 0) y = 0;
    }

    exitBtn->setGeometry(x, y, w, hBtn);
}

void ShogiView::relayoutEditExitButton()
{
    ensureAndPlaceEditExitButton();
}

// 「局面編集終了」ボタンの見た目を設定（ダイアログの決定ボタンと同じ青系。駒台のカードと同じ角丸）
void ShogiView::styleEditExitButton(QPushButton* btn)
{
    if (!btn) return;

    btn->setStyleSheet(QStringLiteral(
        "QPushButton#editExitButton {"
        "  color: #ffffff;"
        "  border: 1px solid #155ea8;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a86de, stop:1 #1a6fc6);"
        "}"
        "QPushButton#editExitButton:hover {"
        "  border-color: #1a69b8;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a94e8, stop:1 #237bd3);"
        "}"
        "QPushButton#editExitButton:pressed {"
        "  border-color: #114c8c;"
        "  padding-top: 5px; padding-bottom: 3px;"
        "  background: #165fae;"
        "}"
        "QPushButton#editExitButton[compact=\"true\"] { padding: 3px 3px; }"
        "QPushButton#editExitButton[compact=\"true\"]:pressed { padding-top: 4px; padding-bottom: 2px; }"
        "QPushButton#editExitButton:disabled {"
        "  color: #eceff1;"
        "  border-color: #90a4ae;"
        "  background: #b0bec5;"
        "}"));

    // 盤の上に浮かせる薄い影
    auto* shadow = new QGraphicsDropShadowEffect(btn);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 70));
    btn->setGraphicsEffect(shadow);
}

// ボタンの文字列が maxWidth に必ず収まるよう、フォントサイズを自動調整（縮小のみ）。
// まずチェック印付きで縮め、入らなければ印を外して余白を詰め（compact）、さらに縮める。
void ShogiView::fitEditExitButtonFont(QPushButton* btn, int maxWidth)
{
    if (!btn || maxWidth <= 20) return;

    QFont f = btn->font();
    int pixel = QFontInfo(f).pixelSize();
    if (pixel <= 0) pixel = 16;

    auto withSize = [&f](int px) { QFont tf = f; tf.setPixelSize(px); return tf; };
    auto textWidth = [btn](const QFont& tf) { return QFontMetrics(tf).horizontalAdvance(btn->text()); };
    // 通常: 左右の余白10px・枠1px、チェック印と間隔6px
    auto fitsWithIcon = [&](const QFont& tf) { return textWidth(tf) + editExitIconSize(tf) + 6 + 22 <= maxWidth; };
    // 詰めた表示: 左右の余白3px・枠1px、印なし
    auto fitsCompact = [&](const QFont& tf) { return textWidth(tf) + 8 <= maxWidth; };

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
