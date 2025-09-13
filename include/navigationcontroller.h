#ifndef NAVIGATIONCONTROLLER_H
#define NAVIGATIONCONTROLLER_H

#include <QObject>
#include <QPointer>
class QPushButton;
class INavigationContext;

class NavigationController : public QObject {
    Q_OBJECT
public:
    struct Buttons {
        QPushButton* first{};
        QPushButton* back10{};
        QPushButton* prev{};
        QPushButton* next{};
        QPushButton* fwd10{};
        QPushButton* last{};
    };

    NavigationController(const Buttons& btns,
                         INavigationContext* ctx,
                         QObject* parent = nullptr);

public slots:
    void toFirst();
    void back10();
    void prev();
    void next();
    void fwd10();
    void toLast();

private:
    INavigationContext* m_ctx; // MainWindow の寿命 > Controller を前提（親子関係で担保）
    int clampRow(int row) const;
};

#endif // NAVIGATIONCONTROLLER_H
