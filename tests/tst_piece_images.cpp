#include <QtTest>
#include <QAction>
#include <QSettings>
#include <QTemporaryDir>

#include "appsettings.h"
#include "pieceimageprovider.h"
#include "piecestylecontroller.h"
#include "settingscommon.h"
#include "settingskeys.h"

class TestPieceImages : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void menuSelectionAndPersistence()
    {
        // 旧セットを選んでいた環境でも、残したメニューで標準の駒を選択できる。
        SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle, "wood");
        QAction standard;
        PieceStyleController controller({{&standard, QStringLiteral("standard")}});
        auto& provider = PieceImageProvider::instance();
        QSignalSpy changed(&provider, &PieceImageProvider::styleChanged);
        QVERIFY(standard.isChecked());
        standard.trigger();
        QCOMPARE(provider.style(), QStringLiteral("standard"));
        QVERIFY(standard.isChecked());
        QCOMPARE(changed.count(), 0);
        SettingsCommon::openSettings().sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kPieceStyle).toString(), QStringLiteral("standard"));
        QAction restoredStandard;
        PieceStyleController restoredController({{&restoredStandard, QStringLiteral("standard")}});
        QVERIFY(restoredStandard.isChecked());
    }

    void variantSelectionAndPersistence_data()
    {
        QTest::addColumn<QString>("selectedStyle");
        for (const auto& style : AppSettings::availablePieceStyles()) {
            if (style != QStringLiteral("standard")) QTest::newRow(qPrintable(style)) << style;
        }
    }

    void variantSelectionAndPersistence()
    {
        QFETCH(QString, selectedStyle);
        QAction standard, selected;
        PieceStyleController controller({{&standard, QStringLiteral("standard")}, {&selected, selectedStyle}});
        auto& provider = PieceImageProvider::instance();
        QSignalSpy changed(&provider, &PieceImageProvider::styleChanged);
        const auto standardPawn = provider.icon('P').pixmap(90).toImage();
        selected.trigger();
        QVERIFY(selected.isChecked());
        QVERIFY(!standard.isChecked());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(provider.style(), selectedStyle);
        QVERIFY(provider.icon('P').pixmap(90).toImage() != standardPawn);
        SettingsCommon::openSettings().sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kPieceStyle).toString(), selectedStyle);
        QAction restoredStandard, restoredSelected;
        PieceStyleController restoredController({{&restoredStandard, QStringLiteral("standard")},
                                                 {&restoredSelected, selectedStyle}});
        QVERIFY(restoredSelected.isChecked());
        selected.trigger();
        QCOMPARE(changed.count(), 1);
        standard.trigger();
        QCOMPARE(changed.count(), 2);
        QVERIFY(standard.isChecked());
        QVERIFY(restoredStandard.isChecked());
        QVERIFY(!restoredSelected.isChecked());
        QCOMPARE(provider.icon('P').pixmap(90).toImage(), standardPawn);
    }

    void legacySettingsMigrate_data()
    {
        QTest::addColumn<QString>("legacyStyle");
        for (const auto& style : {"standard", "clear", "wood", "ivory", "dark", "unknown"})
            QTest::newRow(style) << QString::fromLatin1(style);
    }

    void legacySettingsMigrate()
    {
        QFETCH(QString, legacyStyle);
        const BoardColors colors = AppSettings::boardColors();
        SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle, legacyStyle);
        auto& provider = PieceImageProvider::instance();
        QCOMPARE(provider.style(), QStringLiteral("standard"));
        QCOMPARE(SettingsCommon::openSettings().value(SettingsKeys::kPieceStyle).toString(),
                 QStringLiteral("standard"));
        QVERIFY(AppSettings::boardColors() == colors);
        const auto pawn = QIcon(":/pieces/Sente_fu45.svg").pixmap(90).toImage();
        QVERIFY(!pawn.isNull());
        QCOMPARE(provider.icon('P').pixmap(90).toImage(), pawn);
        QCOMPARE(provider.iconForStyle('P', legacyStyle).pixmap(90).toImage(), pawn);
        provider.setStyle(QStringLiteral("../unknown"));
        QCOMPARE(provider.style(), QStringLiteral("standard"));
    }

    void standardAndVariantsAreAvailable()
    {
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(AppSettings::availablePieceStyles().size(), 21);
        QCOMPARE(AppSettings::availablePieceStyles().first(), QStringLiteral("standard"));
        for (const auto& removed : {"clear", "wood", "ivory", "dark"})
            QVERIFY(!QFile::exists(QStringLiteral(":/pieces/%1/Sente_fu45.svg").arg(QLatin1String(removed))));
    }

    void allResourcesRender()
    {
        const QStringList names = {"fu", "kyou", "kei", "gin", "kin", "kaku", "hi", "ou",
                                   "gyoku", "to", "narikyou", "narikei", "narigin", "uma", "ryuu"};
        for (const auto& style : AppSettings::availablePieceStyles()) {
            const QString prefix = style == QStringLiteral("standard")
                ? QStringLiteral(":/pieces/") : QStringLiteral(":/pieces/%1/").arg(style);
            for (const QString& side : {QStringLiteral("Sente_"), QStringLiteral("Gote_")}) {
                for (const auto& name : names) {
                    const QString path = prefix + side + name + QStringLiteral("45.svg");
                    QVERIFY2(QFile::exists(path), qPrintable(path));
                    for (const int size : {20, 45, 128}) {
                        const auto image = QIcon(path).pixmap(size, size).toImage();
                        QVERIFY2(!image.isNull(), qPrintable(path));
                        QVERIFY2(image.pixelColor(size / 2, size / 2).alpha() > 0, qPrintable(path));
                        const auto silhouette = QIcon(QStringLiteral(":/pieces/") + side + name
                                                      + QStringLiteral("45.svg")).pixmap(size, size).toImage();
                        for (int y = 0; y < size; ++y) {
                            for (int x = 0; x < size; ++x) {
                                if (silhouette.pixelColor(x, y).alpha() == 0) {
                                    QVERIFY2(image.pixelColor(x, y).alpha() == 0,
                                             qPrintable(QStringLiteral("%1 size=%2 outside (%3,%4)")
                                                            .arg(path).arg(size).arg(x).arg(y)));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void flippedKingsKeepIdentity()
    {
        auto& provider = PieceImageProvider::instance();
        const QString types = QStringLiteral("PLNSGBRKQMOTCUplnsgbrkqmotcu");
        for (const auto& style : AppSettings::availablePieceStyles()) {
            provider.setStyle(style);
            const QString prefix = style == QStringLiteral("standard")
                ? QStringLiteral(":/pieces/") : QStringLiteral(":/pieces/%1/").arg(style);
            for (const QChar type : types) {
                QVERIFY(!provider.icon(type).pixmap(45).isNull());
                QVERIFY(!provider.icon(type, true).pixmap(45).isNull());
                QVERIFY(provider.icon(type).pixmap(45).toImage()
                        != provider.icon(type, true).pixmap(45).toImage());
            }
            QCOMPARE(provider.icon('K', true).pixmap(90).toImage(),
                     QIcon(prefix + "Gote_ou45.svg").pixmap(90).toImage());
            QCOMPARE(provider.icon('k', true).pixmap(90).toImage(),
                     QIcon(prefix + "Sente_gyoku45.svg").pixmap(90).toImage());
            QVERIFY(provider.icon(' ').isNull());
        }
    }
};

QTEST_MAIN(TestPieceImages)
#include "tst_piece_images.moc"
