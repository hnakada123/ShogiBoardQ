/// @file tst_kifusavecoordinator.cpp
/// @brief KifuSaveCoordinator の保存形式判定と上書き保存のテスト

#include <QtTest>
#include <QFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>

#include "kifusavecoordinator.h"

using KifuSaveCoordinator::SaveFormat;

Q_DECLARE_METATYPE(KifuSaveCoordinator::SaveFormat)

class SaveDialogResponder : public QObject
{
    Q_OBJECT
public:
    bool cancelFile = false;
    QString filePath;
    QMessageBox::StandardButton answer = QMessageBox::Cancel;
public slots:
    void respond()
    {
        if (auto* file = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            if (cancelFile) file->reject();
            else {
                file->selectFile(filePath);
                QMetaObject::invokeMethod(file, "accept");
            }
        } else if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            box->button(answer)->click();
        }
    }
};

class TestKifuSaveCoordinator : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;

    static QByteArray readAllBytes(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        return f.readAll();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void saveDialogGeneratesOnlySelectedFormat_data()
    {
        QTest::addColumn<QString>("suffix");
        QTest::addColumn<SaveFormat>("expectedFormat");
        QTest::newRow("cancel") << QString() << SaveFormat::Kif;
        QTest::newRow("kif") << QStringLiteral("kifu") << SaveFormat::Kif;
        QTest::newRow("ki2") << QStringLiteral("ki2u") << SaveFormat::Ki2;
        QTest::newRow("csa") << QStringLiteral("csa") << SaveFormat::Csa;
        QTest::newRow("jkf") << QStringLiteral("jkf") << SaveFormat::Jkf;
        QTest::newRow("usen") << QStringLiteral("usen") << SaveFormat::Usen;
        QTest::newRow("usi") << QStringLiteral("usi") << SaveFormat::Usi;
    }

    void saveDialogGeneratesOnlySelectedFormat()
    {
        QFETCH(QString, suffix);
        QFETCH(SaveFormat, expectedFormat);
        QTemporaryDir dir;
        SaveDialogResponder responder;
        responder.cancelFile = suffix.isEmpty();
        responder.filePath = dir.filePath(QStringLiteral("record.") + suffix);
        int generated = 0;
        SaveFormat actual = SaveFormat::Kif;
        QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
        QString error;
        const auto path = KifuSaveCoordinator::saveViaDialog(nullptr,
            [&](SaveFormat format) {
                ++generated;
                actual = format;
                return QStringList{QStringLiteral("selected format only")};
            }, PlayMode::NotStarted, {}, {}, {}, {}, false, false, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(generated, responder.cancelFile ? 0 : 1);
        if (responder.cancelFile) QVERIFY(path.isEmpty());
        else {
            QCOMPARE(actual, expectedFormat);
            QCOMPARE(path, responder.filePath);
            QCOMPARE(readAllBytes(path), QByteArray("selected format only\n"));
        }
    }

    void unsavedChangesGuard_data()
    {
        QTest::addColumn<bool>("dirty");
        QTest::addColumn<int>("answer");
        QTest::addColumn<bool>("writeFails");
        QTest::addColumn<bool>("proceed");
        QTest::newRow("clean") << false << int(QMessageBox::Cancel) << false << true;
        QTest::newRow("cancel") << true << int(QMessageBox::Cancel) << false << false;
        QTest::newRow("discard") << true << int(QMessageBox::Discard) << false << true;
        QTest::newRow("save") << true << int(QMessageBox::Save) << false << true;
        QTest::newRow("save-fails") << true << int(QMessageBox::Save) << true << false;
    }

    void unsavedChangesGuard()
    {
        QFETCH(bool, dirty);
        QFETCH(int, answer);
        QFETCH(bool, writeFails);
        QFETCH(bool, proceed);
        QTemporaryDir dir;
        SaveDialogResponder responder;
        responder.answer = static_cast<QMessageBox::StandardButton>(answer);
        int saves = 0;
        const QString path = writeFails ? dir.path() : dir.filePath(QStringLiteral("record.kifu"));
        if (dirty) QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
        const bool accepted = KifuSaveCoordinator::confirmDiscardUnsaved(nullptr, dirty, [&]() {
            ++saves;
            return KifuSaveCoordinator::overwriteExisting(path, {QStringLiteral("unsaved record")});
        });
        QCOMPARE(accepted, proceed);
        QCOMPARE(saves, dirty && answer == int(QMessageBox::Save) ? 1 : 0);
        if (saves && !writeFails) QCOMPARE(readAllBytes(path), QByteArray("unsaved record\n"));
    }

    void unsavedChangesGuard_saveAsCanceled()
    {
        SaveDialogResponder responder;
        responder.answer = QMessageBox::Save;
        responder.cancelFile = true;
        int generated = 0;
        QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
        const bool accepted = KifuSaveCoordinator::confirmDiscardUnsaved(nullptr, true, [&]() {
            QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
            return !KifuSaveCoordinator::saveViaDialog(nullptr, [&](SaveFormat) {
                ++generated;
                return QStringList{QStringLiteral("unsaved record")};
            }, PlayMode::NotStarted, {}, {}, {}, {}).isEmpty();
        });
        QVERIFY(!accepted);
        QCOMPARE(generated, 0);
    }

    void selectedFilterAddsExtension_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("filter");
        QTest::addColumn<QString>("expected");
        QTest::newRow("csa") << QStringLiteral("game") << QStringLiteral("CSA形式 (*.csa)") << QStringLiteral("game.csa");
        QTest::newRow("sjis") << QStringLiteral("game") << QStringLiteral("KIF形式 Shift_JIS (*.kif)") << QStringLiteral("game.kif");
        QTest::newRow("ki2") << QStringLiteral("game") << QStringLiteral("KI2 (*.ki2u)") << QStringLiteral("game.ki2u");
        QTest::newRow("explicit") << QStringLiteral("game.jkf") << QStringLiteral("CSA (*.csa)") << QStringLiteral("game.jkf");
        QTest::newRow("all") << QStringLiteral("game") << QStringLiteral("All files (*)") << QStringLiteral("game.kifu");
        QTest::newRow("cancel") << QString() << QStringLiteral("CSA (*.csa)") << QString();
    }

    void selectedFilterAddsExtension()
    {
        QFETCH(QString, path);
        QFETCH(QString, filter);
        QFETCH(QString, expected);
        QCOMPARE(KifuSaveCoordinator::pathForSelectedFilter(path, filter), expected);
    }

    void shiftJisFailurePreservesExistingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kif"));
        const QByteArray original("original record\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), original.size());
        file.close();
        QString error;
        QVERIFY(!KifuSaveCoordinator::overwriteExisting(path, {QString::fromUtf8("棋譜😀")}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readAllBytes(path), original);
    }

    void writeFailureHasError()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!KifuSaveCoordinator::overwriteExisting(dir.path(), {QStringLiteral("record")}, &error));
        QVERIFY(!error.isEmpty());
    }

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

    void hasKnownSaveExtension()
    {
        const QStringList known = {
            QStringLiteral("a.kif"), QStringLiteral("a.kifu"),
            QStringLiteral("a.ki2"), QStringLiteral("a.ki2u"),
            QStringLiteral("a.csa"), QStringLiteral("a.jkf"),
            QStringLiteral("a.usen"), QStringLiteral("a.usi"),
            QStringLiteral("A.KIF"),
        };
        for (const QString& name : known) {
            QVERIFY2(KifuSaveCoordinator::hasKnownSaveExtension(QStringLiteral("/tmp/") + name),
                     qPrintable(name));
        }

        // 保存形式を決められない拡張子は上書き対象にしない
        QVERIFY(!KifuSaveCoordinator::hasKnownSaveExtension(QStringLiteral("/tmp/a.sfen")));
        QVERIFY(!KifuSaveCoordinator::hasKnownSaveExtension(QStringLiteral("/tmp/a.txt")));
        QVERIFY(!KifuSaveCoordinator::hasKnownSaveExtension(QStringLiteral("/tmp/a")));
        QVERIFY(!KifuSaveCoordinator::hasKnownSaveExtension(QString()));
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

QTEST_MAIN(TestKifuSaveCoordinator)
#include "tst_kifusavecoordinator.moc"
