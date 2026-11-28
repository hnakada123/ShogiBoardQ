#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include "applicationfonts.h"
#include "pvboarddialog.h"
#include "settingscommon.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumeshogisettings.h"

class TestTsumeshogiGeneratorGui : public QObject
{
    Q_OBJECT
    const QString sfen = QStringLiteral("9/9/6R1+R/5k3/9/7+S1/9/9/9 b 2b4g3s4n4l18p 1");
    const QStringList pv{QStringLiteral("3c5c+"), QStringLiteral("4d4e"), QStringLiteral("1c4c"),
                         QStringLiteral("P*4d"), QStringLiteral("4c4d")};
    QString savePath;

    QPushButton* button(TsumeshogiGeneratorDialog& dialog, const char* name)
    { return dialog.findChild<QPushButton*>(QLatin1String(name)); }
    void addResult(TsumeshogiGeneratorDialog& dialog, const QString& position = QString())
    {
        const QString found = position.isEmpty() ? sfen : position;
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onPositionFound", Q_ARG(QString, found), Q_ARG(QStringList, pv)));
    }
    void registerEngine()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray("Engines"); settings.setArrayIndex(0);
        settings.setValue("name", "Audit USI");
        settings.setValue("path", QStringLiteral(AUDIT_DIR "/mock_usi.py"));
        settings.endArray();
    }
    void dismissError()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept();
    }
    void chooseSaveFile()
    {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            dialog->selectFile(savePath);
            static_cast<QDialog*>(dialog)->accept();
        }
    }

