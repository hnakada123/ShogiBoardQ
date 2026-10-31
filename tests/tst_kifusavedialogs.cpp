/// @file tst_kifusavecoordinator.cpp
/// @brief KifuSaveDialogs の保存形式判定と上書き保存のテスト

#include <QtTest>
#include <QFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>

#include "kifufilefilter.h"
#include "kifusavedialogs.h"
#include "kifusavefile.h"
#include "matchplayers.h"

using SaveFormat = KifuFormat;

Q_DECLARE_METATYPE(KifuFormat)

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

class TestKifuSaveDialogs : public QObject
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

    /// 改行コードを LF に正規化して読み出す
    ///
    /// KifuSaveFile は QIODevice::Text で書き出すため、
    /// 改行はプラットフォーム標準（Windows では CRLF、それ以外は LF）になる。
    /// KIF/KI2 は Windows 系ツールの慣習で CRLF が期待され、CSA/JKF/USI/USEN も
    /// 読み込み側が "\r?\n" を正規化するため、どちらの改行でも正しく往復する。
    /// 内容の検証では改行コードの違いと、UTF-8 の KIF・KI2 の先頭の BOM（utf8KifAndKi2_startWithBom で確かめる）を無視する。
    static QByteArray readAllNormalized(const QString& path)
    {
        QByteArray bytes = readAllBytes(path);
        if (bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0, 3);
        bytes.replace("\r\n", "\n");
        return bytes;
    }

    /// 形式・文字コードによらない行を作る
    static KifuSaveFile::LineGenerator fixedLines(const QStringList& lines)
    {
        return [lines](KifuFormat, KifuEncoding) { return lines; };
    }

    /// KIF の行を作る（先頭行の文字コードの宣言は、書き出しと同じく渡された文字コードにする）
    static KifuSaveFile::LineGenerator kifLines(const QStringList& body)
    {
        return [body](KifuFormat, KifuEncoding encoding) {
            return QStringList{encoding == KifuEncoding::ShiftJis ? QStringLiteral("#KIF version=2.0 encoding=Shift_JIS")
                                                                   : QStringLiteral("#KIF version=2.0 encoding=UTF-8")}
                + body;
        };
    }

    /// UTF-8 として読み出し、改行コードを LF に正規化する
    static QString readAllTextNormalized(const QString& path)
    {
        return QString::fromUtf8(readAllNormalized(path));
    }

    /// 改行コードを LF に正規化する
    static QString normalizeNewlines(QString text)
    {
        return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
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
        const auto path = KifuSaveDialogs::saveViaDialog(nullptr,
            [&](SaveFormat format, KifuEncoding) {
                ++generated;
                actual = format;
                return QStringList{QStringLiteral("selected format only")};
            }, QString(), false, false, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(generated, responder.cancelFile ? 0 : 1);
        if (responder.cancelFile) QVERIFY(path.isEmpty());
        else {
            QCOMPARE(actual, expectedFormat);
            QCOMPARE(path, responder.filePath);
            QCOMPARE(readAllNormalized(path), QByteArray("selected format only\n"));
            // UTF-8 の KIF・KI2 だけ BOM を付ける
            QCOMPARE(readAllBytes(path).startsWith("\xEF\xBB\xBF"),
                     expectedFormat == SaveFormat::Kif || expectedFormat == SaveFormat::Ki2);
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
        const bool accepted = KifuSaveDialogs::confirmDiscardUnsaved(nullptr, dirty, [&]() {
            ++saves;
            return KifuSaveFile::write(path, fixedLines({QStringLiteral("unsaved record")}));
        });
        QCOMPARE(accepted, proceed);
        QCOMPARE(saves, dirty && answer == int(QMessageBox::Save) ? 1 : 0);
        if (saves && !writeFails) QCOMPARE(readAllNormalized(path), QByteArray("unsaved record\n"));
    }

    void unsavedChangesGuard_saveAsCanceled()
    {
        SaveDialogResponder responder;
        responder.answer = QMessageBox::Save;
        responder.cancelFile = true;
        int generated = 0;
        QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
        const bool accepted = KifuSaveDialogs::confirmDiscardUnsaved(nullptr, true, [&]() {
            QTimer::singleShot(0, &responder, &SaveDialogResponder::respond);
            return !KifuSaveDialogs::saveViaDialog(nullptr, [&](SaveFormat, KifuEncoding) {
                ++generated;
                return QStringList{QStringLiteral("unsaved record")};
            }, QString()).isEmpty();
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
        QCOMPARE(KifuFileFilter::pathForSaving(path, filter), expected);
    }

    /// 保存の絞り込みは保存できる拡張子の表の順（UTF-8 の KIF が先頭）で、選んだ絞り込みからその拡張子に戻せる
    void saveFilterComesFromTheTable()
    {
        const QStringList filters = KifuFileFilter::forSaving().split(QStringLiteral(";;"));
        const auto& targets = KifuFormats::saveTargets();
        QCOMPARE(filters.size(), targets.size() + 1);
        QCOMPARE(filters.first(), QStringLiteral("KIF形式 UTF-8 (*.kifu)"));
        QCOMPARE(filters.last(), QStringLiteral("すべてのファイル (*)"));
        for (qsizetype i = 0; i < targets.size(); ++i) {
            QVERIFY2(filters.at(i).endsWith(QStringLiteral("(*.%1)").arg(targets.at(i).suffix)), qPrintable(filters.at(i)));
            QCOMPARE(KifuFileFilter::pathForSaving(QStringLiteral("game"), filters.at(i)),
                     QStringLiteral("game.") + targets.at(i).suffix);
        }
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
        QVERIFY(!KifuSaveFile::write(path, fixedLines({QString::fromUtf8("棋譜😀")}), &error));
        // ダイアログを出さない呼び出し（自動化 API・CLI）は、保存できない文字を示してエラーにする
        QVERIFY(error.contains(QString::fromUtf8("😀")));
        QCOMPARE(readAllBytes(path), original);
    }

    void charactersNotInShiftJis()
    {
        QCOMPARE(KifuSaveFile::charactersNotInShiftJis(QString::fromUtf8("先手：鈴木\n▲７六歩")), QString());
        // 中国語UIの既定の対局者名「您」と絵文字（サロゲートペア）を、重複なく出現順に返す
        QCOMPARE(KifuSaveFile::charactersNotInShiftJis(QString::fromUtf8("先手：您\n後手：您😀X😀")),
                 QString::fromUtf8("您😀"));
        QCOMPARE(KifuSaveFile::charactersNotInShiftJis(QString::fromUtf8("您😀"), 1), QString::fromUtf8("您"));
    }

    /// 自動保存のファイル名は、対局者名の制御文字・向きを変える文字を置き換え、長さを切る（セキュリティ監査 S21）
    void defaultSaveFileNameIsSafe()
    {
        const QString black = QStringLiteral("Alice") + QChar(0) + QStringLiteral("\r../\u202Etxt.exe");
        const QDateTime now(QDate(2026, 10, 23), QTime(12, 0, 0));
        const QString name = KifuSaveFile::makeDefaultSaveFileName(black, QString(300, QLatin1Char('b')), now);
        QVERIFY2(name.startsWith(QStringLiteral("20261023_120000_Alice")), qPrintable(name));
        QVERIFY2(name.endsWith(QStringLiteral(".kifu")), qPrintable(name));
        QVERIFY(!name.contains(QChar(0)));
        QVERIFY(!name.contains(QLatin1Char('/')));
        QVERIFY(!name.contains(QChar(0x202E)));
        QVERIFY(!name.contains(QLatin1Char('\r')));
        QVERIFY2(name.toUtf8().size() <= 255, qPrintable(name));
        // 空白だけの名前は、既定の呼び名にする。両方とも無ければ日時だけ
        QVERIFY(KifuSaveFile::makeDefaultSaveFileName(black, QStringLiteral("   "), now).endsWith(QStringLiteral("_vs_P2.kifu")));
        QCOMPARE(KifuSaveFile::makeDefaultSaveFileName(QString(), QStringLiteral(" "), now, QStringLiteral("csa")),
                 QStringLiteral("20261023_120000_Session.csa"));
    }

    void shiftJisFallback_data()
    {
        QTest::addColumn<QMessageBox::StandardButton>("answer");
        QTest::newRow("save-as-utf8") << QMessageBox::Save;
        QTest::newRow("cancel") << QMessageBox::Cancel;
    }

    /// 画面からの保存では、Shift_JIS で表せない文字があれば UTF-8 で保存するか確認する
    void shiftJisFallback()
    {
        QFETCH(QMessageBox::StandardButton, answer);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kif"));
        const QByteArray original("original record\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), original.size());
        file.close();

        const QStringList body{QString::fromUtf8("先手：您"), QString::fromUtf8("   1 ７六歩(77)   ( 0:01/00:00:01)")};
        QList<KifuEncoding> requested;
        const KifuSaveFile::LineGenerator lines = [&](KifuFormat format, KifuEncoding encoding) {
            requested.append(encoding);
            return kifLines(body)(format, encoding);
        };
        QWidget parent;
        SaveDialogResponder responder;
        responder.answer = answer;
        QString messageText;
        QTimer::singleShot(0, &responder, [&]() {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) messageText = box->text();
            responder.respond();
        });
        QString error;
        const bool ok = KifuSaveDialogs::overwriteExisting(&parent, path, lines, &error);
        QCOMPARE(requested.first(), KifuEncoding::ShiftJis);
        QVERIFY(messageText.contains(QString::fromUtf8("您")));
        QVERIFY2(error.isEmpty(), qPrintable(error));  // キャンセルはエラーとして通知しない
        if (answer == QMessageBox::Save) {
            QVERIFY(ok);
            // UTF-8 で行を作り直して保存する（先頭行の encoding 宣言も UTF-8）
            QCOMPARE(requested, (QList<KifuEncoding>{KifuEncoding::ShiftJis, KifuEncoding::Utf8}));
            QCOMPARE(readAllTextNormalized(path),
                     kifLines(body)(KifuFormat::Kif, KifuEncoding::Utf8).join(QLatin1Char('\n')) + QLatin1Char('\n'));
        } else {
            QVERIFY(!ok);
            QCOMPARE(readAllBytes(path), original);
        }
    }

    /// Shift_JIS で表せる内容なら確認を出さずに Shift_JIS で保存する
    void shiftJisRepresentableDoesNotAsk()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("game.kif"));
        QWidget parent;
        bool asked = false;
        QTimer::singleShot(0, &parent, [&]() {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                asked = true;
                box->reject();
            }
        });
        QString error;
        QVERIFY(KifuSaveDialogs::overwriteExisting(&parent, path, kifLines({QString::fromUtf8("先手：鈴木")}), &error));
        QVERIFY(!asked);
        QVERIFY(readAllBytes(path).startsWith("#KIF version=2.0 encoding=Shift_JIS"));
    }

    void writeFailureHasError()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!KifuSaveFile::write(dir.path(), fixedLines({QStringLiteral("record")}), &error));
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
        QCOMPARE(KifuSaveFile::saveFormatForPath(path), expected);
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
            QVERIFY2(KifuSaveFile::hasKnownSaveExtension(QStringLiteral("/tmp/") + name),
                     qPrintable(name));
        }

        // 保存形式を決められない拡張子は上書き対象にしない
        QVERIFY(!KifuSaveFile::hasKnownSaveExtension(QStringLiteral("/tmp/a.sfen")));
        QVERIFY(!KifuSaveFile::hasKnownSaveExtension(QStringLiteral("/tmp/a.txt")));
        QVERIFY(!KifuSaveFile::hasKnownSaveExtension(QStringLiteral("/tmp/a")));
        QVERIFY(!KifuSaveFile::hasKnownSaveExtension(QString()));
    }

    void encodingForPath()
    {
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.kif")), KifuEncoding::ShiftJis);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.KI2")), KifuEncoding::ShiftJis);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.kifu")), KifuEncoding::Utf8);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.ki2u")), KifuEncoding::Utf8);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.csa")), KifuEncoding::Utf8);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.usi")), KifuEncoding::Utf8);
        QCOMPARE(KifuSaveFile::encodingForPath(QStringLiteral("/tmp/a.txt")), KifuEncoding::Utf8);
    }

    /// 行は保存先の拡張子の形式と文字コードで1回だけ作る（Shift_JIS で表せれば作り直さない）
    void writeAsksTheTargetFormatAndEncoding_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<SaveFormat>("format");
        QTest::addColumn<int>("encoding");
        QTest::newRow("kifu") << QStringLiteral("a.kifu") << SaveFormat::Kif << int(KifuEncoding::Utf8);
        QTest::newRow("kif") << QStringLiteral("a.kif") << SaveFormat::Kif << int(KifuEncoding::ShiftJis);
        QTest::newRow("ki2") << QStringLiteral("a.ki2") << SaveFormat::Ki2 << int(KifuEncoding::ShiftJis);
        QTest::newRow("ki2u") << QStringLiteral("a.ki2u") << SaveFormat::Ki2 << int(KifuEncoding::Utf8);
        QTest::newRow("csa") << QStringLiteral("a.csa") << SaveFormat::Csa << int(KifuEncoding::Utf8);
        QTest::newRow("usen") << QStringLiteral("a.usen") << SaveFormat::Usen << int(KifuEncoding::Utf8);
    }

    void writeAsksTheTargetFormatAndEncoding()
    {
        QFETCH(QString, name);
        QFETCH(SaveFormat, format);
        QFETCH(int, encoding);
        QTemporaryDir dir;
        QList<QPair<KifuFormat, KifuEncoding>> requested;
        QString error;
        QVERIFY2(KifuSaveFile::write(dir.filePath(name), [&](KifuFormat f, KifuEncoding e) {
            requested.append({f, e});
            return QStringList{QStringLiteral("record")};
        }, &error), qPrintable(error));
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first, format);
        QCOMPARE(int(requested.first().second), encoding);
    }

    /// UTF-8 の KIF・KI2 には BOM を付け、ほかの形式には付けない（ShogiGUI は BOM の無い UTF-8 の棋譜を
    /// Shift_JIS として読んでしまう。ShogiGUI との相互読み込みの検証）
    void utf8KifAndKi2_startWithBom_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("bom");
        QTest::newRow("kifu") << QStringLiteral("a.kifu") << true;
        QTest::newRow("ki2u") << QStringLiteral("a.ki2u") << true;
        QTest::newRow("csa") << QStringLiteral("a.csa") << false;
        QTest::newRow("jkf") << QStringLiteral("a.jkf") << false;
        QTest::newRow("usi") << QStringLiteral("a.usi") << false;
        QTest::newRow("usen") << QStringLiteral("a.usen") << false;
    }

    void utf8KifAndKi2_startWithBom()
    {
        QFETCH(QString, name);
        QFETCH(bool, bom);
        QTemporaryDir dir;
        const QString path = dir.filePath(name);
        QString error;
        QVERIFY2(KifuSaveFile::write(path, [](KifuFormat, KifuEncoding) {
            return QStringList{QString::fromUtf8("先手：鈴木")};
        }, &error), qPrintable(error));
        const QByteArray bytes = readAllBytes(path);
        QCOMPARE(bytes.startsWith("\xEF\xBB\xBF"), bom);
        QCOMPARE(normalizeNewlines(QString::fromUtf8(bom ? bytes.mid(3) : bytes)), QString::fromUtf8("先手：鈴木\n"));
    }

    /// Shift_JIS で表せない文字で UTF-8 に書き直した .kif も BOM を付ける。CLI の変換の出力（writeLines）も同じ
    void utf8Fallback_andWriteLines_startWithBom()
    {
        QTemporaryDir dir;
        const QString kif = dir.filePath(QStringLiteral("fallback.kif"));
        QString error;
        QVERIFY2(KifuSaveFile::write(kif, [](KifuFormat, KifuEncoding) {
            return QStringList{QString::fromUtf8("先手：𠮷田")};
        }, &error, [](const QString&) { return true; }), qPrintable(error));
        QVERIFY(readAllBytes(kif).startsWith("\xEF\xBB\xBF"));

        const QString kifu = dir.filePath(QStringLiteral("cli.kifu"));
        QVERIFY2(KifuSaveFile::writeLines(kifu, {QString::fromUtf8("先手：鈴木")}, KifuFormat::Kif,
                                          KifuEncoding::Utf8, &error), qPrintable(error));
        QVERIFY(readAllBytes(kifu).startsWith("\xEF\xBB\xBF"));
        const QString jkf = dir.filePath(QStringLiteral("cli.jkf"));
        QVERIFY2(KifuSaveFile::writeLines(jkf, {QStringLiteral("{}")}, KifuFormat::Jkf, KifuEncoding::Utf8, &error),
                 qPrintable(error));
        QVERIFY(!readAllBytes(jkf).startsWith("\xEF\xBB\xBF"));
    }

    /// 作った行を文字コードを指定して書く（CLI の変換の出力）。Shift_JIS で表せない文字は示して失敗する
    void writeLinesUsesTheGivenEncoding()
    {
        QStringEncoder probe("Shift-JIS");
        if (!probe.isValid()) QSKIP("Shift_JIS codec is not available in this Qt build");
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("out.txt"));
        QString error;
        QVERIFY2(KifuSaveFile::writeLines(path, {QString::fromUtf8("先手：鈴木")}, KifuFormat::Kif, KifuEncoding::ShiftJis, &error),
                 qPrintable(error));
        QStringDecoder decoder("Shift-JIS");
        QCOMPARE(normalizeNewlines(decoder(readAllBytes(path))), QString::fromUtf8("先手：鈴木\n"));
        QVERIFY(!KifuSaveFile::writeLines(path, {QString::fromUtf8("您")}, KifuFormat::Kif, KifuEncoding::ShiftJis, &error));
        QVERIFY(error.contains(QString::fromUtf8("您")));
        QVERIFY2(KifuSaveFile::writeLines(path, {QString::fromUtf8("您")}, KifuFormat::Csa, KifuEncoding::Utf8, &error), qPrintable(error));
        QCOMPARE(readAllTextNormalized(path), QString::fromUtf8("您\n"));
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
        QVERIFY2(KifuSaveFile::write(path, fixedLines(lines), &err), qPrintable(err));

        const QString text = readAllTextNormalized(path);
        QCOMPARE(text, lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    }

    /// 既存の内容は完全に置き換えられる
    void overwriteExisting_replacesPreviousContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.usi"));

        QString err;
        QVERIFY(KifuSaveFile::write(
            path, fixedLines({QStringLiteral("position startpos moves 7g7f 3c3d 2g2f")}), &err));
        QVERIFY(KifuSaveFile::write(
            path, fixedLines({QStringLiteral("position startpos moves 7g7f")}), &err));

        QCOMPARE(readAllTextNormalized(path),
                 QStringLiteral("position startpos moves 7g7f\n"));
    }

    /// .kifu は UTF-8 で、encoding 宣言も UTF-8
    void overwriteExisting_kifuKeepsUtf8Header()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kifu"));

        const QStringList body = {QStringLiteral("先手：鈴木"), QStringLiteral("   1 ７六歩(77)")};
        QString err;
        QVERIFY2(KifuSaveFile::write(path, kifLines(body), &err), qPrintable(err));

        const QString text = readAllTextNormalized(path);
        QCOMPARE(text, QStringLiteral("#KIF version=2.0 encoding=UTF-8\n") + body.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    }

    /// .kif は Shift_JIS で書かれ、encoding 宣言も Shift_JIS で作る
    void overwriteExisting_kifUsesShiftJis()
    {
        QStringEncoder probe("Shift-JIS");
        if (!probe.isValid()) {
            QSKIP("Shift_JIS codec is not available in this Qt build");
        }

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.kif"));

        QString err;
        QVERIFY2(KifuSaveFile::write(path, kifLines({QStringLiteral("先手：鈴木"), QStringLiteral("   1 ７六歩(77)")}), &err),
                 qPrintable(err));

        const QByteArray bytes = readAllBytes(path);
        QStringDecoder decoder("Shift-JIS");
        const QString text = normalizeNewlines(decoder(bytes));
        QVERIFY(text.startsWith(QStringLiteral("#KIF version=2.0 encoding=Shift_JIS\n")));
        QVERIFY(text.contains(QStringLiteral("先手：鈴木")));
        QVERIFY(text.contains(QStringLiteral("７六歩(77)")));

        // UTF-8 のバイト列ではない（= 実際に Shift_JIS で書かれている）
        QVERIFY(!QString::fromUtf8(bytes).contains(QStringLiteral("先手：鈴木")));
    }
};

QTEST_MAIN(TestKifuSaveDialogs)
#include "tst_kifusavedialogs.moc"
