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
#include <QSizePolicy>

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
    const int nameHeight = QFontMetrics(nameFont).height() + 2;
    const int badgeHeight = QFontMetrics(badgeFont).height() + 4;
    const int clockHeight = qMax(nameHeight + 2, qRound(fs.height() * 0.42));
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

        name->setFont(nameFont);
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
        exitBtn->setVisible(false);
        exitBtn->setFocusPolicy(Qt::NoFocus);
        exitBtn->setCursor(Qt::PointingHandCursor);
        exitBtn->setAutoDefault(false);
        exitBtn->setDefault(false);
        exitBtn->setFlat(false);
        exitBtn->raise();
    }

    const QString solidRedSS = QString::fromLatin1(R"(
        QPushButton#editExitButton {
            border: 1px solid #b40000;
            border-radius: 12px;
            padding: 4px 12px;
            color: #ffffff;
            font-weight: 600;
            background-color: #e00000;
        }
        QPushButton#editExitButton:hover {
            border: 1px solid #ff4444;
            background-color: #e00000;
        }
        QPushButton#editExitButton:pressed {
            padding-top: 5px; padding-bottom: 3px;
            border: 1px solid #8a0000;
            background-color: #e00000;
        }
        QPushButton#editExitButton:disabled {
            color: rgba(255,255,255,0.75);
            border-color: #9a0000;
            background-color: #e00000;
        }
    )");
    exitBtn->setStyleSheet(solidRedSS);

    // ── 右側の"名前ラベル"を基準に配置 ──
    QLabel* base = nullptr;
    if (bn && wn) {
        const int bx = bn->geometry().center().x();
        const int wx = wn->geometry().center().x();
        base = (bx > wx) ? bn : wn;
    } else {
        base = bn ? bn : wn;
    }

    QRect baseGeo;
    if (base) {
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

    // サイズは名前ラベルに合わせ、漢字の字形は日本語用フォントを優先する。
    QFont buttonFont = base ? base->font() : font();
    QStringList families = {
        QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Noto Sans JP"),
        QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo"),
        QStringLiteral("Hiragino Sans"), QStringLiteral("Hiragino Kaku Gothic ProN"),
        QStringLiteral("IPAGothic")
    };
    families.append(buttonFont.families());
    buttonFont.setFamilies(families);
    exitBtn->setFont(buttonFont);

    int x = baseGeo.x();
    int w = baseGeo.width();
    const int maxW = this->width() - x - 4;
    if (w > maxW) w = maxW;

    fitEditExitButtonFont(exitBtn, w);

    const int hBtn = qMax(exitBtn->sizeHint().height(), 28);
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

// 「局面編集終了」ボタンの見た目を設定
void ShogiView::styleEditExitButton(QPushButton* btn)
{
    if (!btn) return;

    btn->setStyleSheet(
        "QPushButton#editExitButton {"
        "  background: #e53935;"
        "  color: #ffffff;"
        "  border: 1px solid #8e0000;"
        "  padding: 6px 10px;"
        "  font-weight: 600;"
        "  border-radius: 0px;"
        "}"
        "QPushButton#editExitButton:hover {"
        "  background: #d32f2f;"
        "}"
        "QPushButton#editExitButton:pressed {"
        "  background: #b71c1c;"
        "}"
        "QPushButton#editExitButton:disabled {"
        "  background: #bdbdbd;"
        "  color: #ffffff;"
        "  border-color: #9e9e9e;"
        "}"
        );
}

// ボタンの文字列が maxWidth に必ず収まるよう、フォントサイズを自動調整（縮小のみ）
void ShogiView::fitEditExitButtonFont(QPushButton* btn, int maxWidth)
{
    if (!btn || maxWidth <= 20) return;

    const int inner = qMax(1, maxWidth - 24);

    QFont f = btn->font();
    int point = f.pointSize();
    int pixel = f.pixelSize();

    if (point <= 0 && pixel <= 0) {
        point = 12;
        f.setPointSize(point);
        btn->setFont(f);
    }

    auto fits = [&](const QFont& tf)->bool {
        QFontMetrics fm(tf);
        const int textW = fm.horizontalAdvance(btn->text());
        return textW <= inner;
    };

    if (fits(f)) {
        btn->setFont(f);
        return;
    }

    const int minPoint = 8;
    const int minPixel = 12;

    if (pixel > 0) {
        int sz = pixel;
        while (sz > minPixel) {
            QFont tf = f;
            tf.setPixelSize(--sz);
            if (fits(tf)) { btn->setFont(tf); return; }
        }
        f.setPixelSize(minPixel);
        btn->setFont(f);
    } else {
        int sz = point;
        while (sz > minPoint) {
            QFont tf = f;
            tf.setPointSize(--sz);
            if (fits(tf)) { btn->setFont(tf); return; }
        }
        f.setPointSize(minPoint);
        btn->setFont(f);
    }
}
