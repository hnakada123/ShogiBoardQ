#ifndef P2PSTATUSSTRIP_H
#define P2PSTATUSSTRIP_H

/// @file p2pstatusstrip.h
/// @brief P2P対局のウィンドウの上の、接続の状態の帯

#include <QFrame>

class QLabel;
class QPushButton;

/**
 * @brief 接続の状態と「接続」「切断」のボタンを、どのタブでも見える所に出す帯
 *
 * 「招待を作る」などが押せない理由（接続していない）と、接続の操作を、タブを移らずに分かるようにする。
 * 状態は、印の形（塗り・輪）と文の両方で示す（色だけにしない）。
 */
class P2pStatusStrip : public QFrame
{
    Q_OBJECT
public:
    enum class State { Offline, Starting, Online, Failed };

    explicit P2pStatusStrip(QWidget* parent = nullptr);

    /// 状態と、その下に出す説明（接続中は招待の経路、接続していなければ今の設定）
    void setStatus(State state, const QString& title, const QString& detail);
    QPushButton* button() const { return m_button; }

private:
    QLabel* m_mark = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_detail = nullptr;
    QPushButton* m_button = nullptr;
    State m_state = State::Offline;
    bool m_drawn = false;
};

#endif // P2PSTATUSSTRIP_H
