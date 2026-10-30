/// @file stepbuttons.cpp
/// @brief 局面送りの6つのボタンの列の実装

#include "stepbuttons.h"

#include "panelbutton.h"

#include <QBoxLayout>
#include <QPushButton>

StepButtons::StepButtons(Qt::Orientation orientation, const QString& namePrefix, QWidget* parent)
    : QWidget(parent)
{
    struct Spec {
        const char* name;
        const char* vertical;     ///< 棋譜欄（手が上から下へ進む）
        const char* horizontal;   ///< 評価値グラフ（横軸が手数）
    };
    // 横は ◀▶ で向きを表す。⏮⏭ は多くの日本語フォントに無いため、読み筋の盤面と同じく
    // ◀▶ と縦棒を組み合わせて記号の大きさを揃える
    static constexpr Spec kSpecs[Count] = {
        {"First", "▲|", "|◀"},
        {"Back10", "▲▲", "◀◀"},
        {"Prev", "▲", "◀"},
        {"Next", "▼", "▶"},
        {"Fwd10", "▼▼", "▶▶"},
        {"Last", "▼|", "▶|"},
    };
    auto* layout = new QBoxLayout(orientation == Qt::Vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight,
                                  this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        const Spec& spec = kSpecs[i];
        auto* button = new QPushButton(
            QString::fromUtf8(orientation == Qt::Vertical ? spec.vertical : spec.horizontal), this);
        button->setObjectName(namePrefix + QLatin1String(spec.name));
        PanelButton::setup(button);
        layout->addWidget(button, 0, orientation == Qt::Vertical ? Qt::AlignHCenter : Qt::AlignVCenter);
        m_buttons[i] = button;
    }
}
