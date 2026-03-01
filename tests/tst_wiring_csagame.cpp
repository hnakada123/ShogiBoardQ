/// @file tst_wiring_csagame.cpp
/// @brief CsaGameWiring の配線契約テスト（ソース解析型）
///
/// wire() / wireExternalSignals() / startCsaGame() が
/// 必要な connect() を漏れなく行っているかを検証する。

#include <QtTest>
#include <QFile>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestWiringCsaGame : public QObject
{
    Q_OBJECT

private:
    QString m_wiringSrc;
    QString m_wiringHeader;

    /// ソースファイルを読み込む
    static QString readSource(const QString& relativePath)
    {
        QFile f(QStringLiteral(SOURCE_DIR) + QStringLiteral("/") + relativePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(f.readAll());
    }

    /// 指定のシグナルパターンとスロットパターンが同一 connect() 呼び出し内に存在するか
    static bool hasConnection(const QString& src,
                              const QString& signalPattern,
                              const QString& slotPattern)
    {
        qsizetype pos = 0;
        while ((pos = src.indexOf(QStringLiteral("connect("), pos)) != -1) {
            qsizetype end = src.indexOf(QStringLiteral(");"), pos);
            if (end < 0) break;
            const QString block = src.mid(pos, end - pos + 2);
            if (block.contains(signalPattern) && block.contains(slotPattern))
                return true;
            pos = end + 1;
        }
        return false;
    }

    /// connect() 呼び出しの総数を数える
    static int countConnects(const QString& src)
    {
        int count = 0;
        qsizetype pos = 0;
        while ((pos = src.indexOf(QStringLiteral("connect("), pos)) != -1) {
            ++count;
            ++pos;
        }
        return count;
    }

private slots:
    void initTestCase()
    {
        m_wiringSrc = readSource(QStringLiteral("src/ui/wiring/csagamewiring.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/wiring/csagamewiring.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "csagamewiring.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "csagamewiring.h not found");
    }

    // ================================================================
    // wire() 契約テスト
    // ================================================================

    void wire_connectsAllCoordinatorSignals()
    {
        // CsaGameCoordinator の主要シグナルがすべて接続されていること
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::gameStarted"),
                               QStringLiteral("CsaGameWiring::onGameStarted")),
                 "gameStarted → onGameStarted connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::gameEnded"),
                               QStringLiteral("CsaGameWiring::onGameEnded")),
                 "gameEnded → onGameEnded connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::moveMade"),
                               QStringLiteral("CsaGameWiring::onMoveMade")),
                 "moveMade → onMoveMade connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::turnChanged"),
                               QStringLiteral("CsaGameWiring::onTurnChanged")),
                 "turnChanged → onTurnChanged connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::logMessage"),
                               QStringLiteral("CsaGameWiring::onLogMessage")),
                 "logMessage → onLogMessage connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::moveHighlightRequested"),
                               QStringLiteral("CsaGameWiring::onMoveHighlightRequested")),
                 "moveHighlightRequested → onMoveHighlightRequested connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::errorOccurred"),
                               QStringLiteral("CsaGameWiring::errorMessageRequested")),
                 "errorOccurred → errorMessageRequested connection missing");
    }

    void wire_connectsInternalSignals()
    {
        // 内部シグナル→内部スロットの接続
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::playModeChanged"),
                               QStringLiteral("CsaGameWiring::onPlayModeChangedInternal")),
                 "playModeChanged → onPlayModeChangedInternal connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::showGameEndDialogRequested"),
                               QStringLiteral("CsaGameWiring::showGameEndDialogInternal")),
                 "showGameEndDialogRequested → showGameEndDialogInternal connection missing");
    }

    // ================================================================
    // wireExternalSignals() 契約テスト
    // ================================================================

    void wireExternalSignals_connectsUiStatePolicyManager()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::disableNavigationRequested"),
                               QStringLiteral("UiStatePolicyManager::transitionToDuringCsaGame")),
                 "disableNavigationRequested → transitionToDuringCsaGame connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::enableNavigationRequested"),
                               QStringLiteral("UiStatePolicyManager::transitionToIdle")),
                 "enableNavigationRequested → transitionToIdle connection missing");
    }

    void wireExternalSignals_connectsRecordService()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::appendKifuLineRequested"),
                               QStringLiteral("GameRecordUpdateService::appendKifuLine")),
                 "appendKifuLineRequested → appendKifuLine connection missing");
    }

    void wireExternalSignals_connectsNotifService()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameWiring::errorMessageRequested"),
                               QStringLiteral("UiNotificationService::displayErrorMessage")),
                 "errorMessageRequested → displayErrorMessage connection missing");
    }

    // ================================================================
    // startCsaGame() 契約テスト
    // ================================================================

    void startCsaGame_connectsCsaLogAndCommands()
    {
        // CSA通信ログの転送
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("CsaGameCoordinator::csaCommLogAppended"),
                               QStringLiteral("EngineAnalysisTab::appendCsaLog")),
                 "csaCommLogAppended → appendCsaLog connection missing");

        // CSAコマンド送信
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::csaRawCommandRequested"),
                               QStringLiteral("CsaGameCoordinator::sendRawCommand")),
                 "csaRawCommandRequested → sendRawCommand connection missing");

        // 盤面操作からの指し手転送
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("BoardSetupController::csaMoveRequested"),
                               QStringLiteral("CsaGameCoordinator::onHumanMove")),
                 "csaMoveRequested → onHumanMove connection missing");
    }

    // ================================================================
    // 全体構造テスト
    // ================================================================

    void totalConnectCount_matchesExpected()
    {
        const int total = countConnects(m_wiringSrc);
        // wire():9 + startCsaGame():4 + wireExternalSignals():4 = 17以上
        QVERIFY2(total >= 17,
                 qPrintable(QStringLiteral("Expected >= 17 connect() calls, got %1").arg(total)));
    }
};

QTEST_MAIN(TestWiringCsaGame)

#include "tst_wiring_csagame.moc"
