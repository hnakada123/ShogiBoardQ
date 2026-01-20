#ifndef UIACTIONSWIRING_H
#define UIACTIONSWIRING_H

#include <QObject>

class ShogiView;
namespace Ui { class MainWindow; }

class UiActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr; // ui_*
        ShogiView*      shogiView = nullptr; // 盤の拡大/縮小を直結
        QObject*        ctx = nullptr; // 受け側(MainWindow*)。そのスロットへ接続する
    };

    explicit UiActionsWiring(const Deps& d, QObject* parent=nullptr)
        : QObject(parent), m_d(d) {}

    void wire(); // 多重接続を避けたい場合は呼び出し側で1回だけ呼べばOK

private:
    Deps m_d;
};

#endif // UIACTIONSWIRING_H
