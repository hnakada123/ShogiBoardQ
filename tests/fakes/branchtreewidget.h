#ifndef BRANCHTREEWIDGET_H
#define BRANCHTREEWIDGET_H

class KifuBranchTree;

#include <QObject>

class BranchTreeWidget : public QObject
{
    Q_OBJECT

public:
    explicit BranchTreeWidget(QObject* parent = nullptr) : QObject(parent) {}
    void setTree(KifuBranchTree*) {}
    void highlightNode(int, int) {}

signals:
    void nodeClicked(int, int);
};

#endif // BRANCHTREEWIDGET_H
