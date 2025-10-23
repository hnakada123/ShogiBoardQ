#ifndef KIFULOADCOORDINATOR_H
#define KIFULOADCOORDINATOR_H

#include <QObject>
#include <QTableWidget>
#include <QDockWidget>

#include "kiftosfenconverter.h"
#include "engineanalysistab.h"
#include "shogiview.h"

class KifuLoadCoordinator : public QObject
{
    Q_OBJECT
public:
    // コンストラクタ
    explicit KifuLoadCoordinator(QObject *parent = nullptr);

    void loadKifuFromFile(const QString& filePath);

    void setGameInfoTable(QTableWidget *newGameInfoTable);

    void setTab(QTabWidget *newTab);

    void setShogiView(ShogiView *newShogiView);

signals:
    void errorOccurred(const QString& errorMessage);
    void setReplayMode(bool on);

private:
    bool m_loadingKifu = false;
    QTableWidget* m_gameInfoTable = nullptr;
    QDockWidget*  m_gameInfoDock  = nullptr;
    EngineAnalysisTab*    m_analysisTab = nullptr;
    QTabWidget* m_tab = nullptr;
    ShogiView* m_shogiView = nullptr;
    QStringList m_usiMoves;

    QString prepareInitialSfen(const QString& filePath, QString& teaiLabel) const;
    void populateGameInfo(const QList<KifGameInfoItem>& items);
    void addGameInfoTabIfMissing();
    void applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items);
    QString findGameInfoValue(const QList<KifGameInfoItem>& items, const QStringList& keys) const;
};

#endif // KIFULOADCOORDINATOR_H
