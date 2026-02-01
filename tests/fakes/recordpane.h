#ifndef RECORDPANE_H
#define RECORDPANE_H

#include <QObject>
#include <QString>
#include <QModelIndex>

class QTableView;

class RecordPane : public QObject
{
    Q_OBJECT

public:
    explicit RecordPane(QObject* parent = nullptr) : QObject(parent) {}

    QTableView* kifuView() const { return nullptr; }
    QTableView* branchView() const { return nullptr; }
    void setBranchCommentText(const QString&) {}

signals:
    void branchActivated(const QModelIndex&);
};

#endif // RECORDPANE_H
