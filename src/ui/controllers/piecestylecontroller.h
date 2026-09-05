#ifndef PIECESTYLECONTROLLER_H
#define PIECESTYLECONTROLLER_H

#include <QObject>

class QAction;
class QActionGroup;

/// 駒種類メニューの排他選択と設定の同期。
class PieceStyleController : public QObject
{
    Q_OBJECT
public:
    PieceStyleController(QAction* standard, QAction* clear, QObject* parent = nullptr);

private slots:
    void selectStyle(QAction* action);
    void updateCheckedAction();

private:
    QActionGroup* m_group;
};

#endif // PIECESTYLECONTROLLER_H
