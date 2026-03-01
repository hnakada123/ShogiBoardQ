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
#include <QRegularExpression>
#include <QSizePolicy>

// 角丸(border-radius)を 0px に強制するユーティリティ
static QString ensureNoBorderRadiusStyle(const QString& base)
{
    QString s = base;
    static const QRegularExpression re(R"(border-radius\s*:\s*[^;]+;?)",
                                       QRegularExpression::CaseInsensitiveOption);
    if (re.match(s).hasMatch()) {
        s.replace(re, "border-radius:0px;");
    } else {
        if (!s.isEmpty() && !s.trimmed().endsWith(';')) s.append(';');
        s.append("border-radius:0px;");
    }
    return s;
}

// スタイルシートからフォントサイズを除去するユーティリティ（setFont()でサイズ調整するため）
static QString removeFontSizeFromStyle(const QString& base)
{
    QString s = base;
    // font-size プロパティを除去
    static const QRegularExpression re(R"(font-size\s*:\s*[^;]+;?)",
                                       QRegularExpression::CaseInsensitiveOption);
    s.remove(re);
    return s;
}

static void enforceSquareCorners(QLabel* lab)
{
    if (!lab) return;
    lab->setStyleSheet(ensureNoBorderRadiusStyle(lab->styleSheet()));
}

// 手番ラベルを（無ければ）生成して初期設定する。
void ShogiView::ensureTurnLabels()
{
    // 先手用
    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    if (!tlBlack) {
        tlBlack = new QLabel(tr("次の手番"), this);
        tlBlack->setObjectName(QStringLiteral("turnLabelBlack"));
        tlBlack->setAlignment(Qt::AlignCenter);
        tlBlack->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tlBlack->setContentsMargins(0, 0, 0, 0);
        tlBlack->setMargin(0);
        tlBlack->setWordWrap(false);
        tlBlack->setTextFormat(Qt::PlainText);
        tlBlack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        tlBlack->hide();
        if (m_blackNameLabel) {
            QString style = removeFontSizeFromStyle(m_blackNameLabel->styleSheet());
            tlBlack->setStyleSheet(ensureNoBorderRadiusStyle(style));
        }
    }

    // 後手用
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    if (!tlWhite) {
        tlWhite = new QLabel(tr("次の手番"), this);
        tlWhite->setObjectName(QStringLiteral("turnLabelWhite"));
        tlWhite->setAlignment(Qt::AlignCenter);
        tlWhite->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tlWhite->setContentsMargins(0, 0, 0, 0);
        tlWhite->setMargin(0);
        tlWhite->setWordWrap(false);
        tlWhite->setTextFormat(Qt::PlainText);
        tlWhite->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        tlWhite->hide();
        if (m_whiteClockLabel) {
            QString style = removeFontSizeFromStyle(m_whiteClockLabel->styleSheet());
            tlWhite->setStyleSheet(ensureNoBorderRadiusStyle(style));
        }
    }
}

