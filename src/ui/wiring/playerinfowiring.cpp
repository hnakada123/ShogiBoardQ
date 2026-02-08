/// @file playerinfowiring.cpp
/// @brief プレイヤー情報配線クラスの実装

#include "playerinfowiring.h"

#include <QDebug>
#include <QTabWidget>

#include "gameinfopanecontroller.h"
#include "playerinfocontroller.h"
#include "shogiview.h"
#include "settingsservice.h"
#include "engineanalysistab.h"
#include "engineinfowidget.h"

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

void PlayerInfoWiring::setAnalysisTab(EngineAnalysisTab* analysisTab)
{
    m_analysisTab = analysisTab;
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

    // デフォルトの対局情報を設定（対局開始時刻は現在時刻）
    const QDateTime now = QDateTime::currentDateTime();

    QList<KifGameInfoItem> defaultItems;
    defaultItems.append({tr("対局日"), now.toString(QStringLiteral("yyyy/MM/dd"))});
    defaultItems.append({tr("開始日時"), now.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});
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

    // 検討モード・詰み探索モードの場合、検討タブと思考タブにエンジン名を設定
    if (m_playMode &&
        (*m_playMode == PlayMode::ConsiderationMode || *m_playMode == PlayMode::TsumiSearchMode)) {
        if (m_analysisTab) {
            m_analysisTab->setConsiderationEngineName(e1);
            if (m_analysisTab->info1() && !e1.isEmpty()) {
                m_analysisTab->info1()->setDisplayNameFallback(e1);
            }
        }
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

void PlayerInfoWiring::resolveNamesAndSetupGameInfo(const QString& human1, const QString& human2,
                                                     const QString& engine1, const QString& engine2,
                                                     int playMode,
                                                     const QString& startSfen,
                                                     const TimeControlInfo& timeInfo)
{
    // まず対局者名の確定処理
    onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);

    // プレイモードに応じた先手・後手名を決定
    const PlayMode mode = static_cast<PlayMode>(playMode);
    QString blackName, whiteName;
    switch (mode) {
    case PlayMode::HumanVsHuman:
        blackName = human1.isEmpty() ? tr("先手") : human1;
        whiteName = human2.isEmpty() ? tr("後手") : human2;
        break;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        blackName = human1.isEmpty() ? tr("先手") : human1;
        whiteName = engine2.isEmpty() ? tr("Engine") : engine2;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        blackName = engine1.isEmpty() ? tr("Engine") : engine1;
        whiteName = human2.isEmpty() ? tr("後手") : human2;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        blackName = engine1.isEmpty() ? tr("Engine1") : engine1;
        whiteName = engine2.isEmpty() ? tr("Engine2") : engine2;
        break;
    default:
        blackName = tr("先手");
        whiteName = tr("後手");
        break;
    }

    // 手合割の判定
    const QString sfen = startSfen.trimmed();
    const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    QString handicap = tr("平手");
    if (!sfen.isEmpty()) {
        const QString pp = sfen.section(QLatin1Char(' '), 0, 0);
        if (!pp.isEmpty() && pp != initPP) {
            handicap = tr("その他");
        }
    }

    // 対局情報を設定
    setGameInfoForMatchStart(
        timeInfo.gameStartDateTime,
        blackName,
        whiteName,
        handicap,
        timeInfo.hasTimeControl,
        timeInfo.baseTimeMs,
        timeInfo.byoyomiMs,
        timeInfo.incrementMs
    );
}

void PlayerInfoWiring::onPlayerNamesResolvedWithTime(const QString& human1, const QString& human2,
                                                       const QString& engine1, const QString& engine2,
                                                       int playMode,
                                                       const QString& startSfen,
                                                       const TimeControlInfo& timeInfo)
{
    resolveNamesAndSetupGameInfo(human1, human2, engine1, engine2, playMode, startSfen, timeInfo);
}

void PlayerInfoWiring::onGameInfoUpdated(const QList<KifGameInfoItem>& items)
{
    qDebug().noquote() << "[PlayerInfoWiring] onGameInfoUpdated: items=" << items.size();
    Q_EMIT gameInfoUpdated(items);
}

void PlayerInfoWiring::setGameInfoForMatchStart(const QDateTime& startDateTime,
                                                const QString& blackName,
                                                const QString& whiteName,
                                                const QString& handicap,
                                                bool hasTimeControl,
                                                qint64 baseTimeMs,
                                                qint64 byoyomiMs,
                                                qint64 incrementMs)
{
    ensureGameInfoController();
    if (!m_gameInfoController) return;

    QList<KifGameInfoItem> items;

    // 対局日
    items.append({tr("対局日"), startDateTime.toString(QStringLiteral("yyyy/MM/dd"))});

    // 開始日時
    items.append({tr("開始日時"), startDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});

    // 先手
    items.append({tr("先手"), blackName.isEmpty() ? tr("先手") : blackName});

    // 後手
    items.append({tr("後手"), whiteName.isEmpty() ? tr("後手") : whiteName});

    // 手合割
    items.append({tr("手合割"), handicap.isEmpty() ? tr("平手") : handicap});

    // 持ち時間（時間制御が有効な場合のみ）
    if (hasTimeControl) {
        const int baseMin = static_cast<int>(baseTimeMs / 60000);
        const int baseSec = static_cast<int>((baseTimeMs % 60000) / 1000);
        const int byoyomiSec = static_cast<int>(byoyomiMs / 1000);
        const int incrementSec = static_cast<int>(incrementMs / 1000);

        QString timeStr;
        if (baseMin > 0 || baseSec > 0) {
            timeStr = QStringLiteral("%1:%2")
                .arg(baseMin, 2, 10, QLatin1Char('0'))
                .arg(baseSec, 2, 10, QLatin1Char('0'));
        } else {
            timeStr = QStringLiteral("00:00");
        }
        if (byoyomiSec > 0) {
            timeStr += QStringLiteral("+%1").arg(byoyomiSec);
        } else if (incrementSec > 0) {
            timeStr += QStringLiteral("+%1").arg(incrementSec);
        }
        items.append({tr("持ち時間"), timeStr});
    }

    m_gameInfoController->setGameInfo(items);

    qDebug().noquote() << "[PlayerInfoWiring] setGameInfoForMatchStart: items=" << items.size()
                       << " hasTimeControl=" << hasTimeControl;
}

void PlayerInfoWiring::updateGameInfoWithEndTime(const QDateTime& endDateTime)
{
    if (!m_gameInfoController) return;

    // 現在の対局情報を取得
    QList<KifGameInfoItem> items = m_gameInfoController->gameInfo();

    // 終了日時が既にあれば更新、なければ追加
    bool found = false;
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].key == tr("終了日時")) {
            items[i].value = endDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
            found = true;
            break;
        }
    }
    if (!found) {
        items.append({tr("終了日時"), endDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});
    }

    m_gameInfoController->setGameInfo(items);

    qDebug().noquote() << "[PlayerInfoWiring] updateGameInfoWithEndTime:"
                       << endDateTime.toString(Qt::ISODate);
}

