#include "globaltooltip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QString>
// 影を付けたい場合だけ有効化
// #include <QGraphicsDropShadowEffect>

GlobalToolTip::GlobalToolTip(QWidget* parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    // 見た目（枠線なし・背景不透過・角丸）
    setStyleSheet(
        "background-color:#FFF9C4;"
        "color:#333;"
        "padding:10px 12px;"
        "border-radius:6px;"
        );

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);

    m_label = new QLabel(this);
    m_label->setFrameStyle(QFrame::NoFrame);
    lay->addWidget(m_label);

    // 初期フォントサイズ（お好みで）
    setPointSizeF(14.0);

    // 任意：さりげないドロップシャドウ
    // auto *eff = new QGraphicsDropShadowEffect(this);
    // eff->setBlurRadius(12);
    // eff->setOffset(0, 2);
    // eff->setColor(QColor(0,0,0,80));
    // setGraphicsEffect(eff);
}

void GlobalToolTip::setPointSizeF(qreal pt)
{
    QFont f = m_label->font();
    f.setPointSizeF(pt);
    m_label->setFont(f);

    // 一部スタイル環境でフォントが勝手に上書きされる対策として念押し
    m_label->setStyleSheet(QStringLiteral("font-size:%1pt;").arg(pt));

    adjustSize();
}

void GlobalToolTip::showText(const QPoint& globalPos, const QString& plainText)
{
    // HTML無効化（素の文字列として表示）
    m_label->setText(plainText.toHtmlEscaped());
    adjustSize();

    // 少し右下にオフセット
    move(globalPos + QPoint(12, 16));
    show();
}

void GlobalToolTip::hideTip()
{
    hide();
}

void GlobalToolTip::setCompact(bool on)
{
    // 小さめ設定
    const int padV   = on ? 4 : 6;   // 上下 padding
    const int padH   = on ? 4 : 6;   // 左右 padding
    const int radius = on ? 4 : 6;    // 角丸
    const qreal pt   = on ? 12.0 : 14.0;  // フォント

    setPointSizeF(pt); // フォント更新（内部で adjustSize() も呼びます）

    // レイアウト余白も合わせて詰める（ここを忘れると縮みません）
    if (auto* lay = qobject_cast<QHBoxLayout*>(layout()))
        lay->setContentsMargins(padH, padV, padH, padV);

    // フレームの padding/角丸を更新
    setStyleSheet(QStringLiteral(
                      "background-color:#FFF9C4;"
                      "color:#333;"
                      "padding:%1px %2px;"
                      "border-radius:%3px;"
                      ).arg(padV).arg(padH).arg(radius));

    adjustSize();
}
