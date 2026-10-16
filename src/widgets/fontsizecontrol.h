#ifndef FONTSIZECONTROL_H
#define FONTSIZECONTROL_H

/// @file fontsizecontrol.h
/// @brief A-/A+ のボタンの組（見た目・説明・範囲の端での無効化）

#include <QAbstractButton>
#include <QObject>
#include <QPointer>

class FontSizeHelper;
class QToolButton;
class QWidget;

/**
 * @brief 文字の大きさの A-/A+ のボタン2つを FontSizeHelper に結ぶ
 *
 * ボタンの文字・見た目・ツールチップ・読み上げ名と、範囲の端（FontSizeHelper の最小・最大）で押せなくする
 * 処理はここだけで決める。ダイアログ（DialogFontScale）もドックのパネルも同じ部品を使う。
 * ボタンは呼び出し側が置き（.ui のボタンでもよい）、objectName は呼び出し側が決める。
 */
class FontSizeControl : public QObject
{
    Q_OBJECT

public:
    /// ボタンに付ける役割のプロパティ（値は kDecreaseRole か kIncreaseRole。表示の文字によらず探せる）
    static constexpr const char* kRoleProperty = "fontSizeControl";
    static constexpr const char* kDecreaseRole = "decrease";
    static constexpr const char* kIncreaseRole = "increase";

    /// ボタンの大きさの決め方
    enum class Sizing {
        Panel,    ///< 呼び出し側が決める（ドックのツールバーなど）
        Dialog,   ///< 文字に合わせて幅と高さを決める（ダイアログの操作列）
    };

    /// decrease・increase を helper に結ぶ（この部品は helper の子になる）
    FontSizeControl(QAbstractButton* decrease, QAbstractButton* increase, FontSizeHelper* helper,
                    Sizing sizing = Sizing::Panel);

    /// A-/A+ の QToolButton を作って結ぶ（objectName はそれぞれ decreaseName・increaseName）
    static FontSizeControl* createToolButtons(QWidget* parent, FontSizeHelper* helper, const QString& decreaseName,
                                              const QString& increaseName);

    QAbstractButton* decreaseButton() const { return m_decrease; }
    QAbstractButton* increaseButton() const { return m_increase; }

public slots:
    /// 押せるか・大きさを今の文字の大きさに合わせる
    void refresh();

private:
    QPointer<QAbstractButton> m_decrease;
    QPointer<QAbstractButton> m_increase;
    FontSizeHelper* m_helper = nullptr;
    Sizing m_sizing = Sizing::Panel;
};

#endif // FONTSIZECONTROL_H
