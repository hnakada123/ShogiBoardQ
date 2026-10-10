#include <QtTest>
#include <QApplication>
#include <QAbstractButton>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTranslator>
#include <QTimer>
#include <QTableWidget>
#include <QLibraryInfo>
#include <memory>

#include "applicationfonts.h"
#include "appsettings.h"
#include "settingscommon.h"
#include "dialogfontscale.h"
#include "boardcolordialog.h"
#include "changeenginesettingsdialog.h"
#include "csagamedialog.h"
#include "csawaitingdialog.h"
#include "engineregistrationdialog.h"
#include "fontsettingsdialog.h"
#include "jishogiscoredialog.h"
#include "josekimergedialog.h"
#include "josekimovedialog.h"
#include "kifuanalysisdialog.h"
#include "kifupastedialog.h"
#include "piecesoundsettingsdialog.h"
#include "promotedialog.h"
#include "pvboarddialog.h"
#include "sfencollectiondialog.h"
#include "sfenutils.h"
#include "shogiview.h"
#include "startgamedialog.h"
#include "tsumecollectiondialog.h"
#include "tsumeplaydialog.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumeshogisearchdialog.h"
#include "versiondialog.h"
#include "evaluationchartconfigurator.h"
#include "longlongspinbox.h"

class TestDialogAppearance : public QObject
{
    Q_OBJECT

