#include <QtTest>

#include "prestartcleanuphandler.h"

class TestPreStartCleanupHandler : public QObject
{
    Q_OBJECT

private slots:
    void testStartFromCurrentPositionWhenStartSfenSet();
    void testStartFromCurrentPositionWhenStartSfenEmpty();
    void testStartFromCurrentPositionStartpos();
};

void TestPreStartCleanupHandler::testStartFromCurrentPositionWhenStartSfenSet()
{
    const QString startSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString currentSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/p1ppppppp/1p7/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 5");

    QVERIFY(PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 4));
}

void TestPreStartCleanupHandler::testStartFromCurrentPositionWhenStartSfenEmpty()
{
    const QString startSfen;
    const QString currentSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/p1ppppppp/1p7/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 5");

    QVERIFY(PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 0));
}

void TestPreStartCleanupHandler::testStartFromCurrentPositionStartpos()
{
    const QString startSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString currentSfen = QStringLiteral("startpos");

    QVERIFY(!PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 0));
}

QTEST_GUILESS_MAIN(TestPreStartCleanupHandler)
#include "tst_prestartcleanuphandler.moc"
