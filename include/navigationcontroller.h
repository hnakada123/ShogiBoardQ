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
    void toFirst(bool checked = false);
    void back10(bool checked = false);
    void prev(bool checked = false);
    void next(bool checked = false);
    void fwd10(bool checked = false);
    void toLast(bool checked = false);

private:
    INavigationContext* m_ctx; // MainWindow の寿命 > Controller を前提（親子関係で担保）
    int clampRow(int row) const;
};

#endif // NAVIGATIONCONTROLLER_H
