#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

#include "piecestylecatalog.h"
#include "appearancesettings.h"
#include "boardappearance.h"
#include "pieceimageprovider.h"
#include "settingscommon.h"

namespace {
/// 駒のセットを保存するキー
constexpr char kPieceStyleKey[] = "UI/pieceStyle";
}

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
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void variantSelectionAndPersistence_data()
    {
        QTest::addColumn<QString>("selectedStyle");
        for (const auto& style : PieceStyleCatalog::ids()) {
            if (style != QStringLiteral("standard")) QTest::newRow(qPrintable(style)) << style;
        }
    }

    void variantSelectionAndPersistence()
    {
        QFETCH(QString, selectedStyle);
        auto& provider = PieceImageProvider::instance();
        auto& appearance = BoardAppearance::instance();
        QSignalSpy changed(&appearance, &BoardAppearance::pieceStyleChanged);
        const auto standardPawn = provider.icon(Piece::BlackPawn).pixmap(90).toImage();
        appearance.setPieceStyle(selectedStyle);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(appearance.pieceStyle(), selectedStyle);
        QVERIFY(provider.icon(Piece::BlackPawn).pixmap(90).toImage() != standardPawn);
        SettingsCommon::openSettings().sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(kPieceStyleKey).toString(), selectedStyle);
        appearance.setPieceStyle(selectedStyle);
        QCOMPARE(changed.count(), 1);
        appearance.setPieceStyle(QStringLiteral("standard"));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(appearance.pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(provider.icon(Piece::BlackPawn).pixmap(90).toImage(), standardPawn);
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
        const BoardColors colors = AppearanceSettings::boardColors();
        SettingsCommon::openSettings().setValue(kPieceStyleKey, legacyStyle);
        auto& provider = PieceImageProvider::instance();
        auto& appearance = BoardAppearance::instance();
        QSignalSpy changed(&appearance, &BoardAppearance::pieceStyleChanged);
        QCOMPARE(appearance.pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(SettingsCommon::openSettings().value(kPieceStyleKey).toString(),
                 QStringLiteral("standard"));
        QVERIFY(AppearanceSettings::boardColors() == colors);
        const auto pawn = QIcon(":/pieces/Sente_fu45.svg").pixmap(90).toImage();
        QVERIFY(!pawn.isNull());
        QCOMPARE(provider.icon(Piece::BlackPawn).pixmap(90).toImage(), pawn);
        QCOMPARE(provider.iconForStyle(Piece::BlackPawn, legacyStyle).pixmap(90).toImage(), pawn);
        appearance.setPieceStyle(QStringLiteral("standard"));
        appearance.setPieceStyle(QStringLiteral("../unknown"));
        QCOMPARE(appearance.pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(changed.count(), 0);
        SettingsCommon::openSettings().sync();
        const QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(kPieceStyleKey).toString(), QStringLiteral("standard"));
    }

    void standardAndVariantsAreAvailable()
    {
        QCOMPARE(AppearanceSettings::pieceStyle(), QStringLiteral("standard"));
        QCOMPARE(PieceStyleCatalog::ids().size(), 40);
        QVERIFY(PieceStyleCatalog::ids().contains(QStringLiteral("sengoku")));
        QCOMPARE(PieceStyleCatalog::ids().first(), QStringLiteral("standard"));
        for (const auto& removed : {"clear", "wood", "ivory", "dark"})
            QVERIFY(!QFile::exists(QStringLiteral(":/pieces/%1/Sente_fu45.svg").arg(QLatin1String(removed))));
    }

    /// 駒のセットの表は、ID と表示名が重ならず、どの分類にも1つ以上あり、標準の駒だけが「すべて」にだけ出る
    void catalogIsConsistent()
    {
        using Family = PieceStyleCatalog::Family;
        QSet<QString> ids;
        QSet<QString> names;
        int perFamily[static_cast<int>(Family::Count)] = {};
        for (const auto& entry : PieceStyleCatalog::entries()) {
            const QString id = QString::fromLatin1(entry.id);
            QVERIFY2(!ids.contains(id), entry.id);
            ids.insert(id);
            const QString name = PieceStyleCatalog::displayName(id);
            QVERIFY2(!names.contains(name), qPrintable(name));
            names.insert(name);
            QCOMPARE(PieceStyleCatalog::family(id), entry.family);
            ++perFamily[static_cast<int>(entry.family)];
            QVERIFY(QFile::exists(PieceStyleCatalog::resourcePrefix(id) + QStringLiteral("Sente_fu45.svg")));
        }
        QCOMPARE(perFamily[static_cast<int>(Family::Standard)], 1);
        for (int family = 1; family < static_cast<int>(Family::Count); ++family) {
            QVERIFY(perFamily[family] > 0);
            QVERIFY(!PieceStyleCatalog::familyName(static_cast<Family>(family)).isEmpty());
        }
        QCOMPARE(PieceStyleCatalog::displayName(QStringLiteral("unknown")),
                 PieceStyleCatalog::displayName(PieceStyleCatalog::standardId()));
        QCOMPARE(PieceStyleCatalog::resourcePrefix(QStringLiteral("../unknown")), QStringLiteral(":/pieces/"));
    }

    void allResourcesRender()
    {
        const QStringList names = {"fu", "kyou", "kei", "gin", "kin", "kaku", "hi", "ou",
                                   "gyoku", "to", "narikyou", "narikei", "narigin", "uma", "ryuu"};
        for (const auto& style : PieceStyleCatalog::ids()) {
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
                                if (style == QStringLiteral("sengoku") || style.startsWith(QLatin1String("chess_"))
                                    || style.startsWith(QLatin1String("alphabet_"))) {
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

    void symbolsAndPromotionsRender_data()
    {
        QTest::addColumn<QString>("family");
        QTest::newRow("chess") << QStringLiteral("chess_");
        QTest::newRow("alphabet") << QStringLiteral("alphabet_");
    }

    void symbolsAndPromotionsRender()
    {
        QFETCH(QString, family);
        auto& provider = PieceImageProvider::instance();
        int count = 0;
        for (const auto& style : PieceStyleCatalog::ids()) {
            if (!style.startsWith(family)) continue;
            ++count;
            // 絵柄・欧文が読み込めず、五角形だけ表示される退行も検出する。
            static constexpr Piece plain[] = {Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight,
                                              Piece::BlackSilver, Piece::BlackBishop, Piece::BlackRook};
            for (const Piece blackBase : plain) {
                for (const bool gote : {false, true}) {
                    const Piece base = gote ? toWhite(blackBase) : blackBase;
                    const Piece promotedPiece = promote(base);
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
        QList<Piece> types;
        for (const Turn side : {Turn::Black, Turn::White})
            for (int kind = 0; kind < kPieceKindCount; ++kind) types << makePiece(side, static_cast<PieceKind>(kind));
        for (const auto& style : PieceStyleCatalog::ids()) {
            BoardAppearance::instance().setPieceStyle(style);
            const QString prefix = style == QStringLiteral("standard")
                ? QStringLiteral(":/pieces/") : QStringLiteral(":/pieces/%1/").arg(style);
            for (const Piece type : std::as_const(types)) {
                QVERIFY(!provider.icon(type).pixmap(45).isNull());
                QVERIFY(!provider.icon(type, true).pixmap(45).isNull());
                QVERIFY(provider.icon(type).pixmap(45).toImage()
                        != provider.icon(type, true).pixmap(45).toImage());
            }
            QCOMPARE(provider.icon(Piece::BlackKing, true).pixmap(90).toImage(),
                     QIcon(prefix + "Gote_ou45.svg").pixmap(90).toImage());
            QCOMPARE(provider.icon(Piece::WhiteKing, true).pixmap(90).toImage(),
                     QIcon(prefix + "Sente_gyoku45.svg").pixmap(90).toImage());
            QVERIFY(provider.icon(Piece::None).isNull());
        }
    }
};

QTEST_MAIN(TestPieceImages)
#include "tst_piece_images.moc"
