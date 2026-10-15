/// @file tst_wiring_consideration.cpp
/// @brief ConsiderationUiController の配線契約テスト（ソース解析型）
///
/// コンストラクタの connect() と Deps 構造体の全フィールド参照を検証する。

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
        m_wiringSrc = readSource(QStringLiteral("src/ui/controllers/considerationuicontroller.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/controllers/considerationuicontroller.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "considerationuicontroller.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "considerationuicontroller.h not found");
    }

    // ================================================================
    // コンストラクタの接続
    // ================================================================

    void constructor_connectsStopRequested()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("ConsiderationTabManager::stopConsiderationRequested"),
                               QStringLiteral("ConsiderationUiController::stopConsideration")),
                 "stopConsiderationRequested → stopConsideration connection missing");
    }

    void startRequested_usesAnalysisTabWiring()
    {
        const QString tabWiring = readSource(QStringLiteral("src/ui/wiring/analysistabwiring.cpp"));
        QVERIFY2(hasConnection(tabWiring,
                               QStringLiteral("EngineAnalysisTab::startConsiderationRequested"),
                               QStringLiteral("ConsiderationUiController::startConsideration")),
                 "startConsiderationRequested → startConsideration connection missing");
    }

    void constructor_connectsMultiPVChanged()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("ConsiderationTabManager::considerationMultiPVChanged"),
                               QStringLiteral("ConsiderationUiController::onMultiPVChanged")),
                 "considerationMultiPVChanged → onMultiPVChanged connection missing");
    }

    void constructor_connectsSessionEnd()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("AnalysisSession::considerationModeEnded"),
                               QStringLiteral("ConsiderationUiController::onModeEnded")),
                 "considerationModeEnded → onModeEnded connection missing");
    }

    // ================================================================
    // Deps 構造体テスト
    // ================================================================

    void depsFields_allReferencedInConstructor()
    {
        // Deps の全フィールドがコンストラクタで参照されていること
        const QStringList fields = {
            QStringLiteral("parentWidget"),
            QStringLiteral("considerationTabManager"),
            QStringLiteral("thinkingInfo1"),
            QStringLiteral("shogiView"),
            QStringLiteral("session"),
            QStringLiteral("considerationModel"),
            QStringLiteral("commLogModel"),
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
        // 矢印・候補手の数・中止・検討の終了・待機の5経路。開始は AnalysisTabWiring が担当。
        QVERIFY2(total >= 5,
                 qPrintable(QStringLiteral("Expected >= 5 connect() calls, got %1").arg(total)));
    }
};

QTEST_MAIN(TestWiringConsideration)

#include "tst_wiring_consideration.moc"
