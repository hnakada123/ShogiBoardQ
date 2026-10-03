#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

#include "appsettings.h"
#include "pieceimageprovider.h"
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
        auto& provider = PieceImageProvider::instance();
        QSignalSpy changed(&provider, &PieceImageProvider::styleChanged);
        const auto standardPawn = provider.icon('P').pixmap(90).toImage();
        provider.setStyle(selectedStyle);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(provider.style(), selectedStyle);
        QVERIFY(provider.icon('P').pixmap(90).toImage() != standardPawn);
        SettingsCommon::openSettings().sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kPieceStyle).toString(), selectedStyle);
        provider.setStyle(selectedStyle);
        QCOMPARE(changed.count(), 1);
        provider.setStyle(QStringLiteral("standard"));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(provider.style(), QStringLiteral("standard"));
        QCOMPARE(provider.icon('P').pixmap(90).toImage(), standardPawn);
    }

    void legacySettingsMigrate_data()
    {
        QTest::addColumn<QString>("legacyStyle");
        for (const auto& style : {"standard", "wood_straight", "clear", "wood", "ivory", "dark", "unknown"})
            QTest::newRow(style) << QString::fromLatin1(style);
    }

    void legacySettingsMigrate()
    {
        QFETCH(QString, legacyStyle);
        const BoardColors colors = AppSettings::boardColors();
        SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle, legacyStyle);
        auto& provider = PieceImageProvider::instance();
        QSignalSpy changed(&provider, &PieceImageProvider::styleChanged);
        QCOMPARE(provider.style(), QStringLiteral("standard"));
        QCOMPARE(SettingsCommon::openSettings().value(SettingsKeys::kPieceStyle).toString(),
                 QStringLiteral("standard"));
        QVERIFY(AppSettings::boardColors() == colors);
        const auto pawn = QIcon(":/pieces/Sente_fu45.svg").pixmap(90).toImage();
        QVERIFY(!pawn.isNull());
        QCOMPARE(provider.icon('P').pixmap(90).toImage(), pawn);
        QCOMPARE(provider.iconForStyle('P', legacyStyle).pixmap(90).toImage(), pawn);
        provider.setStyle(QStringLiteral("standard"));
        provider.setStyle(QStringLiteral("../unknown"));
        QCOMPARE(provider.style(), QStringLiteral("standard"));
        QCOMPARE(changed.count(), 0);
        SettingsCommon::openSettings().sync();
        const QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kPieceStyle).toString(), QStringLiteral("standard"));
    }

    void standardAndVariantsAreAvailable()
    {
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(AppSettings::availablePieceStyles().size(), 31);
        QVERIFY(AppSettings::availablePieceStyles().contains(QStringLiteral("sengoku")));
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
                                if (style == QStringLiteral("sengoku") || style.startsWith(QLatin1String("chess_"))) {
                                    // 境界の合成による1段階の丸め差は許容し、透過領域は完全一致させる。
                                    const int alpha = image.pixelColor(x, y).alpha();
                                    const int standardAlpha = silhouette.pixelColor(x, y).alpha();
                                    QCOMPARE(alpha == 0, standardAlpha == 0);
                                    QVERIFY(qAbs(alpha - standardAlpha) <= 1);
                                }
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

    void chessSymbolsAndPromotionsRender()
    {
        auto& provider = PieceImageProvider::instance();
        int count = 0;
        for (const auto& style : AppSettings::availablePieceStyles()) {
            if (!style.startsWith(QLatin1String("chess_"))) continue;
            ++count;
            // SVG内のPNGが読めず、五角形だけ表示される退行も検出する。
            const QString plain = QStringLiteral("PLNSBR");
            const QString promoted = QStringLiteral("QMOTCU");
            for (int i = 0; i < plain.size(); ++i) {
                for (const bool gote : {false, true}) {
                    const QChar base = gote ? plain.at(i).toLower() : plain.at(i);
                    const QChar promotedPiece = gote ? promoted.at(i).toLower() : promoted.at(i);
                    const auto normal = provider.iconForStyle(base, style).pixmap(90).toImage();
                    const auto red = provider.iconForStyle(promotedPiece, style).pixmap(90).toImage();
                    QVERIFY(normal != red);
                    int inkPixels = 0;
                    int redPixels = 0;
                    for (int y = 0; y < 90; ++y) {
                        for (int x = 0; x < 90; ++x) {
                            const auto a = normal.pixelColor(x, y);
                            const auto b = red.pixelColor(x, y);
                            if (a.alpha() > 200 && a.lightness() < 160) ++inkPixels;
                            if (b.alpha() > 200 && b.red() > b.green() * 1.5
                                && b.red() > b.blue() * 1.5) ++redPixels;
                        }
                    }
                    QVERIFY2(inkPixels > 30, qPrintable(style));
                    QVERIFY2(redPixels > 80, qPrintable(style));
                }
            }
        }
        QCOMPARE(count, 9);
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
