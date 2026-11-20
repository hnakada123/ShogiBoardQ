#ifndef P2PGAMEPANEL_H
#define P2PGAMEPANEL_H

/// @file p2pgamepanel.h
/// @brief P2P対局のウィンドウの「対局中」タブ（対局の状態と、対局を終える操作）

#include <QPointer>
#include <QWidget>

class P2pGameCoordinator;
class QLabel;
class QPushButton;

/**
 * @brief 対局の状態と、対局を終える操作
 *
 * 状態は、今の段階（手番・再接続中など）を見出しに、相手・先後・経路・切断時間の残りをその下に出す。終える操作は、
 * 結果の決まり方で3つに分ける: 負けを認める（投了）、相手と合意して終える（引き分け・中断の申し出）、一方的に
 * やめる（結果未確定）。司令塔は表示のために読むだけで、操作は意図のシグナルで知らせる。
 */
class P2pGamePanel : public QWidget
{
    Q_OBJECT
public:
    explicit P2pGamePanel(P2pGameCoordinator* game, QWidget* parent = nullptr);

signals:
    void resignRequested();
    void offerDrawRequested();
    void offerAbortRequested();
    void leaveGameRequested();

public slots:
    /// 状態の文とボタンの可否を、対局の今の状態に合わせる
    void refresh();

private:
    QPointer<P2pGameCoordinator> m_game;   ///< 非所有
    QLabel* m_headline = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_resign = nullptr;
    QPushButton* m_offerDraw = nullptr;
    QPushButton* m_offerAbort = nullptr;
    QPushButton* m_leave = nullptr;
};

#endif // P2PGAMEPANEL_H
