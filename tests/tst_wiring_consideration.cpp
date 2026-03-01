/// @file tst_wiring_consideration.cpp
/// @brief ConsiderationWiring の配線契約テスト（ソース解析型）
///
/// ensureUIController() の内部 connect() と
/// Deps 構造体の全フィールド参照を検証する。

#include <QtTest>
#include <QFile>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestWiringConsideration : public QObject
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
        m_wiringSrc = readSource(QStringLiteral("src/ui/wiring/considerationwiring.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/wiring/considerationwiring.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "considerationwiring.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "considerationwiring.h not found");
    }

    // ================================================================
    // ensureUIController() 契約テスト
    // ================================================================

    void ensureUIController_connectsStopRequested()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("ConsiderationModeUIController::stopRequested"),
                               QStringLiteral("ConsiderationWiring::handleStopRequest")),
                 "stopRequested → handleStopRequest connection missing");
    }

    void ensureUIController_connectsStartRequested()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("ConsiderationModeUIController::startRequested"),
                               QStringLiteral("ConsiderationWiring::displayConsiderationDialog")),
                 "startRequested → displayConsiderationDialog connection missing");
    }

    void ensureUIController_connectsMultiPVChangeRequested()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("ConsiderationModeUIController::multiPVChangeRequested"),
                               QStringLiteral("ConsiderationWiring::onMultiPVChangeRequested")),
                 "multiPVChangeRequested → onMultiPVChangeRequested connection missing");
    }

    // ================================================================
    // Deps 構造体テスト
    // ================================================================

    void depsFields_allReferencedInConstructor()
    {
        // Deps の全フィールドがコンストラクタまたは updateDeps で参照されていること
        const QStringList fields = {
            QStringLiteral("parentWidget"),
            QStringLiteral("considerationTabManager"),
            QStringLiteral("thinkingInfo1"),
            QStringLiteral("shogiView"),
            QStringLiteral("match"),
            QStringLiteral("dialogCoordinator"),
            QStringLiteral("considerationModel"),
            QStringLiteral("commLogModel"),
            QStringLiteral("playMode"),
            QStringLiteral("currentSfenStr"),
            QStringLiteral("ensureDialogCoordinator"),
        };
        for (const auto& field : std::as_const(fields)) {
            QVERIFY2(m_wiringSrc.contains(QStringLiteral("deps.") + field),
                     qPrintable(QStringLiteral("Deps field '%1' not referenced").arg(field)));
        }
    }

    // ================================================================
    // 全体構造テスト
    // ================================================================

    void totalConnectCount_matchesExpected()
    {
        const int total = countConnects(m_wiringSrc);
        // ensureUIController():3
        QVERIFY2(total >= 3,
                 qPrintable(QStringLiteral("Expected >= 3 connect() calls, got %1").arg(total)));
    }
};

QTEST_MAIN(TestWiringConsideration)

#include "tst_wiring_consideration.moc"
