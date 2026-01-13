#include "playerinfowiring.h"

#include <QDebug>
#include <QTabWidget>

#include "gameinfopanecontroller.h"
#include "playerinfocontroller.h"
#include "shogiview.h"
#include "settingsservice.h"

PlayerInfoWiring::PlayerInfoWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_tabWidget(deps.tabWidget)
    , m_shogiView(deps.shogiView)
    , m_playMode(deps.playMode)
    , m_humanName1(deps.humanName1)
    , m_humanName2(deps.humanName2)
    , m_engineName1(deps.engineName1)
    , m_engineName2(deps.engineName2)
{
}

void PlayerInfoWiring::ensureGameInfoController()
{
    if (m_gameInfoController) return;

    m_gameInfoController = new GameInfoPaneController(m_parentWidget);

    // 対局情報更新時のシグナル接続
    connect(m_gameInfoController, &GameInfoPaneController::gameInfoUpdated,
            this, &PlayerInfoWiring::onGameInfoUpdated);

    qDebug().noquote() << "[PlayerInfoWiring] GameInfoPaneController created";
}

void PlayerInfoWiring::setTabWidget(QTabWidget* tabWidget)
{
    m_tabWidget = tabWidget;
}

void PlayerInfoWiring::ensurePlayerInfoController()
{
    if (m_playerInfoController) return;

    m_playerInfoController = new PlayerInfoController(m_parentWidget);

    // 依存オブジェクトの設定
    m_playerInfoController->setShogiView(m_shogiView);
    m_playerInfoController->setGameInfoController(m_gameInfoController);

    qDebug().noquote() << "[PlayerInfoWiring] PlayerInfoController created";
}

void PlayerInfoWiring::addGameInfoTabAtStartup()
{
    if (!m_tabWidget) return;

    // GameInfoPaneControllerを確保
    ensureGameInfoController();

    if (!m_gameInfoController) return;

    // 既に「対局情報」タブがあるか確認
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabText(i) == tr("対局情報")) {
            m_tabWidget->removeTab(i);
            break;
        }
    }

    // 初期データを設定
    populateDefaultGameInfo();

    // タブを最初（インデックス0）に挿入
    m_tabWidget->insertTab(0, m_gameInfoController->containerWidget(), tr("対局情報"));

    // 保存されたタブインデックスを復元
    int savedIndex = SettingsService::lastSelectedTabIndex();
    if (savedIndex >= 0 && savedIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(savedIndex);
    } else {
        m_tabWidget->setCurrentIndex(0);
    }

    // タブ変更時にシグナルを発行
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &PlayerInfoWiring::tabCurrentChanged,
            Qt::UniqueConnection);

    qDebug().noquote() << "[PlayerInfoWiring] addGameInfoTabAtStartup completed";
}

void PlayerInfoWiring::populateDefaultGameInfo()
{
    if (!m_gameInfoController) return;

    // デフォルトの対局情報を設定
    QList<KifGameInfoItem> defaultItems;
    defaultItems.append({tr("先手"), tr("先手")});
    defaultItems.append({tr("後手"), tr("後手")});
    defaultItems.append({tr("手合割"), tr("平手")});

    m_gameInfoController->setGameInfo(defaultItems);
}

void PlayerInfoWiring::onSetPlayersNames(const QString& p1, const QString& p2)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->onSetPlayersNames(p1, p2);
    }
}

void PlayerInfoWiring::onSetEngineNames(const QString& e1, const QString& e2)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        if (m_playMode) {
            m_playerInfoController->setPlayMode(*m_playMode);
        }
        m_playerInfoController->onSetEngineNames(e1, e2);

        // MainWindow側のメンバ変数を同期（シグナルで通知）
        const QString newEngine1 = m_playerInfoController->engineName1();
        const QString newEngine2 = m_playerInfoController->engineName2();

        // 外部参照も更新
        if (m_engineName1) *m_engineName1 = newEngine1;
        if (m_engineName2) *m_engineName2 = newEngine2;

        Q_EMIT engineNamesUpdated(newEngine1, newEngine2);
    }
}

void PlayerInfoWiring::updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->updateGameInfoPlayerNames(blackName, whiteName);
    }
}

void PlayerInfoWiring::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setOriginalGameInfo(items);
    }
}

void PlayerInfoWiring::updateGameInfoForCurrentMatch()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        if (m_playMode) {
            m_playerInfoController->setPlayMode(*m_playMode);
        }
        if (m_humanName1 && m_humanName2) {
            m_playerInfoController->setHumanNames(*m_humanName1, *m_humanName2);
        }
        if (m_engineName1 && m_engineName2) {
            m_playerInfoController->setEngineNames(*m_engineName1, *m_engineName2);
        }
        m_playerInfoController->updateGameInfoForCurrentMatch();
    }
}

void PlayerInfoWiring::onPlayerNamesResolved(const QString& human1, const QString& human2,
                                              const QString& engine1, const QString& engine2,
                                              int playMode)
{
    qDebug().noquote() << "[PlayerInfoWiring] onPlayerNamesResolved: playMode=" << playMode;

    // 外部参照を更新
    if (m_humanName1) *m_humanName1 = human1;
    if (m_humanName2) *m_humanName2 = human2;
    if (m_engineName1) *m_engineName1 = engine1;
    if (m_engineName2) *m_engineName2 = engine2;
    if (m_playMode) *m_playMode = static_cast<PlayMode>(playMode);

    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);
    }

    // シグナルで通知
    Q_EMIT playerNamesResolved(human1, human2, engine1, engine2, playMode);
}

void PlayerInfoWiring::onGameInfoUpdated(const QList<KifGameInfoItem>& items)
{
    qDebug().noquote() << "[PlayerInfoWiring] onGameInfoUpdated: items=" << items.size();
    Q_EMIT gameInfoUpdated(items);
}