    std::unique_ptr<QDialog> create(const QString& name)
    {
        if (name == QLatin1String("boardColors")) return std::make_unique<BoardColorDialog>();
        if (name == QLatin1String("engineOptions")) {
            auto& settings = SettingsCommon::openSettings();
            settings.beginWriteArray(QStringLiteral("Test engine"), 2);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("name"), QStringLiteral("Threads"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("spin"));
            settings.setValue(QStringLiteral("default"), QStringLiteral("1"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("4"));
            settings.setValue(QStringLiteral("min"), QStringLiteral("1"));
            settings.setValue(QStringLiteral("max"), QStringLiteral("1024"));
            settings.setArrayIndex(1);
            settings.setValue(QStringLiteral("name"), QStringLiteral("BookFile"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("combo"));
            settings.setValue(QStringLiteral("default"), QStringLiteral("standard_book.db"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("standard_book.db"));
            settings.setValue(QStringLiteral("valueList"), QStringLiteral("[\"standard_book.db\",\"long_book_filename_for_selection.db\"]"));
            settings.endArray();
            settings.sync();
            auto dialog = std::make_unique<ChangeEngineSettingsDialog>();
            dialog->setEngineName(QStringLiteral("Test engine"));
            dialog->setupEngineOptionsDialog();
            return dialog;
        }
        if (name == QLatin1String("csaGame")) return std::make_unique<CsaGameDialog>();
        if (name == QLatin1String("csaWaiting")) return std::make_unique<CsaWaitingDialog>(nullptr);
        if (name == QLatin1String("engines")) return std::make_unique<EngineRegistrationDialog>();
        if (name == QLatin1String("fontSettings")) return std::make_unique<FontSettingsDialog>();
        if (name == QLatin1String("jishogi")) return std::make_unique<JishogiScoreDialog>(JishogiCalculator::JishogiResult{}, false, false);
        if (name == QLatin1String("josekiMerge")) return std::make_unique<JosekiMergeDialog>();
        if (name == QLatin1String("josekiMove")) return std::make_unique<JosekiMoveDialog>();
        if (name == QLatin1String("analysis")) return std::make_unique<KifuAnalysisDialog>();
        if (name == QLatin1String("paste")) return std::make_unique<KifuPasteDialog>();
        if (name == QLatin1String("sound")) return std::make_unique<PieceSoundSettingsDialog>(nullptr);
        if (name == QLatin1String("promotion")) return std::make_unique<PromoteDialog>();
        if (name == QLatin1String("pv")) return std::make_unique<PvBoardDialog>(SfenUtils::hirateSfen(), QStringList{QStringLiteral("7g7f")});
        if (name == QLatin1String("sfen")) return std::make_unique<SfenCollectionDialog>();
        if (name == QLatin1String("startGame")) return std::make_unique<StartGameDialog>();
        if (name == QLatin1String("tsumeCollection")) return std::make_unique<TsumeCollectionDialog>();
        if (name == QLatin1String("tsumePlay")) return std::make_unique<TsumePlayDialog>();
        if (name == QLatin1String("generator")) return std::make_unique<TsumeshogiGeneratorDialog>();
        if (name == QLatin1String("tsumeSearch")) return std::make_unique<TsumeShogiSearchDialog>();
        if (name == QLatin1String("version")) return std::make_unique<VersionDialog>();
        if (name == QLatin1String("messageBox")) {
            return std::make_unique<QMessageBox>(QMessageBox::Warning, QStringLiteral("時間設定"),
                QStringLiteral("エンジンが参加する対局は、時間無制限（持ち時間・秒読み・加算がすべて0秒）にできません。\n"
                               "エンジンは使える時間を0秒と受け取り、ほとんど考えずに指してしまいます。"
                               "持ち時間・秒読み・加算のいずれかを設定してください。"));
        }
        if (name == QLatin1String("messageBoxChoices")) {
            return std::make_unique<QMessageBox>(QMessageBox::Warning, QStringLiteral("未保存の棋譜"),
                QStringLiteral("棋譜が保存されていません。保存しますか？"),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        }
        auto dialog = std::make_unique<QInputDialog>();
        dialog->setLabelText(QStringLiteral("しおり名:"));
        dialog->setTextValue(QStringLiteral("重要な局面"));
        dialog->ensurePolished();
        DialogFontScale::install(dialog.get(), QStringLiteral("input"), true);
        return dialog;
    }

    QList<QAbstractButton*> fontButtons(QDialog* dialog)
    {
        QAbstractButton* down = nullptr;
        QAbstractButton* up = nullptr;
        for (auto* button : dialog->findChildren<QAbstractButton*>()) {
            if (button->window() != dialog) continue;
            if (button->text() == QLatin1String("A-") || button->text() == QStringLiteral("A−")) down = button;
            if (button->text() == QLatin1String("A+")) up = button;
        }
        return {down, up};
    }

    void snapshot(QDialog* dialog, const QString& name)
    {
        const QString directory = qEnvironmentVariable("SHOGIBOARDQ_DIALOG_SCREENSHOTS");
        if (directory.isEmpty()) return;
        QDir().mkpath(directory);
        QVERIFY(dialog->grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")));
    }

private slots:
    void init()
    {
        QVERIFY(SettingsCommon::resetAllSettings());
        ApplicationFonts::initialize();
    }

    void dialogs_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("english");
        for (const auto* name : {"boardColors", "engineOptions", "csaGame", "csaWaiting", "engines",
             "fontSettings", "jishogi", "josekiMerge", "josekiMove", "analysis", "paste", "sound", "promotion",
             "pv", "sfen", "startGame", "tsumeCollection", "tsumePlay", "generator", "tsumeSearch", "version", "input",
             "messageBox", "messageBoxChoices"}) {
            for (bool english : {false, true})
                QTest::newRow(qPrintable(QString::fromLatin1(name) + (english ? "-en" : "-ja"))) << QString::fromLatin1(name) << english;
        }
    }

    void dialogs()
    {
        QFETCH(QString, name);
        QFETCH(bool, english);
        QTranslator translator;
        QTranslator qtTranslator;
        if (qtTranslator.load(english ? QStringLiteral("qtbase_en") : QStringLiteral("qtbase_ja"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            qApp->installTranslator(&qtTranslator);
        if (english) {
            QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_en.qm")));
            qApp->installTranslator(&translator);
        }
        ApplicationFonts::initialize({}, english ? QStringLiteral("en") : QStringLiteral("ja_JP"));
        auto dialog = create(name);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog.get()));
        QTest::qWait(30);
        const auto buttons = fontButtons(dialog.get());
        QVERIFY2(buttons[0] && buttons[1], qPrintable(name + " has no font controls"));
        snapshot(dialog.get(), QString::fromLatin1(QTest::currentDataTag()));
        const int initial = dialog->font().pointSize();
        for (int i = 0; i < 32 && buttons[1]->isEnabled(); ++i) {
            QTest::mouseClick(buttons[1], Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        QVERIFY2(!buttons[1]->isEnabled(), "Increase must be disabled at the upper limit");
        QTest::qWait(30);
        QVERIFY(dialog->isVisible());
        for (auto* button : buttons) {
            QVERIFY(button->isVisible());
            QVERIFY(!button->accessibleName().isEmpty());
            QVERIFY(button->focusPolicy() != Qt::NoFocus);
            QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 12);
            QVERIFY(dialog->rect().contains(QRect(button->mapTo(dialog.get(), QPoint()), button->size())));
            QVERIFY2(button->visibleRegion().boundingRect().contains(button->rect()), "Font controls must remain fully visible");
        }
        snapshot(dialog.get(), QString::fromLatin1(QTest::currentDataTag()) + "-large");
        for (int i = 0; i < 32 && buttons[0]->isEnabled(); ++i) {
            QTest::mouseClick(buttons[0], Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        QVERIFY(!buttons[0]->isEnabled());
        if (name != QLatin1String("jishogi")) QVERIFY(dialog->font().pointSize() <= initial);
        dialog->reject();
    }

    void licenseDocuments_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("noticeText");
        QTest::addColumn<QString>("sourceTitle");
        QTest::newRow("ja_JP") << QStringLiteral("ja_JP") << QStringLiteral("ShogiBoardQ は Qt を使用しています。")
                             << QStringLiteral("ソースコードの入手");
        QTest::newRow("en") << QStringLiteral("en") << QStringLiteral("ShogiBoardQ uses Qt.")
                          << QStringLiteral("Obtaining source code");
        QTest::newRow("zh_CN") << QStringLiteral("zh_CN") << QStringLiteral("ShogiBoardQ 使用 Qt。")
                             << QStringLiteral("获取源代码");
        QTest::newRow("zh_TW") << QStringLiteral("zh_TW") << QStringLiteral("ShogiBoardQ 使用 Qt。")
                             << QStringLiteral("取得原始碼");
    }

    void licenseDocuments()
    {
        QFETCH(QString, language);
        QFETCH(QString, noticeText);
        QFETCH(QString, sourceTitle);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        VersionDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* documents = dialog.findChild<QComboBox*>(QStringLiteral("licenseDocuments"));
        auto* browser = dialog.findChild<QTextBrowser*>(QStringLiteral("licenseBrowser"));
        QVERIFY(documents && browser);
        QVERIFY(browser->toPlainText().contains(noticeText));
        const QRegularExpression japanese(QStringLiteral("[\\p{Hiragana}\\p{Katakana}\\p{Han}]"));
        if (language == QLatin1String("en")) QVERIFY(!japanese.match(browser->toPlainText()).hasMatch());
        snapshot(&dialog, QStringLiteral("license-notice-") + language);

        // 本文中のリンクも同じ言語の案内文書を開く。
        const QRegularExpression sourceLink(QStringLiteral("qrc:/licenses/SOURCE_CODE[^\"]*\\.md"));
        const auto link = sourceLink.match(browser->toHtml());
        QVERIFY(link.hasMatch());
        browser->setSource(QUrl(link.captured()));
        QVERIFY(browser->toPlainText().startsWith(sourceTitle));
        if (language == QLatin1String("en")) QVERIFY(!japanese.match(browser->toPlainText()).hasMatch());
        documents->setCurrentIndex(3);
        QVERIFY(browser->toPlainText().startsWith(sourceTitle));
        if (language == QLatin1String("en")) QVERIFY(!japanese.match(browser->toPlainText()).hasMatch());
        snapshot(&dialog, QStringLiteral("license-source-") + language);

        // ライセンス本文は翻訳・短縮せず、収録した原文を全文表示する。
        for (int index : {1, 2}) {
            documents->setCurrentIndex(index);
            QFile original(index == 1 ? QStringLiteral(":/licenses/GPL-3.0.txt")
                                      : QStringLiteral(":/licenses/LGPL-3.0.txt"));
            QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(browser->toPlainText().trimmed(), QString::fromUtf8(original.readAll()).trimmed());
        }
    }

    void bundledLibraryNotices()
    {
        // 配布版の licenses に同梱ライブラリの一覧があるときだけ選択肢に出し、その文書を表示する。
        const QDir appDir(QCoreApplication::applicationDirPath());
        if (appDir.exists(QStringLiteral("licenses"))) QSKIP("licenses already exists beside the test executable");
        QVERIFY(appDir.mkdir(QStringLiteral("licenses")));
        const auto cleanup = qScopeGuard([&appDir] {
            QDir(appDir.filePath(QStringLiteral("licenses"))).removeRecursively();
        });
        // 拡張子のない本文にも、改行や <…> をそのまま含める。
        const QByteArray license = "Copyright <authors@example.org>\n\n  Indented line\n";
        QVERIFY(appDir.mkpath(QStringLiteral("licenses/third-party/openssl")));
        for (const auto& [name, text] : {std::pair{"NOTICE.md", QByteArray("# Notice\n")},
                                         std::pair{"THIRD-PARTY-NOTICES.md", QByteArray(
                                             "# Bundled library licenses\n\n## openssl 3.6.0-1\n\n"
                                             "- License texts: [third-party/openssl/LICENSE](third-party/openssl/LICENSE)\n")},
                                         std::pair{"third-party/openssl/LICENSE", license}}) {
            QFile file(appDir.filePath(QStringLiteral("licenses/") + QLatin1String(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(text);
        }
        VersionDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* documents = dialog.findChild<QComboBox*>(QStringLiteral("licenseDocuments"));
        auto* browser = dialog.findChild<QTextBrowser*>(QStringLiteral("licenseBrowser"));
        QVERIFY(documents && browser);
        // Qt の一覧がない場合も、選択肢と表示する文書が対応する。
        QCOMPARE(documents->count(), 5);
        QCOMPARE(documents->itemText(4), QStringLiteral("同梱ライブラリのライセンス一覧"));
        documents->setCurrentIndex(4);
        QVERIFY(browser->toPlainText().contains(QStringLiteral("openssl 3.6.0-1")));

        // 一覧のリンク（licenses 内の相対パス）から本文を開ける。起動時のフォルダには依存しない。
        browser->setFocus();
        QTest::keyClick(browser, Qt::Key_Tab);
        QTest::keyClick(browser, Qt::Key_Return);
        QCOMPARE(browser->toPlainText(), QString::fromUtf8(license));
        // 同じ項目を選び直すと一覧に戻る。
        documents->activated(4);
        QVERIFY(browser->toPlainText().contains(QStringLiteral("openssl 3.6.0-1")));
        documents->setCurrentIndex(0);
        QVERIFY(browser->toPlainText().startsWith(QStringLiteral("Notice")));
    }

    void soundNumericInput()
    {
        PieceSoundSettingsDialog dialog(nullptr);
        for (const auto* name : {"pieceSoundVolume", "pieceSoundPitch", "pieceSoundLow", "pieceSoundMid", "pieceSoundHigh"}) {
            auto* slider = dialog.findChild<QSlider*>(QString::fromLatin1(name));
            auto* input = dialog.findChild<QSpinBox*>(QString::fromLatin1(name) + "Value");
            QVERIFY(slider && input);
            input->setValue(input->maximum());
            QCOMPARE(slider->value(), input->maximum());
            slider->setValue(slider->minimum());
            QCOMPARE(input->value(), slider->minimum());
        }
    }

    /// 音の高さが ±1 半音のときは英語の単位を単数形（1 semitone）にする
    void soundPitchSingularUnit()
    {
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_en.qm")));
        qApp->installTranslator(&translator);
        PieceSoundSettingsDialog dialog(nullptr);
        auto* slider = dialog.findChild<QSlider*>(QStringLiteral("pieceSoundPitch"));
        auto* input = dialog.findChild<QSpinBox*>(QStringLiteral("pieceSoundPitchValue"));
        QVERIFY(slider && input);
        const QList<QPair<int, QString>> cases{
            {0, QStringLiteral("0 semitones")}, {1, QStringLiteral("1 semitone")},
            {-1, QStringLiteral("-1 semitone")}, {2, QStringLiteral("2 semitones")}};
        for (const auto& [value, text] : cases) {
            slider->setValue(value);
            QCOMPARE(input->text(), text);
        }
    }

    void footerReflowsWithoutHidingActions()
    {
        PieceSoundSettingsDialog dialog(nullptr);
        dialog.resize(1000, 500);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* box = dialog.findChild<QDialogButtonBox*>();
        auto* scale = dialog.findChild<QWidget*>(QStringLiteral("dialogFontScale"));
        QVERIFY(box && scale);
        QTRY_VERIFY(qAbs(box->geometry().center().y() - scale->geometry().center().y()) <= 2);
        for (int i = 0; i < 14; ++i) QTest::mouseClick(fontButtons(&dialog)[1], Qt::LeftButton);
        dialog.resize(qMax(box->minimumSizeHint().width(), scale->minimumSizeHint().width()) + 32, 750);
        QTRY_VERIFY(box->geometry().top() >= scale->geometry().bottom());
        QTest::qWait(50);
        for (auto* button : box->buttons()) {
            QVERIFY(button->visibleRegion().boundingRect().contains(button->rect()));
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
        }
        dialog.resize(1600, 800);
        QTRY_VERIFY(qAbs(box->geometry().center().y() - scale->geometry().center().y()) <= 2);
        dialog.reject();
    }

    /// 推奨の高さより低く縮めても、最小の高さ以上なら縮めた高さを保つ（小さい画面でも決定ボタンまで表示できる）
    void dialogKeepsHeightBelowPreferred()
    {
        BoardColorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(dialog.sizeHint().height() > 720);
        dialog.resize(1120, 700);
        QTest::qWait(100);
        QCOMPARE(dialog.height(), 700);
        dialog.reject();
    }

    void boardZoomResizesWindow_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("pv") << QStringLiteral("pv");
        QTest::newRow("sfen") << QStringLiteral("sfen");
    }

    /// 盤の拡大・縮小でウィンドウも盤に合わせ、スクロール領域の中で盤が切れない
    void boardZoomResizesWindow()
    {
        QFETCH(QString, name);
        auto dialog = create(name);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog.get()));
        QTest::qWait(50);
        QAbstractButton* reduce = nullptr;
        QAbstractButton* enlarge = nullptr;
        for (auto* button : dialog->findChildren<QAbstractButton*>()) {
            if (button->toolTip() == QStringLiteral("将棋盤を縮小する")) reduce = button;
            if (button->toolTip() == QStringLiteral("将棋盤を拡大する")) enlarge = button;
        }
        QVERIFY(reduce && enlarge);
        auto* scroll = dialog->findChild<QScrollArea*>(QStringLiteral("boardScrollArea"));
        auto* view = dialog->findChild<ShogiView*>();
        QVERIFY(scroll && view);

        // offscreen の小さい画面（800x800）でも盤全体が入る大きさまで縮めてから拡大する
        for (int i = 0; i < 40; ++i) reduce->click();
        const QSize reduced = dialog->size();
        for (int i = 0; i < 10; ++i) enlarge->click();
        QCOMPARE(view->size(), view->sizeHint());
        QVERIFY(dialog->width() > reduced.width() && dialog->height() > reduced.height());
        // スクロールバーはイベント処理の後に消える
        QTRY_VERIFY(!scroll->horizontalScrollBar()->isVisible() && !scroll->verticalScrollBar()->isVisible());
        dialog->reject();
    }

    /// 表示時に文字サイズ操作を足す QInputDialog でも決定ボタンが隠れない（以前の版で保存した小さいサイズを含む）
    void inputDialogKeepsActionsVisible()
    {
        AppSettings::setAuxiliaryDialogSize(QStringLiteral("inputDialogTest"), QSize(250, 130));
        QInputDialog dialog;
        dialog.setLabelText(QStringLiteral("レイアウト名:"));
        dialog.ensurePolished();
        DialogFontScale::install(&dialog, QStringLiteral("inputDialogTest"), true);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("dialogFontScale")));
        auto* box = dialog.findChild<QDialogButtonBox*>();
        QVERIFY(box);
        QTest::qWait(50);
        for (auto* button : box->buttons()) {
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
            QVERIFY(button->width() >= button->sizeHint().width());
        }
        dialog.reject();
    }

    void emptyStatesAndMergeSelection()
    {
        EngineRegistrationDialog registration;
        auto* list = registration.findChild<QListWidget*>();
        auto* hint = registration.findChild<QLabel*>(QStringLiteral("engineHintLabel"));
        QVERIFY(list && hint);
        QVERIFY(hint->text().contains(QStringLiteral("追加")));
        list->addItem(QStringLiteral("Test engine"));
        QVERIFY(hint->text().contains(QStringLiteral("ダブルクリック")));
        list->clear();
        QVERIFY(hint->text().contains(QStringLiteral("追加")));

        JosekiMergeDialog merge;
        auto* all = merge.findChild<QPushButton*>(QStringLiteral("registerAllMoves"));
        auto* table = merge.findChild<QTableWidget*>();
        QVERIFY(all && table);
        QVERIFY(!all->isEnabled());
        const QString sfen = SfenUtils::hirateSfen();
        merge.setKifuData({{1, sfen, QStringLiteral("7g7f"), QStringLiteral("▲７六歩"), true}});
        QVERIFY(all->isEnabled());
        merge.setRegisteredMoves({sfen.section(QLatin1Char(' '), 0, 2) + QStringLiteral(":7g7f")});
        QVERIFY(!all->isEnabled());
        QVERIFY(!table->cellWidget(0, 2)->isEnabled());
        merge.setRegisteredMoves({});
        QVERIFY(all->isEnabled());
        merge.show();
        QVERIFY(QTest::qWaitForWindowExposed(&merge));
        for (int i = 0; i < 12; ++i) QTest::mouseClick(fontButtons(&merge)[1], Qt::LeftButton);
        QTest::qWait(30);
        QVERIFY(table->rowHeight(0) >= table->cellWidget(0, 2)->sizeHint().height());
        snapshot(&merge, QStringLiteral("joseki-merge-populated-large"));
        merge.reject();

        SfenCollectionDialog collection;
        auto* file = collection.findChild<QLabel*>(QStringLiteral("collectionFileLabel"));
        QVERIFY(file);
        QVERIFY(file->text().contains(QStringLiteral("ファイルを開く")));
    }

    void engineSelectionAndKeyboard()
    {
        EngineRegistrationDialog registration;
        auto* list = registration.findChild<QListWidget*>();
        auto* configure = registration.findChild<QPushButton*>(QStringLiteral("configureEngineButton"));
        auto* remove = registration.findChild<QPushButton*>(QStringLiteral("removeEngineButton"));
        QVERIFY(list && configure && remove);
        QVERIFY(!configure->isEnabled());
        QVERIFY(!remove->isEnabled());
        list->addItem(QStringLiteral("Test engine"));
        list->setCurrentRow(0);
        QVERIFY(configure->isEnabled());
        QVERIFY(remove->isEnabled());
        list->clearSelection();
        QVERIFY(!configure->isEnabled());

        auto options = create(QStringLiteral("engineOptions"));
        auto* threads = options->findChild<LongLongSpinBox*>(QStringLiteral("Threads"));
        auto* book = options->findChild<QComboBox*>(QStringLiteral("BookFile"));
        QVERIFY(threads && book);
        QVERIFY(threads->focusPolicy() & Qt::TabFocus);
        QCOMPARE(threads->value(), 4);
        QCOMPARE(book->currentText(), QStringLiteral("standard_book.db"));
        QVERIFY(book->maximumWidth() > 200);
    }

    void savedFontAndSize()
    {
        {
            PromoteDialog dialog;
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            QTest::mouseClick(fontButtons(&dialog)[1], Qt::LeftButton);
            dialog.resize(540, 280);
            dialog.reject();
            QCOMPARE(AppSettings::dialogFontSize(QStringLiteral("promotion"), 10), dialog.font().pointSize());
            QCOMPARE(AppSettings::auxiliaryDialogSize(QStringLiteral("promotion")), dialog.size());
        }
        PromoteDialog reopened;
        QCOMPARE(reopened.size(), QSize(540, 280));
        QCOMPARE(reopened.font().pointSize(), AppSettings::dialogFontSize(QStringLiteral("promotion"), 10));
    }

    /// 静的関数で出すメッセージボックスにも文字サイズ操作が付き、決定ボタンと同じ段に並ぶ
    void messageBoxFontControls()
    {
        bool inspected = false;
        int enlarged = 0;
        QTimer::singleShot(0, this, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            const auto close = qScopeGuard([box] { box->reject(); });
            auto* scale = box->findChild<QWidget*>(QStringLiteral("dialogFontScale"));
            auto* ok = box->button(QMessageBox::Ok);
            const auto buttons = fontButtons(box);
            QVERIFY(scale && ok && buttons[0] && buttons[1]);
            QTRY_VERIFY(qAbs(scale->mapTo(box, scale->rect().center()).y() - ok->mapTo(box, ok->rect().center()).y()) <= 2);
            // 既定のボタンの強調（アプリ共通のスタイル）とフォーカスは残す（Enter で決定できる）
            QVERIFY(ok->styleSheet().isEmpty());
            QCOMPARE(box->focusWidget(), ok);
            auto* label = box->findChild<QLabel*>(QStringLiteral("qt_msgbox_label"));
            QVERIFY(label);
            const int initial = label->font().pointSize();
            const QSize initialSize = box->size();
            QTest::mouseClick(buttons[1], Qt::LeftButton);
            QCOMPARE(label->font().pointSize(), initial + 1);
            QTRY_VERIFY(box->width() > initialSize.width() || box->height() > initialSize.height());
            enlarged = label->font().pointSize();
            inspected = true;
        });
        QMessageBox::warning(nullptr, QStringLiteral("時間設定"), QStringLiteral("人間が選択されています。"));
        QVERIFY(inspected);
        QCOMPARE(AppSettings::dialogFontSize(QStringLiteral("messageBox"), 0), enlarged);

        // 文字サイズはメッセージボックス全体で共有する
        QMessageBox next(QMessageBox::Information, QStringLiteral("情報"), QStringLiteral("登録する指し手がありません。"));
        QCOMPARE(DialogFontScale::messageBoxFont(next.font()).pointSize(), enlarged);
        next.show();
        QVERIFY(QTest::qWaitForWindowExposed(&next));
        QCOMPARE(next.findChild<QLabel*>(QStringLiteral("qt_msgbox_label"))->font().pointSize(), enlarged);
        next.reject();
    }

    /// QMessageBox がレイアウトを作り直しても、文字サイズ操作を決定ボタンの横に入れ直す
    void messageBoxKeepsFontControlsAfterRelayout()
    {
        QMessageBox box(QMessageBox::Warning, QStringLiteral("履歴の初期化"), QStringLiteral("初期化しますか？"),
                        QMessageBox::Yes | QMessageBox::Cancel);
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box));
        auto* scale = box.findChild<QWidget*>(QStringLiteral("dialogFontScale"));
        auto* actions = box.findChild<QDialogButtonBox*>();
        QVERIFY(scale && actions);
        box.setIcon(QMessageBox::Question);
        box.setInformativeText(QStringLiteral("この操作は取り消せません。"));
        QTRY_VERIFY(qAbs(scale->geometry().center().y() - actions->geometry().center().y()) <= 2);
        QTest::qWait(30);
        QVERIFY(box.rect().contains(scale->geometry()));
        QVERIFY(scale->geometry().right() < actions->geometry().left());
        for (auto* button : actions->buttons())
            QVERIFY(box.rect().contains(QRect(button->mapTo(&box, QPoint()), button->size())));
        box.reject();
    }

    void colorPickerAndCommunicationLog()
    {
        BoardColorDialog appearance;
        auto* picker = appearance.findChild<QColorDialog*>();
        QVERIFY(picker);
        picker->show();
        QVERIFY(QTest::qWaitForWindowExposed(picker));
        const auto colorButtons = fontButtons(picker);
        QVERIFY(colorButtons[0] && colorButtons[1]);
        const int colorSize = picker->font().pointSize();
        QTest::mouseClick(colorButtons[1], Qt::LeftButton);
        QCOMPARE(picker->font().pointSize(), colorSize + 1);
        snapshot(picker, QStringLiteral("color-picker"));
        picker->reject();

        CsaWaitingDialog waiting(nullptr);
        waiting.show();
        auto* log = waiting.findChild<QDialog*>(QStringLiteral("csaWaitingLogWindow"));
        QVERIFY(log);
        log->show();
        QVERIFY(QTest::qWaitForWindowExposed(log));
        const auto logButtons = fontButtons(log);
        QVERIFY(logButtons[0] && logButtons[1]);
        QTest::mouseClick(logButtons[1], Qt::LeftButton);
        const int logSize = logButtons[1]->font().pointSize();
        QTest::mouseClick(fontButtons(&waiting)[1], Qt::LeftButton);
        QCOMPARE(logButtons[1]->font().pointSize(), logSize);
        snapshot(log, QStringLiteral("csa-log"));
        log->reject();
        waiting.reject();
    }

    void evaluationSettings()
    {
        EvaluationChartConfigurator configurator;
        QTimer::singleShot(0, this, &TestDialogAppearance::checkEvaluationSettings);
        QVERIFY(QMetaObject::invokeMethod(&configurator, "showSettings", Qt::DirectConnection));
    }

private:
    void checkEvaluationSettings()
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        // 検査に失敗しても、モーダルループが残らないようにする。
        QTimer::singleShot(1000, dialog, &QDialog::reject);
        const auto buttons = fontButtons(dialog);
        QVERIFY(buttons[0] && buttons[1]);
        const int initial = dialog->font().pointSize();
        QTest::mouseClick(buttons[1], Qt::LeftButton);
        QCOMPARE(dialog->font().pointSize(), initial + 1);
        snapshot(dialog, QStringLiteral("evaluation-settings"));
        dialog->reject();
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("SHOGIBOARDQ_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    qputenv("SHOGIBOARDQ_DATA_HOME", data.path().toUtf8());
    qputenv("SHOGIBOARDQ_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DialogAppearanceTest"));
    ApplicationFonts::initialize();
    DialogFontScale::installForMessageBoxes(&app);
    TestDialogAppearance test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dialog_appearance.moc"
