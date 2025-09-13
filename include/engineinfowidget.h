#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

#include <QWidget>
class QLineEdit; class QLabel;
class UsiCommLogModel;

class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(QWidget* parent=nullptr);
    void setModel(UsiCommLogModel* model);
    void setDisplayNameFallback(const QString& name);

private slots:
    void onNameChanged();
    void onPredChanged();
    void onSearchedChanged();
    void onDepthChanged();
    void onNodesChanged();
    void onNpsChanged();
    void onHashChanged();

private:
    UsiCommLogModel* m_model=nullptr;
    QLineEdit *m_engineName=nullptr, *m_pred=nullptr, *m_searched=nullptr;
    QLineEdit *m_depth=nullptr, *m_nodes=nullptr, *m_nps=nullptr, *m_hash=nullptr;
    QString m_fallbackName;
};

#endif // ENGINEINFOWIDGET_H
