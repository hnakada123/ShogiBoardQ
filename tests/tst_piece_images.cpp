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
        QAction standard, clear;
        PieceStyleController controller(&standard, &clear);
        auto& provider = PieceImageProvider::instance();
        QSignalSpy changed(&provider, &PieceImageProvider::styleChanged);
        QVERIFY(standard.isChecked());
        QVERIFY(!clear.isChecked());

        clear.trigger();
        QVERIFY(clear.isChecked());
        QVERIFY(!standard.isChecked());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(provider.style(), QStringLiteral("clear"));
        SettingsCommon::openSettings().sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kPieceStyle).toString(), QStringLiteral("clear"));

        QAction restoredStandard, restoredClear;
        PieceStyleController restoredController(&restoredStandard, &restoredClear);
        QVERIFY(restoredClear.isChecked());
        clear.trigger();
        QCOMPARE(changed.count(), 1);
        QVERIFY(clear.isChecked());

        standard.trigger();
        QCOMPARE(changed.count(), 2);
        QVERIFY(standard.isChecked());
        QVERIFY(restoredStandard.isChecked());
        QVERIFY(!restoredClear.isChecked());
        QCOMPARE(provider.style(), QStringLiteral("standard"));
    }

    void invalidSettingFallsBack()
    {
        SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle, "unknown");
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        PieceImageProvider::instance().setStyle(QStringLiteral("clear"));
        PieceImageProvider::instance().setStyle(QStringLiteral("../unknown"));
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("clear"));
    }

    void allResourcesRender()
    {
        const QStringList names = {"fu", "kyou", "kei", "gin", "kin", "kaku", "hi", "ou",
                                   "gyoku", "to", "narikyou", "narikei", "narigin", "uma", "ryuu"};
        for (const QString& prefix : {QStringLiteral(":/pieces/"), QStringLiteral(":/pieces/clear/")}) {
            for (const QString& side : {QStringLiteral("Sente_"), QStringLiteral("Gote_")}) {
                for (const auto& name : names) {
                    const QString path = prefix + side + name + QStringLiteral("45.svg");
                    QVERIFY2(QFile::exists(path), qPrintable(path));
                    for (const int size : {20, 45, 128}) {
                        const auto image = QIcon(path).pixmap(size, size).toImage();
                        QVERIFY2(!image.isNull(), qPrintable(path));
                        QVERIFY2(image.pixelColor(size / 2, size / 2).alpha() > 0, qPrintable(path));
                        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
                    }
                }
            }
        }
    }

    void flippedKingsKeepIdentity()
    {
        auto& provider = PieceImageProvider::instance();
        const QString types = QStringLiteral("PLNSGBRKQMOTCUplnsgbrkqmotcu");
        for (const QString& style : {QStringLiteral("standard"), QStringLiteral("clear")}) {
            provider.setStyle(style);
            const QString prefix = style == QStringLiteral("clear")
                ? QStringLiteral(":/pieces/clear/") : QStringLiteral(":/pieces/");
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
