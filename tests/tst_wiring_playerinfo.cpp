/// @file tst_wiring_playerinfo.cpp
/// @brief PlayerInfoWiring の配線契約テスト（軽量ソース検証）
///
/// 実行依存を増やさない範囲で、配線契約とキー利用契約を検証する。

#include <QtTest>
#include <QFile>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestWiringPlayerInfo : public QObject
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

private slots:
    void initTestCase()
    {
        m_wiringSrc = readSource(QStringLiteral("src/ui/wiring/playerinfowiring.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/wiring/playerinfowiring.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "playerinfowiring.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "playerinfowiring.h not found");
    }

    void addGameInfoTab_connectsTabCurrentChanged()
    {
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("QTabWidget::currentChanged"),
                               QStringLiteral("PlayerInfoWiring::tabCurrentChanged")),
                 "tabWidget currentChanged → tabCurrentChanged connection missing");
    }

    void depsFields_allReferencedInConstructor()
    {
        const QStringList fields = {
            QStringLiteral("parentWidget"),
            QStringLiteral("tabWidget"),
            QStringLiteral("shogiView"),
            QStringLiteral("playMode"),
            QStringLiteral("humanName1"),
            QStringLiteral("humanName2"),
            QStringLiteral("engineName1"),
            QStringLiteral("engineName2"),
            QStringLiteral("startSfenStr"),
            QStringLiteral("timeControllerRef"),
        };
        for (const auto& field : std::as_const(fields)) {
            QVERIFY2(m_wiringSrc.contains(QStringLiteral("deps.") + field),
                     qPrintable(QStringLiteral("Dependencies field '%1' not referenced").arg(field)));
        }
    }

    void ensureMethods_assignOwnedPointers()
    {
        QVERIFY2(m_wiringSrc.contains(QStringLiteral("void PlayerInfoWiring::ensureGameInfoController()")),
                 "ensureGameInfoController definition missing");
        QVERIFY2(m_wiringSrc.contains(QStringLiteral("m_gameInfoController =")),
                 "ensureGameInfoController should assign m_gameInfoController");

        QVERIFY2(m_wiringSrc.contains(QStringLiteral("void PlayerInfoWiring::ensurePlayerInfoController()")),
                 "ensurePlayerInfoController definition missing");
        QVERIFY2(m_wiringSrc.contains(QStringLiteral("m_playerInfoController =")),
                 "ensurePlayerInfoController should assign m_playerInfoController");
    }

    void canonicalGameInfoKeys_areUsed()
    {
        const QStringList keys = {
            QStringLiteral("GameInfoKeys::kGameDate"),
            QStringLiteral("GameInfoKeys::kStartDateTime"),
            QStringLiteral("GameInfoKeys::kEndDateTime"),
            QStringLiteral("GameInfoKeys::kBlackPlayer"),
            QStringLiteral("GameInfoKeys::kWhitePlayer"),
            QStringLiteral("GameInfoKeys::kHandicap"),
            QStringLiteral("GameInfoKeys::kTimeControl"),
        };
        for (const auto& key : std::as_const(keys)) {
            QVERIFY2(m_wiringSrc.contains(key),
                     qPrintable(QStringLiteral("Canonical key '%1' not used").arg(key)));
        }
    }

    void publicSlots_allDeclaredInHeader()
    {
        const QStringList slotNames = {
            QStringLiteral("PlayerInfoWiring::onSetPlayersNames"),
            QStringLiteral("PlayerInfoWiring::onSetEngineNames"),
            QStringLiteral("PlayerInfoWiring::onPlayerNamesResolved"),
            QStringLiteral("PlayerInfoWiring::onMenuPlayerNamesResolved"),
        };
        for (const auto& name : std::as_const(slotNames)) {
            QVERIFY2(m_wiringSrc.contains(name),
                     qPrintable(QStringLiteral("Slot implementation '%1' not found").arg(name)));
        }
    }
};

QTEST_MAIN(TestWiringPlayerInfo)

#include "tst_wiring_playerinfo.moc"
