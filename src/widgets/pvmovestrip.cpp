/// @file pvmovestrip.cpp
/// @brief 読み筋の手を並べ、表示中の局面の手を示す帯の実装

#include "pvmovestrip.h"

#include "flowlayout.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QColor>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace {

constexpr int kStripMargin = 8;
constexpr int kStripMinimumHeight = 60;
constexpr int kLineSpacing = 2;
constexpr int kWordPaddingX = 4;
constexpr int kWordPaddingY = 3;

/// 今の局面の語の塗り。盤の移動先のハイライト（半透明の橙）が木の盤の上で見える色で、文字と盤の手を対応させる
constexpr QRgb kCurrentFill = 0xE8A058;
constexpr QRgb kCurrentText = 0x1A1A1A;
constexpr QRgb kPlayedText = 0x232629;
constexpr QRgb kUpcomingText = 0x666B73;

/// 局面1つの語。今の局面かは checked で持つ。背景の塗りだけで示し、太さを変えないので語の幅は変わらない
class PvMoveWord final : public QAbstractButton
{
public:
    PvMoveWord(const QString& label, QWidget* parent)
        : QAbstractButton(parent)
    {
        setText(label);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        // キーボードでは局面送りのボタンが同じ操作をする。長い読み筋で Tab が手の数だけ止まらないようにする
        setFocusPolicy(Qt::NoFocus);
    }

    void setPlayed(bool played)
    {
        if (m_played == played) return;
        m_played = played;
        update();
    }

    QSize sizeHint() const override
    {
        ensurePolished();
        const QFontMetrics metrics = fontMetrics();
        return {metrics.horizontalAdvance(text()) + 2 * kWordPaddingX, metrics.height() + 2 * kWordPaddingY};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        if (isChecked()) painter.fillRect(rect(), QColor(kCurrentFill));
        painter.setPen(QColor(isChecked() ? kCurrentText : m_played ? kPlayedText : kUpcomingText));
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextSingleLine, text());
    }

private:
    bool m_played = false;
};

} // namespace

PvMoveStrip::PvMoveStrip(const QStringList& labels, const QString& ending, QWidget* parent)
    : QWidget(parent)
    , m_buttons(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("pvMoveStrip"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("#pvMoveStrip { background-color: #f5f5f5; border: 1px solid #ccc; }"));
    setMinimumHeight(kStripMinimumHeight);

    // 1行のときは帯の上下の中央に置く
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kStripMargin, kStripMargin, kStripMargin, kStripMargin);
    layout->setSpacing(0);
    auto* flow = new FlowLayout(0, 0, kLineSpacing);
    layout->addStretch();
    layout->addLayout(flow);
    layout->addStretch();

    for (int index = 0; index < labels.size(); ++index) {
        auto* word = new PvMoveWord(labels.at(index), this);
        word->setObjectName(QStringLiteral("pvMove%1").arg(index));
        m_buttons->addButton(word, index);
        flow->addWidget(word);
    }
    // 手のない語は1語ずつ置く（局面と合わない読み筋では語が続くので、まとめると折り返せない）
    const QStringList endingWords = ending.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& text : endingWords) {
        auto* word = new QLabel(this);
        word->setObjectName(QStringLiteral("pvMoveEnding"));
        word->setTextFormat(Qt::PlainText);   // エンジンの出力を HTML として読まない
        word->setText(text);
        word->setContentsMargins(kWordPaddingX, kWordPaddingY, kWordPaddingX, kWordPaddingY);
        word->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QColor(kUpcomingText).name()));
        flow->addWidget(word);
    }

    connect(m_buttons, &QButtonGroup::idClicked, this, &PvMoveStrip::positionRequested);
    setCurrent(0);
}

QSize PvMoveStrip::sizeHint() const
{
    // FlowLayout の推奨の幅は一番長い語の幅で、そこから高さを求めると語が縦に1列に並ぶ高さになる。
    // 帯の幅は外（ウィンドウ）で決まり、高さはその幅の heightForWidth で決まるので、推奨は最小の大きさでよい
    return minimumSizeHint().expandedTo(minimumSize());
}

void PvMoveStrip::setCurrent(int index)
{
    QAbstractButton* current = m_buttons->button(index);
    if (!current) return;
    current->setChecked(true);
    const auto words = m_buttons->buttons();
    for (QAbstractButton* word : words) static_cast<PvMoveWord*>(word)->setPlayed(m_buttons->id(word) < index);
}
