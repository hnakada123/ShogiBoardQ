#ifndef PIECESTYLECONTROLLER_H
#define PIECESTYLECONTROLLER_H

#include <QObject>
#include <QList>

class QAction;
class QActionGroup;

/// 駒種類メニューの排他選択と設定の同期。
class PieceStyleController : public QObject
{
    Q_OBJECT
public:
    struct StyleAction {
        QAction* action;
        QString id;
    };
    PieceStyleController(const QList<StyleAction>& actions, QObject* parent = nullptr);

private slots:
    void selectStyle(QAction* action);
    void updateCheckedAction();

private:
    QActionGroup* m_group;
};

#endif // PIECESTYLECONTROLLER_H
