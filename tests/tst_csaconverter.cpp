#include <QtTest>
#include <QCoreApplication>

#include "kifdisplayitem.h"
#include "csatosfenconverter.h"
#include "kifparsetypes.h"

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestCsaConverter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    void parse_basic()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_basic.csa")), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 7);
        QCOMPARE(result.mainline.usiMoves, kExpectedUsiMoves);
    }

    void extractGameInfo()
    {
        auto info = CsaToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("test_basic.csa")));

        bool foundSente = false;
        bool foundGote = false;
        for (const auto& item : std::as_const(info)) {
            if (item.value.contains(QStringLiteral("テスト先手")))
                foundSente = true;
            if (item.value.contains(QStringLiteral("テスト後手")))
                foundGote = true;
        }
        QVERIFY(foundSente);
        QVERIFY(foundGote);
    }
};

QTEST_MAIN(TestCsaConverter)
#include "tst_csaconverter.moc"