void ShogiView::relayoutTurnLabels()
{
    ensureTurnLabels();

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));

    QLabel* bn = m_blackNameLabel;  // 先手（黒）名前
    QLabel* bc = m_blackClockLabel; // 先手（黒）時計
    QLabel* wn = m_whiteNameLabel;  // 後手（白）名前
    QLabel* wc = m_whiteClockLabel; // 後手（白）時計
    if (!bn || !bc || !wn || !wc || !tlBlack || !tlWhite) return;

    // 角丸を完全無効化（見た目を統一）
    auto safeSquare = [](QLabel* lab){ if (lab) enforceSquareCorners(lab); };
    safeSquare(bn); safeSquare(bc); safeSquare(wn); safeSquare(wc);
    safeSquare(tlBlack); safeSquare(tlWhite);

    // 1マス寸法（fallbackあり）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));

    // 名前ラベルのフォントを（双方）同一スケールで調整
    {
        QFont f = bn->font();
        f.setPointSizeF(qMax(8.0, fs.height() * m_layout.nameFontScale()));
        bn->setFont(f);
        wn->setFont(f);
    }

    // ラベル高さの安全取得（フォントメトリクスベースで最小限に）
    auto hLab = [&](QLabel* lab, int /*fallbackPx*/)->int {
        if (!lab) return static_cast<int>(fs.height() * 0.5);
        QFontMetrics fm(lab->font());
        return fm.height() + 4;
    };

    const int marginOuter = 2;
    const int marginInner = 1;

    // 駒台外接矩形
    const QRect standBlack = blackStandBoundingRect();
    const QRect standWhite = whiteStandBoundingRect();
    if (!standBlack.isValid() || !standWhite.isValid()) return;

    const int BW = standBlack.width();
    const int BX = standBlack.left();
    const int WW = standWhite.width();
    const int WX = standWhite.left();

    // 各高さ
    const int HnBlack = hLab(bn, fs.height());
    const int HcBlack = m_clockEnabled ? hLab(bc, fs.height()) : 0;
    const int HtBlack = hLab(tlBlack, fs.height());

    const int HnWhite = hLab(wn, fs.height());
    const int HcWhite = m_clockEnabled ? hLab(wc, fs.height()) : 0;
    const int HtWhite = hLab(tlWhite, fs.height());

    // 直下に縦積み（駒台の下）
    auto stackBelowStand = [&](const QRect& stand, int X, int W,
                               QLabel* lab1, int H1,
                               QLabel* lab2, int H2,
                               QLabel* lab3, int H3)
    {
        int y = stand.bottom() + 1 + marginOuter;
        QRect r1(X, y, W, H1);
        QRect r2, r3;

        if (H2 > 0) {
            r2 = QRect(X, r1.bottom() + marginInner, W, H2);
            r3 = QRect(X, r2.bottom() + marginInner, W, H3);
        } else {
            r2 = QRect(X, r1.bottom(), W, 0);
            r3 = QRect(X, r1.bottom() + marginInner, W, H3);
        }

        int overflow = (r3.bottom() + marginOuter) - height();
        if (overflow > 0) { r1.translate(0,-overflow); r2.translate(0,-overflow); r3.translate(0,-overflow); }

        lab1->setGeometry(r1);
        lab2->setGeometry(r2);
        lab3->setGeometry(r3);

        if (H2 > 0 && (lab2 == m_blackClockLabel || lab2 == m_whiteClockLabel)) {
            fitLabelFontToRect(lab2, lab2->text(), r2, 2);
        }
        if (H3 > 0 && (lab3 == tlBlack || lab3 == tlWhite)) {
            fitLabelFontToRect(lab3, lab3->text(), r3, 2);
        }
    };

    // 直上に縦積み（駒台の上）
    auto stackAboveStand = [&](const QRect& stand, int X, int W,
                               QLabel* lab1, int H1,
                               QLabel* lab2, int H2,
                               QLabel* lab3, int H3)
    {
        int totalH;
        if (H3 > 0) {
            totalH = H1 + marginInner + H2 + marginInner + H3;
        } else {
            totalH = H1 + marginInner + H2;
        }
        int yTop = stand.top() - 1 - marginOuter - totalH;
        if (yTop < 0) yTop = 0;

        QRect r1(X, yTop, W, H1);
        QRect r2(X, r1.bottom() + marginInner, W, H2);
        QRect r3;

        if (H3 > 0) {
            r3 = QRect(X, r2.bottom() + marginInner, W, H3);
        } else {
            r3 = QRect(X, r2.bottom(), W, 0);
        }

        lab1->setGeometry(r1);
        lab2->setGeometry(r2);
        lab3->setGeometry(r3);

        if (H3 > 0 && (lab3 == m_blackClockLabel || lab3 == m_whiteClockLabel)) {
            fitLabelFontToRect(lab3, lab3->text(), r3, 2);
        }
        if (H1 > 0 && (lab1 == tlBlack || lab1 == tlWhite)) {
            fitLabelFontToRect(lab1, lab1->text(), r1, 2);
        }
    };

    if (!m_layout.flipMode()) {
        stackBelowStand(standWhite, WX, WW, wn, HnWhite, wc, HcWhite, tlWhite, HtWhite);
        stackAboveStand(standBlack, BX, BW, tlBlack, HtBlack, bn, HnBlack, bc, HcBlack);
    } else {
        stackBelowStand(standBlack, BX, BW, bn, HnBlack, bc, HcBlack, tlBlack, HtBlack);
        stackAboveStand(standWhite, WX, WW, tlWhite, HtWhite, wn, HnWhite, wc, HcWhite);
    }

    // Zオーダー（可視/不可視は変更しない）
    bn->raise(); bc->raise(); wn->raise(); wc->raise();
    tlBlack->raise(); tlWhite->raise();

    relayoutEditExitButton();
}

void ShogiView::updateTurnIndicator(ShogiGameController::Player now)
{
    if (!m_blackNameLabel || !m_whiteClockLabel) return;

    ensureTurnLabels();

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    if (!tlBlack || !tlWhite) return;

    tlBlack->hide();
    tlWhite->hide();

    relayoutTurnLabels();

    const auto side = (now == ShogiGameController::Player1 || now == ShogiGameController::Player2)
                          ? now : ShogiGameController::Player1;

    if (side == ShogiGameController::Player1) {
        tlBlack->show();
        tlBlack->raise();
    } else {
        tlWhite->show();
        tlWhite->raise();
    }
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
        exitBtn->setFont(base->font());
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