void PlayerInfoWiring::updateGameInfoWithTimeControl(bool hasTimeControl,
                                                     qint64 baseTimeMs,
                                                     qint64 byoyomiMs,
                                                     qint64 incrementMs)
{
    if (!m_gameInfoController) return;
    if (!hasTimeControl) return;

    // 現在の対局情報を取得
    QList<KifGameInfoItem> items = m_gameInfoController->gameInfo();

    // 持ち時間文字列を生成
    const int baseMin = static_cast<int>(baseTimeMs / 60000);
    const int baseSec = static_cast<int>((baseTimeMs % 60000) / 1000);
    const int byoyomiSec = static_cast<int>(byoyomiMs / 1000);
    const int incrementSec = static_cast<int>(incrementMs / 1000);

    QString timeStr;
    if (baseMin > 0 || baseSec > 0) {
        timeStr = QStringLiteral("%1:%2")
            .arg(baseMin, 2, 10, QLatin1Char('0'))
            .arg(baseSec, 2, 10, QLatin1Char('0'));
    } else {
        timeStr = QStringLiteral("00:00");
    }
    if (byoyomiSec > 0) {
        timeStr += QStringLiteral("+%1").arg(byoyomiSec);
    } else if (incrementSec > 0) {
        timeStr += QStringLiteral("+%1").arg(incrementSec);
    }

    // 持ち時間が既にあれば更新、なければ追加
    bool found = false;
    for (int i = 0; i < items.size(); ++i) {
        if (items[i].key == tr("持ち時間")) {
            items[i].value = timeStr;
            found = true;
            break;
        }
    }
    if (!found) {
        items.append({tr("持ち時間"), timeStr});
    }

    m_gameInfoController->setGameInfo(items);

    qDebug().noquote() << "[PlayerInfoWiring] updateGameInfoWithTimeControl:"
                       << timeStr;
}
