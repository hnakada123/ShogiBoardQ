/// @file tst_kifusavecoordinator.cpp
/// @brief KifuSaveCoordinator の保存形式判定と上書き保存のテスト

#include <QtTest>
#include <QFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTemporaryDir>

#include "kifusavecoordinator.h"

using KifuSaveCoordinator::SaveFormat;

Q_DECLARE_METATYPE(KifuSaveCoordinator::SaveFormat)

class TestKifuSaveCoordinator : public QObject
{
    Q_OBJECT

private:
    static QByteArray readAllBytes(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        return f.readAll();
    }

private slots:
    // === 拡張子 → 保存形式 ===

    void saveFormatForPath_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<SaveFormat>("expected");

        QTest::newRow("kif")         << QStringLiteral("/tmp/a.kif")  << SaveFormat::Kif;
        QTest::newRow("kifu")        << QStringLiteral("/tmp/a.kifu") << SaveFormat::Kif;
        QTest::newRow("ki2")         << QStringLiteral("/tmp/a.ki2")  << SaveFormat::Ki2;
        QTest::newRow("ki2u")        << QStringLiteral("/tmp/a.ki2u") << SaveFormat::Ki2;
        QTest::newRow("csa")         << QStringLiteral("/tmp/a.csa")  << SaveFormat::Csa;
        QTest::newRow("jkf")         << QStringLiteral("/tmp/a.jkf")  << SaveFormat::Jkf;
        QTest::newRow("usen")        << QStringLiteral("/tmp/a.usen") << SaveFormat::Usen;
        QTest::newRow("usi")         << QStringLiteral("/tmp/a.usi")  << SaveFormat::Usi;
        QTest::newRow("upper-case")  << QStringLiteral("/tmp/A.CSA")  << SaveFormat::Csa;
        QTest::newRow("unknown-ext") << QStringLiteral("/tmp/a.txt")  << SaveFormat::Kif;
        QTest::newRow("no-ext")      << QStringLiteral("/tmp/a")      << SaveFormat::Kif;
    }

    void saveFormatForPath()
    {
        QFETCH(QString, path);
        QFETCH(SaveFormat, expected);
        QCOMPARE(KifuSaveCoordinator::saveFormatForPath(path), expected);
    }

    void usesShiftJisForPath()
    {
        QVERIFY(KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.kif")));
        QVERIFY(KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.KI2")));
        QVERIFY(!KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.kifu")));
        QVERIFY(!KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.ki2u")));
        QVERIFY(!KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.csa")));
        QVERIFY(!KifuSaveCoordinator::usesShiftJisForPath(QStringLiteral("/tmp/a.usi")));
    }

    // === 上書き保存 ===

    /// 渡した行がそのまま UTF-8 で書かれる（.csa は encoding 宣言も無変更）
    void overwriteExisting_writesGivenLinesAsUtf8()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.csa"));

        const QStringList lines = {
            QStringLiteral("'CSA encoding=UTF-8"),
            QStringLiteral("V2.2"),
            QStringLiteral("N+鈴木"),
            QStringLiteral("+7776FU"),
        };
        QString err;
        QVERIFY2(KifuSaveCoordinator::overwriteExisting(path, lines, &err), qPrintable(err));

        const QString text = QString::fromUtf8(readAllBytes(path));
        QCOMPARE(text, lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    }

    /// 既存の内容は完全に置き換えられる
    void overwriteExisting_replacesPreviousContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.usi"));

        QString err;
        QVERIFY(KifuSaveCoordinator::overwriteExisting(
            path, {QStringLiteral("position startpos moves 7g7f 3c3d 2g2f")}, &err));
        QVERIFY(KifuSaveCoordinator::overwriteExisting(
            path, {QStringLiteral("position startpos moves 7g7f")}, &err));

        QCOMPARE(QString::fromUtf8(readAllBytes(path)),
                 QStringLiteral("position startpos moves 7g7f\n"));
    }

    /// .kifu は UTF-8 のまま、encoding 宣言も無変更
    void overwriteExisting_kifuKeepsUtf8Header()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kifu"));

        const QStringList lines = {
            QStringLiteral("#KIF version=2.0 encoding=UTF-8"),
            QStringLiteral("先手：鈴木"),
            QStringLiteral("   1 ７六歩(77)"),
        };
        QString err;
        QVERIFY2(KifuSaveCoordinator::overwriteExisting(path, lines, &err), qPrintable(err));

        const QString text = QString::fromUtf8(readAllBytes(path));
        QCOMPARE(text, lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    }

    /// .kif は Shift_JIS で書かれ、encoding 宣言が差し替わる
    void overwriteExisting_kifUsesShiftJis()
    {
        QStringEncoder probe("Shift-JIS");
        if (!probe.isValid()) {
            QSKIP("Shift_JIS codec is not available in this Qt build");
        }

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kif"));

        const QStringList lines = {
            QStringLiteral("#KIF version=2.0 encoding=UTF-8"),
            QStringLiteral("先手：鈴木"),
            QStringLiteral("   1 ７六歩(77)"),
        };
        QString err;
        QVERIFY2(KifuSaveCoordinator::overwriteExisting(path, lines, &err), qPrintable(err));

        const QByteArray bytes = readAllBytes(path);
        QStringDecoder decoder("Shift-JIS");
        const QString text = decoder(bytes);
        QVERIFY(text.startsWith(QStringLiteral("#KIF version=2.0 encoding=Shift_JIS\n")));
        QVERIFY(text.contains(QStringLiteral("先手：鈴木")));
        QVERIFY(text.contains(QStringLiteral("７六歩(77)")));

        // UTF-8 のバイト列ではない（= 実際に Shift_JIS で書かれている）
        QVERIFY(!QString::fromUtf8(bytes).contains(QStringLiteral("先手：鈴木")));
    }
};

QTEST_GUILESS_MAIN(TestKifuSaveCoordinator)
#include "tst_kifusavecoordinator.moc"
