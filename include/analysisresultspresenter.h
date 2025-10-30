#ifndef ANALYSISRESULTSPRESENTER_H
#define ANALYSISRESULTSPRESENTER_H

#include <QObject>
#include <QPointer>

class QDialog;
class QTableView;
class QHeaderView;
class QAbstractItemModel;
class QTimer;
class KifuAnalysisListModel;

class AnalysisResultsPresenter : public QObject
{
    Q_OBJECT
public:
    explicit AnalysisResultsPresenter(QObject* parent=nullptr);

    // モデルを受け取り、非モーダルで表示（UIは都度作り直し）
    void showWithModel(KifuAnalysisListModel* model);

    QTableView* view() const { return m_view; }
    QDialog* dialog() const { return m_dlg; }

private slots:
    void reflowNow();
    void onModelReset();
    void onRowsInserted(const QModelIndex&, int, int);
    void onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&);
    void onLayoutChanged();
    void onScrollRangeChanged(int, int);

private:
    void buildUi(KifuAnalysisListModel* model);
    void connectModelSignals(KifuAnalysisListModel* model);

    QPointer<QDialog> m_dlg;
    QPointer<QTableView> m_view;
    QPointer<QHeaderView> m_header;
    QTimer* m_reflowTimer;
};

#endif // ANALYSISRESULTSPRESENTER_H
