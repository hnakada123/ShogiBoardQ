/// @file tst_wiring_analysistab.cpp
/// @brief AnalysisTabWiring の配線契約テスト（ソース解析型）
///
/// buildUiAndWire() / wireExternalSignals() が
/// 必要な connect() を漏れなく行っているかを検証する。

#include <QtTest>
#include <QFile>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestWiringAnalysisTab : public QObject
{
    Q_OBJECT

private:
    QString m_wiringSrc;
    QString m_wiringHeader;

    static QString readSource(const QString& relativePath)
    {
        QFile f(QStringLiteral(SOURCE_DIR) + QStringLiteral("/") + relativePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(f.readAll());
    }

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
        m_wiringSrc = readSource(QStringLiteral("src/ui/wiring/analysistabwiring.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/wiring/analysistabwiring.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "analysistabwiring.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "analysistabwiring.h not found");
    }

    // ================================================================
    // buildUiAndWire() 契約テスト
    // ================================================================

    void buildUiAndWire_connectsBranchNodeSignalForwarding()
    {
        // signal-to-signal 転送: EngineAnalysisTab → AnalysisTabWiring
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::branchNodeActivated"),
                               QStringLiteral("AnalysisTabWiring::branchNodeActivated")),
                 "branchNodeActivated signal-to-signal forwarding missing");
    }

    // ================================================================
    // wireExternalSignals() 契約テスト
    // ================================================================

    void wireExternalSignals_connectsBranchNavigation()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::branchNodeActivated"),
                               QStringLiteral("BranchNavigationWiring::onBranchNodeActivated")),
                 "branchNodeActivated → onBranchNodeActivated connection missing");
    }

    void wireExternalSignals_connectsCommentCoordinator()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::commentUpdated"),
                               QStringLiteral("CommentCoordinator::onCommentUpdated")),
                 "commentUpdated → onCommentUpdated connection missing");
    }

    void wireExternalSignals_connectsPvClickController()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::pvRowClicked"),
                               QStringLiteral("PvClickController::onPvRowClicked")),
                 "pvRowClicked → onPvRowClicked connection missing");
    }

    void wireExternalSignals_connectsUsiCommandController()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::usiCommandRequested"),
                               QStringLiteral("UsiCommandController::sendCommand")),
                 "usiCommandRequested → sendCommand connection missing");
    }

    void wireExternalSignals_connectsConsiderationWiring()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::startConsiderationRequested"),
                               QStringLiteral("ConsiderationWiring::displayConsiderationDialog")),
                 "startConsiderationRequested → displayConsiderationDialog connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::engineSettingsRequested"),
                               QStringLiteral("ConsiderationWiring::onEngineSettingsRequested")),
                 "engineSettingsRequested → onEngineSettingsRequested connection missing");

        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("EngineAnalysisTab::considerationEngineChanged"),
                               QStringLiteral("ConsiderationWiring::onEngineChanged")),
                 "considerationEngineChanged → onEngineChanged connection missing");
    }

    // ================================================================
    // 全体構造テスト
    // ================================================================

    void totalConnectCount_matchesExpected()
    {
        const int total = countConnects(m_wiringSrc);
        // buildUiAndWire():1 + wireExternalSignals():7 = 8
        QVERIFY2(total >= 8,
                 qPrintable(QStringLiteral("Expected >= 8 connect() calls, got %1").arg(total)));
    }

    void externalSignalDeps_allFieldsUsed()
    {
        // ExternalSignalDeps の全フィールドが wireExternalSignals() で参照されていること
        const QStringList fields = {
            QStringLiteral("branchNavigationWiring"),
            QStringLiteral("pvClickController"),
            QStringLiteral("commentCoordinator"),
            QStringLiteral("usiCommandController"),
            QStringLiteral("considerationWiring"),
        };
        for (const auto& field : std::as_const(fields)) {
            QVERIFY2(m_wiringSrc.contains(QStringLiteral("deps.") + field),
                     qPrintable(QStringLiteral("ExternalSignalDeps field '%1' not used").arg(field)));
        }
    }
};

QTEST_MAIN(TestWiringAnalysisTab)

#include "tst_wiring_analysistab.moc"
