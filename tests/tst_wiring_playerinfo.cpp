/// @file tst_wiring_playerinfo.cpp
/// @brief PlayerInfoWiring の配線契約テスト（ソース解析型）
///
/// addGameInfoTabAtStartup() の connect() と
/// Dependencies 構造体の全フィールド参照を検証する。

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
        m_wiringSrc = readSource(QStringLiteral("src/ui/wiring/playerinfowiring.cpp"));
        m_wiringHeader = readSource(QStringLiteral("src/ui/wiring/playerinfowiring.h"));
        QVERIFY2(!m_wiringSrc.isEmpty(), "playerinfowiring.cpp not found");
        QVERIFY2(!m_wiringHeader.isEmpty(), "playerinfowiring.h not found");
    }

    // ================================================================
    // addGameInfoTabAtStartup() 契約テスト
    // ================================================================

    void addGameInfoTab_connectsTabCurrentChanged()
    {
        // QTabWidget::currentChanged → PlayerInfoWiring::tabCurrentChanged
        QVERIFY2(hasConnection(m_wiringSrc,
                               QStringLiteral("QTabWidget::currentChanged"),
                               QStringLiteral("PlayerInfoWiring::tabCurrentChanged")),
                 "tabWidget currentChanged → tabCurrentChanged connection missing");
    }

    // ================================================================
    // Dependencies 構造体テスト
    // ================================================================

    void depsFields_allReferencedInConstructor()
    {
        // Dependencies の全フィールドがコンストラクタで参照されていること
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

    // ================================================================
    // 内部コントローラ生成テスト
    // ================================================================

    void ensureGameInfoController_createsController()
    {
        // ensureGameInfoController() が GameInfoPaneController を new すること
        QVERIFY2(m_wiringSrc.contains(QStringLiteral("new GameInfoPaneController")),
                 "ensureGameInfoController should create GameInfoPaneController");
    }

    void ensurePlayerInfoController_createsController()
    {
        // ensurePlayerInfoController() が PlayerInfoController を new すること
        QVERIFY2(m_wiringSrc.contains(QStringLiteral("new PlayerInfoController")),
                 "ensurePlayerInfoController should create PlayerInfoController");
    }

    // ================================================================
    // メソッド存在テスト
    // ================================================================

    void publicSlots_allDeclaredInHeader()
    {
        // ヘッダーに宣言された public slots がすべて .cpp に実装されていること
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
