#ifndef STEPBUTTONS_H
#define STEPBUTTONS_H

/// @file stepbuttons.h
/// @brief 局面送りの6つのボタンの列（棋譜欄は縦、評価値グラフは横）

#include <QList>
#include <QWidget>
#include <array>

class QPushButton;

/**
 * @brief 局面送りの6つのボタン（最初・10手戻る・1手戻る・1手進む・10手進む・最後）を並べた部品
 *
 * 棋譜欄（縦。▲▼）と評価値グラフ（横。◀▶）で共通。ボタンは向きの記号だけを持ち、押したときの操作・
 * 説明（ツールチップ）・有効と無効は、結び付ける QAction（ui の KifuNavigationController::bindButtons）から付ける。
 */
class StepButtons : public QWidget
{
    Q_OBJECT

public:
    /// ボタンの並び（KifuNavigationController::Step と同じ）
    enum Step { First, Back10, Prev, Next, Fwd10, Last, Count };

    /// orientation の向きに並べる。ボタンの objectName は namePrefix に "First"・"Back10" などを付けたもの
    StepButtons(Qt::Orientation orientation, const QString& namePrefix, QWidget* parent = nullptr);

    QPushButton* button(Step step) const { return m_buttons.at(static_cast<std::size_t>(step)); }
    QPushButton* firstButton() const { return button(First); }
    QPushButton* back10Button() const { return button(Back10); }
    QPushButton* prevButton() const { return button(Prev); }
    QPushButton* nextButton() const { return button(Next); }
    QPushButton* fwd10Button() const { return button(Fwd10); }
    QPushButton* lastButton() const { return button(Last); }
    /// 6つのボタン（Step の順）
    QList<QPushButton*> buttons() const { return {m_buttons.begin(), m_buttons.end()}; }

private:
    std::array<QPushButton*, Count> m_buttons{};
};

#endif // STEPBUTTONS_H