private slots:
    void init() { SettingsCommon::openSettings().clear(); }
    void presentationAndExport()
    {
        registerEngine();
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("generatorResults"));
        auto* progress = dialog.findChild<QProgressBar*>();
        auto* limit = dialog.findChild<QSpinBox*>(QStringLiteral("generatorMaxPositions"));
        QVERIFY(!button(dialog, "generatorSave")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopyAll")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        QCOMPARE(progress->maximum(), 10);
        QVERIFY(table->height() >= 180);
        QCOMPARE(dialog.findChild<QScrollArea*>()->verticalScrollBar()->maximum(), 0);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-empty.png")));
        limit->setValue(3);
        // 2つ目は手数だけ違う局面（コピーの順を見分ける）
        const QString second = sfen.chopped(1) + QStringLiteral("3");
        addResult(dialog);
        addResult(dialog, second);
        QCOMPARE(progress->maximum(), 3);
        QCOMPARE(progress->value(), 2);
        QVERIFY(button(dialog, "generatorSave")->isEnabled());
        QVERIFY(button(dialog, "generatorCopyAll")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        // 逆順に選んでも表示順でコピーし、SFEN全文と手順を保持する。
        table->selectionModel()->select(table->model()->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        table->selectionModel()->select(table->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        dialog.findChild<QCheckBox*>(QStringLiteral("generatorIncludePv"))->setChecked(true);
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        QTest::keyClick(table, Qt::Key_C, Qt::ControlModifier);
        const QString moves = QStringLiteral(" moves ") + pv.join(QLatin1Char(' '));
        QCOMPARE(QApplication::clipboard()->text(), sfen + moves + '\n' + second + moves);
        table->setCurrentCell(0, 1);
        QTest::keyClick(table, Qt::Key_Return);
        QTRY_VERIFY(dialog.findChild<PvBoardDialog*>());
        dialog.findChild<PvBoardDialog*>()->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                         table->visualRect(table->model()->index(0, 1)).center());
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                          table->visualRect(table->model()->index(0, 1)).center());
        QTRY_COMPARE(dialog.findChildren<PvBoardDialog*>().size(), 1);
        dialog.findChild<PvBoardDialog*>()->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        limit->setValue(100);
        for (int i = 0; i < 50; ++i) addResult(dialog);
        QTest::qWait(30);
        table->verticalScrollBar()->setValue(0);
        addResult(dialog);
        QCOMPARE(table->verticalScrollBar()->value(), 0);
        table->clearSelection();
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onProgressUpdated", Q_ARG(int, 12000), Q_ARG(int, 53), Q_ARG(qint64, 96000)));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorFinished"));
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"))->text().contains(QStringLiteral("生成完了")));
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-results.png")));
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(true);
        // 説明を開いても設定欄はスクロールせずに全項目が見える（必要ならダイアログが高くなる）
        auto* settingsScroll = dialog.findChild<QScrollArea*>(QStringLiteral("generatorSettingsScroll"));
        QTRY_COMPARE(settingsScroll->verticalScrollBar()->maximum(), 0);
        QVERIFY(table->height() >= 180);
        QTest::qWait(50);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-help.png")));
        // 閉じると設定欄が縮み、結果一覧に高さが戻る
        const int tableHeightWithHelp = table->height();
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(false);
        QTRY_VERIFY(table->height() > tableHeightWithHelp);
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(true);
        QTRY_COMPARE(settingsScroll->verticalScrollBar()->maximum(), 0);
        dialog.close();
        QVERIFY(TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded());
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions(), 100);
        TsumeshogiGeneratorDialog restored;
        QCOMPARE(restored.size(), dialog.size());
        QVERIFY(restored.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->isChecked());
    }
    void saveResults()
    {
        QTemporaryDir files;
        QVERIFY(files.isValid());
        savePath = files.filePath(QStringLiteral("positions.txt"));
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        addResult(dialog);
        dialog.findChild<QCheckBox*>(QStringLiteral("generatorIncludePv"))->setChecked(true);
        QTimer::singleShot(0, this, &TestTsumeshogiGeneratorGui::chooseSaveFile);
        QTest::mouseClick(button(dialog, "generatorSave"), Qt::LeftButton);
        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(bytes.startsWith("#"));
        QVERIFY(bytes.endsWith((sfen + " moves " + pv.join(' ') + '\n').toUtf8()));
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(), files.path());
    }
    void generationLifecycle()
    {
        registerEngine();
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* generator = dialog.findChild<TsumeshogiGenerator*>();
        auto* status = dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"));
        auto* progress = dialog.findChild<QProgressBar*>();
        auto* limit = dialog.findChild<QSpinBox*>(QStringLiteral("generatorMaxPositions"));
        limit->setValue(0);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QVERIFY(generator->isRunning());
        QCOMPARE(progress->maximum(), 0);
        QVERIFY(!button(dialog, "generatorRestoreDefaults")->isEnabled());
        QVERIFY(!limit->isEnabled());
        QTest::mouseClick(button(dialog, "generatorStop"), Qt::LeftButton);
        QVERIFY(!generator->isRunning());
        QVERIFY(status->text().contains(QStringLiteral("停止しました")));
        QVERIFY(progress->maximum() > 0);
        QVERIFY(button(dialog, "generatorRestoreDefaults")->isEnabled());
        // 再開時は前の件数・検査状況・経過時間をリセットする。
        addResult(dialog);
        limit->setValue(2);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QCOMPARE(dialog.findChild<QTableWidget*>()->rowCount(), 0);
        QCOMPARE(progress->maximum(), 2);
        QCOMPARE(progress->value(), 0);
        QVERIFY(!button(dialog, "generatorSave")->isEnabled());
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!generator->isRunning());
        QVERIFY(!dialog.isVisible());
        // errorOccurred に続く finished でエラー表示が消えない。
        QTimer::singleShot(0, this, &TestTsumeshogiGeneratorGui::dismissError);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorError", Q_ARG(QString, QStringLiteral("test failure"))));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorFinished"));
        QVERIFY(status->text().contains(QStringLiteral("test failure")));
    }
    void noEngineAndLargeFont()
    {
        TsumeshogiSettings::tsumeshogiGeneratorFontSize.set(16);
        TsumeshogiGeneratorDialog dialog;
        dialog.resize(940, 800);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!button(dialog, "generatorStart")->isEnabled());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"))->text().contains(QStringLiteral("登録")));
        QVERIFY(dialog.findChild<QTableWidget*>()->height() >= 120);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-large-font.png")));
    }
    void retrogradeModeToggleAndPersistence()
    {
        registerEngine();
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString seedPath = temp.filePath(QStringLiteral("seeds.txt"));
        const QString excludePath = temp.filePath(QStringLiteral("exclude.txt"));
        const QString cpPath = temp.filePath(QStringLiteral("cp.json"));
        {
            QFile f(seedPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write("7nk/7nn/9/9/9/9/9/9/9 b N 1 moves N*2c\n");
        }
        {
            QFile f(excludePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write("9/9/6R1+R/5k3/9/7+S1/9/9/9 b 2b4g3s4n4l18p 1 moves 3c5c+ 4d4e 1c4c P*4d 4c4d\n");
        }

        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        auto* mode = dialog.findChild<QComboBox*>(QStringLiteral("generatorMode"));
        auto* retroFiles = dialog.findChild<QWidget*>(QStringLiteral("generatorRetroFiles"));
        auto* requireNoSurplus = dialog.findChild<QCheckBox*>(QStringLiteral("generatorRequireNoSurplus"));
        auto* excludeSimilar = dialog.findChild<QCheckBox*>(QStringLiteral("generatorExcludeSimilar"));
        auto* retroAttack = dialog.findChild<QSpinBox*>(QStringLiteral("generatorRetroMaxAttack"));
        auto* beam = dialog.findChild<QSpinBox*>(QStringLiteral("generatorBeamWidth"));
        auto* seedEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorSeedFile"));
        auto* excludeEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorExcludeFile"));
        auto* cpEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorCheckpointFile"));
        auto* resumeCheck = dialog.findChild<QCheckBox*>(QStringLiteral("generatorResumeCheckpoint"));
        QVERIFY(mode && retroFiles && requireNoSurplus && excludeSimilar && retroAttack && beam
                && seedEdit && excludeEdit && cpEdit && resumeCheck);

        // 既定はランダム配置モードで、逆算専用欄は非表示
        QCOMPARE(mode->currentIndex(), 0);
        QVERIFY(!retroFiles->isVisible());

        // 逆算・編集モードへ切り替えると専用欄が表示され、38枚上限・除外ファイル・途中保存設定を編集できる
        mode->setCurrentIndex(1);
        QTRY_VERIFY(retroFiles->isVisible());
        QVERIFY(requireNoSurplus->isChecked());
        QVERIFY(excludeSimilar->isChecked());
        retroAttack->setValue(24);
        beam->setValue(64);
        seedEdit->setText(seedPath);
        excludeEdit->setText(excludePath);
        cpEdit->setText(cpPath);
        resumeCheck->setChecked(true);
        QVERIFY(!seedEdit->isEnabled());
        QVERIFY(excludeEdit->isEnabled());
        resumeCheck->setChecked(false);
        QVERIFY(seedEdit->isEnabled());

        // 逆算生成器の進捗・採択シグナル受信と停止ライフサイクル
        auto* retroGen = dialog.findChild<TsumeRetrogradeGenerator*>();
        QVERIFY(retroGen != nullptr);
        TsumeRetrogradeGenerator::Progress prog;
        prog.phase = QStringLiteral("growing");
        prog.maxPliesReached = 15;
        prog.frontierNodes = 12;
        prog.generatedCandidates = 345;
        prog.found = 1;
        prog.elapsedMs = 42000;
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onRetroPositionFound",
                                          Q_ARG(QString, sfen), Q_ARG(QStringList, pv),
                                          Q_ARG(int, 1), Q_ARG(int, 30)));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onRetroProgressUpdated",
                                          Q_ARG(TsumeRetrogradeGenerator::Progress, prog)));
        QCOMPARE(dialog.findChild<QTableWidget*>(QStringLiteral("generatorResults"))->rowCount(), 1);

        // 閉じて再生成すると逆算モード設定が永続化されていること
        dialog.close();
        TsumeshogiGeneratorDialog restored;
        QCOMPARE(restored.findChild<QComboBox*>(QStringLiteral("generatorMode"))->currentIndex(), 1);
        QCOMPARE(restored.findChild<QSpinBox*>(QStringLiteral("generatorRetroMaxAttack"))->value(), 24);
        QCOMPARE(restored.findChild<QSpinBox*>(QStringLiteral("generatorBeamWidth"))->value(), 64);
        QCOMPARE(restored.findChild<QLineEdit*>(QStringLiteral("generatorSeedFile"))->text(), seedPath);
        QCOMPARE(restored.findChild<QLineEdit*>(QStringLiteral("generatorExcludeFile"))->text(), excludePath);
        QCOMPARE(restored.findChild<QLineEdit*>(QStringLiteral("generatorCheckpointFile"))->text(), cpPath);
    }
    void retrogradeEndToEndResumeAndExclude()
    {
        const QString mockEngine = QStringLiteral(APP_BUILD "/tests/mock_retro_engine");
        QVERIFY2(QFile::exists(mockEngine), qPrintable(mockEngine));
        {
            auto& settings = SettingsCommon::openSettings();
            settings.beginWriteArray("Engines"); settings.setArrayIndex(0);
            settings.setValue("name", "MockRetro");
            settings.setValue("path", mockEngine);
            settings.endArray();
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString cpPath = temp.filePath(QStringLiteral("gui_retro_cp.json"));
        const QString excludePath = temp.filePath(QStringLiteral("gui_exclude.txt"));
        const QString seedPath = temp.filePath(QStringLiteral("gui_seeds.txt"));
        savePath = temp.filePath(QStringLiteral("gui_retro_out.txt"));

        // 乱数に頼らない: 同じ条件（完全厳密・この模擬エンジン）で採択されると分かっている5手詰を、
        // 種局面ファイルに並べる。種局面は並べた順に評価・厳密証明されるので、採択される局面が
        // 乱数の種によらず決まり、どの段階も数秒で終わる。
        // 以前は0手の詰み局面から乱数だけで育てていたので、種によっては1局面を見つけるのに数十秒かかり、
        // 60秒の待ちを超えて落ちることがあった（乱数の種は時刻から決まるので、実行のたびに変わる）。
        // 局面を複数並べるのは、高い負荷で内蔵探索の事前ふるい分けが時間切れになり、種局面が
        // 捨てられることがあるため（その場合は次の種局面が採択される）。
        const QStringList seedPositions{
            QStringLiteral("5R3/3sk4/3p5/6L2/9/2+B6/9/9/9 b rb4g3s4n3l17p 1"),
            QStringLiteral("9/9/9/9/9/9/3r5/4+B3+R/1Sk1+l4 b b4g3s4n3l18p 1"),
            QStringLiteral("9/9/9/9/9/9/3n1+PS2/4l2+N1/5k1S1 b Rr2b4g2s2n3l17p 1"),
            QStringLiteral("9/9/9/9/9/5l3/5+P3/7G1/4+p1kB1 b RLrb3g4s4n2l16p 1"),
            QStringLiteral("9/9/9/9/9/9/7R1/7Rg/8k b 2b3g4s4n4l18p 1"),
            QStringLiteral("1s7/k1+B6/9/B8/9/9/9/9/9 b 2r4g3s4n4l18p 1"),
            QStringLiteral("9/9/9/9/9/9/6+n2/4+S2kB/4G+Br2 b r3g3s3n4l18p 1"),
            QStringLiteral("9/9/9/9/9/9/6+R2/7sB/5+B+n1k b r4g3s3n4l18p 1"),
        };
        {
            QFile seeds(seedPath);
            QVERIFY(seeds.open(QIODevice::WriteOnly | QIODevice::Text));
            seeds.write(seedPositions.join(QLatin1Char('\n')).toUtf8() + '\n');
        }

        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        auto* mode = dialog.findChild<QComboBox*>(QStringLiteral("generatorMode"));
        auto* target = dialog.findChild<QSpinBox*>(QStringLiteral("generatorTargetMoves"));
        auto* limit = dialog.findChild<QSpinBox*>(QStringLiteral("generatorMaxPositions"));
        auto* retroAttack = dialog.findChild<QSpinBox*>(QStringLiteral("generatorRetroMaxAttack"));
        auto* retroDefend = dialog.findChild<QSpinBox*>(QStringLiteral("generatorRetroMaxDefend"));
        auto* beam = dialog.findChild<QSpinBox*>(QStringLiteral("generatorBeamWidth"));
        auto* branch = dialog.findChild<QSpinBox*>(QStringLiteral("generatorBranchFactor"));
        auto* budget = dialog.findChild<QSpinBox*>(QStringLiteral("generatorVerifyBudget"));
        auto* allowFinal = dialog.findChild<QCheckBox*>(QStringLiteral("generatorAllowFinalAlternatives"));
        auto* seedEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorSeedFile"));
        auto* excludeEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorExcludeFile"));
        auto* cpEdit = dialog.findChild<QLineEdit*>(QStringLiteral("generatorCheckpointFile"));
        auto* resumeCheck = dialog.findChild<QCheckBox*>(QStringLiteral("generatorResumeCheckpoint"));
        auto* includePv = dialog.findChild<QCheckBox*>(QStringLiteral("generatorIncludePv"));
        auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("generatorResults"));
        auto* progress = dialog.findChild<QProgressBar*>(QStringLiteral("generatorProgress"));
        auto* status = dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"));
        auto* retroGen = dialog.findChild<TsumeRetrogradeGenerator*>();
        QVERIFY(mode && target && limit && retroAttack && retroDefend && beam && branch && budget
                && allowFinal && seedEdit && excludeEdit && cpEdit && resumeCheck && includePv && table
                && progress && status && retroGen);

        // 1. GUI から逆算・編集モードで 5手詰（完全厳密）を1問生成しチェックポイント保存
        //    （種局面ファイルの先頭の局面が、厳密証明を通って採択される）
        mode->setCurrentIndex(1);
        target->setValue(5);
        limit->setValue(1);
        retroAttack->setValue(8);
        retroDefend->setValue(5);
        beam->setValue(8);
        branch->setValue(16);
        budget->setValue(60);
        allowFinal->setChecked(false);
        seedEdit->setText(seedPath);
        cpEdit->setText(cpPath);
        resumeCheck->setChecked(false);

        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!retroGen->isRunning() && table->rowCount() == 1, 60000);
        QCOMPARE(progress->value(), 1);
        QCOMPARE(progress->maximum(), 1);
        QVERIFY(status->text().contains(QStringLiteral("生成完了")));
        QVERIFY(QFile::exists(cpPath));
        const QString firstSfen = table->item(0, 1)->text();
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("5"));

        // 逆算モードのコメントヘッダ付き保存を確認
        includePv->setChecked(true);
        QTimer::singleShot(0, this, &TestTsumeshogiGeneratorGui::chooseSaveFile);
        QTest::mouseClick(button(dialog, "generatorSave"), Qt::LeftButton);
        QString firstLine;
        {
            QFile out(savePath);
            QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
            const QString text = QString::fromUtf8(out.readAll());
            QVERIFY(text.contains(QStringLiteral("逆算・編集")));
            QVERIFY(text.contains(firstSfen + QStringLiteral(" moves ")));
            for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                if (!line.startsWith(QLatin1Char('#'))) firstLine = line;
            }
        }
        QVERIFY(!firstLine.isEmpty());

        // 2. 途中保存から再開して上限2問にする → 1問目が結果一覧へ復元され、さらに2問目が採択される
        //    （再開では種局面ファイルを読まない。評価待ちに残っていた種局面は途中保存から戻る）
        limit->setValue(2);
        resumeCheck->setChecked(true);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 1)->text(), firstSfen);
        QTRY_VERIFY_WITH_TIMEOUT(!retroGen->isRunning() && table->rowCount() == 2, 60000);
        QCOMPARE(progress->value(), 2);
        QCOMPARE(progress->maximum(), 2);
        const QString secondSfen = table->item(1, 1)->text();
        QVERIFY(secondSfen != firstSfen);

        // 3. 除外ファイルに1問目を指定して新規開始すると、1問目以外の別局面が採択される
        //    （同じ種局面ファイルから始めても、除外した先頭の局面は飛ばされる）
        {
            QFile ex(excludePath);
            QVERIFY(ex.open(QIODevice::WriteOnly | QIODevice::Text));
            ex.write(firstLine.toUtf8() + '\n');
        }
        resumeCheck->setChecked(false);
        cpEdit->clear();
        excludeEdit->setText(excludePath);
        limit->setValue(1);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!retroGen->isRunning() && table->rowCount() == 1, 60000);
        QVERIFY(table->item(0, 1)->text() != firstSfen);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumeshogiGeneratorGuiTest"));
    ApplicationFonts::initialize();
    TestTsumeshogiGeneratorGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_tsumeshogi_generator_gui.moc"
