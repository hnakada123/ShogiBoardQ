/// @file shogiview_labels.cpp
/// @brief ShogiView のラベル管理（時計・名前）

#include "shogiview.h"
#include "shogiboard.h"
#include "elidelabel.h"
#include "logcategories.h"

#include <QLabel>
#include <QFont>
#include <QFontMetrics>

// ラベルの表示矩形（rect）にテキスト（text）が収まるよう、フォントサイズを自動調整する。
void ShogiView::fitLabelFontToRect(QLabel* label, const QString& text,
                                   const QRect& rect, int paddingPx)
{
    if (!label) return;

    const QRect inner = rect.adjusted(paddingPx, paddingPx, -paddingPx, -paddingPx);

    double lo = 6.0;
    double hi = 200.0;

    QFont f = label->font();

    for (int i = 0; i < 18; ++i) {
        const double mid = (lo + hi) * 0.5;
        f.setPointSizeF(mid);

        QFontMetrics fm(f);
        const int w = fm.horizontalAdvance(text);
        const int h = fm.height();

        if (w <= inner.width() && h <= inner.height()) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    f.setPointSizeF(lo);
    label->setFont(f);
}

// 先手（黒）側：名前・時計ラベルの位置とサイズを更新する。
void ShogiView::updateBlackClockLabelGeometry()
{
    // ラベル未生成なら何もしない
    if (!m_blackClockLabel || !m_blackNameLabel) return;

    // 駒台の基準矩形が無効なら非表示
    const QRect stand = blackStandBoundingRect();
    if (!stand.isValid()) {
        m_blackClockLabel->hide();
        m_blackNameLabel->hide();
        return;
    }

    // マス寸法（無効時はフォールバック）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));

    // 配置用パラメータ
    const int marginOuter = 4;  // 駒台との外側マージン
    const int marginInner = 2;  // 名前と時計の間のマージン
    const int nameH  = qMax(int(fs.height() * 0.8), fs.height());
    const int clockH = qMax(int(fs.height() * 0.9), fs.height());

    const int x = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;
    const int yBelow = stand.bottom() + 1 + marginOuter;

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else {
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    // ジオメトリ反映
    m_blackNameLabel->setGeometry(nameRect);
    m_blackClockLabel->setGeometry(clockRect);

    // フォント調整
    fitLabelFontToRect(m_blackClockLabel, m_blackClockLabel->text(), clockRect, 2);
    QFont f = m_blackNameLabel->font();
    f.setPointSizeF(qMax(8.0, fs.height() * m_layout.nameFontScale()));
    m_blackNameLabel->setFont(f);

    // 前面に出して表示
    m_blackNameLabel->raise();
    m_blackNameLabel->show();

    if (m_clockEnabled) {
        m_blackClockLabel->raise();
        m_blackClockLabel->show();
    } else {
        m_blackClockLabel->hide();
    }
}

// 後手（白）側：名前・時計ラベルの位置とサイズを更新する。
void ShogiView::updateWhiteClockLabelGeometry()
{
    if (!m_whiteClockLabel || !m_whiteNameLabel) return;

    const QRect stand = whiteStandBoundingRect();
    if (!stand.isValid()) {
        m_whiteClockLabel->hide();
        m_whiteNameLabel->hide();
        return;
    }

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));

    const int marginOuter = 4;
    const int marginInner = 2;
    const int nameH  = qMax(int(fs.height() * 0.8), fs.height());
    const int clockH = qMax(int(fs.height() * 0.9), fs.height());

    const int x      = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;
    const int yBelow = stand.bottom() + 1 + marginOuter;

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else {
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    m_whiteNameLabel->setGeometry(nameRect);
    m_whiteClockLabel->setGeometry(clockRect);

    fitLabelFontToRect(m_whiteClockLabel, m_whiteClockLabel->text(), clockRect, 2);
    QFont f = m_blackNameLabel->font(); // 黒側の設定をベースに統一感を保つ
    f.setPointSizeF(qMax(8.0, fs.height() * m_layout.nameFontScale()));
    m_whiteNameLabel->setFont(f);

    m_whiteNameLabel->raise();
    m_whiteNameLabel->show();

    if (m_clockEnabled) {
        m_whiteClockLabel->raise();
        m_whiteClockLabel->show();
    } else {
        m_whiteClockLabel->hide();
    }
}

// 先手（黒）側の時計表示テキストを設定する。
void ShogiView::setBlackClockText(const QString& text)
{
    if (!m_blackClockLabel) return;
    m_blackClockLabel->setText(text);
    fitLabelFontToRect(m_blackClockLabel, text, m_blackClockLabel->geometry(), 2);
}

// 後手（白）側の時計表示テキストを設定する。
void ShogiView::setWhiteClockText(const QString& text)
{
    if (!m_whiteClockLabel) return;
    m_whiteClockLabel->setText(text);
    fitLabelFontToRect(m_whiteClockLabel, text, m_whiteClockLabel->geometry(), 2);
}

// 名前ラベル用のフォント倍率（スケール）を設定する。
void ShogiView::setNameFontScale(double scale)
{
    m_layout.setNameFontScale(scale);
    updateBlackClockLabelGeometry();
}

// 文字列から特定のマーク（▲▼▽△）をすべて除去し、前後の空白をトリムして返す。
QString ShogiView::stripMarks(const QString& s)
{
    QString t = s;

    static const QChar marks[] = {
        QChar(0x25B2), // ▲
        QChar(0x25BC), // ▼
        QChar(0x25BD), // ▽
        QChar(0x25B3)  // △
    };

    for (QChar m : marks)
        t.remove(m);

    return t.trimmed();
}

// 先手（黒）側のプレイヤー名を設定する。
void ShogiView::setBlackPlayerName(const QString& name)
{
    qCDebug(lcView).noquote() << "setBlackPlayerName:" << name;
    m_blackNameBase = stripMarks(name);
    refreshNameLabels();
}

// 後手（白）側のプレイヤー名を設定する。
void ShogiView::setWhitePlayerName(const QString& name)
{
    qCDebug(lcView).noquote() << "setWhitePlayerName:" << name;
    m_whiteNameBase = stripMarks(name);
    refreshNameLabels();
}

// プレイヤー名ラベル（先手/後手）の表示内容とツールチップを最新状態に更新する。
void ShogiView::refreshNameLabels()
{
    // ── 先手（黒）側 ──
    if (m_blackNameLabel) {
        const QString markBlack = m_layout.flipMode() ? QStringLiteral("▼") : QStringLiteral("▲");
        m_blackNameLabel->setFullText(markBlack + m_blackNameBase);

        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        m_blackNameLabel->setToolTip(mkTip(m_blackNameBase));
    }

    // ── 後手（白）側 ──
    if (m_whiteNameLabel) {
        const QString markWhite = m_layout.flipMode() ? QStringLiteral("△") : QStringLiteral("▽");
        m_whiteNameLabel->setFullText(markWhite + m_whiteNameBase);

        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        m_whiteNameLabel->setToolTip(mkTip(m_whiteNameBase));
    }
}
