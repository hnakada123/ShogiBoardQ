/// @file tst_engineselector.cpp
/// @brief 登録エンジンを選ぶ部品（EngineSelector）のテスト

#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "engineprofilestore.h"
#include "engineselector.h"
#include "settingscommon.h"
#include "settingskeys.h"

namespace {
bool isMateEngine(const EngineEntry& engine)
{
    return engine.name.contains(QStringLiteral("Mate"));
}
} // namespace

class TestEngineSelector : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    static void registerEngines(const QStringList& names)
    {
        QList<EngineEntry> engines;
        for (const QString& name : names)
            engines.append({name, QStringLiteral("/engines/") + name, QStringLiteral("Author of ") + name});
        EngineProfileStore::saveEngines(engines);
    }

    static QSettings settings() { return QSettings(SettingsCommon::settingsFilePath(), QSettings::IniFormat); }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("EngineSelectorTest"));
    }

    void init()
    {
        settings().clear();
    }

    /// 保存が無ければ先頭の登録エンジンを選び、選んだものは名前で保存して次に戻す
    void restoresAndSavesByName()
    {
        registerEngines({QStringLiteral("Alpha"), QStringLiteral("Beta")});
        QComboBox combo;
        QPushButton settingsButton;
        QLabel hint;
        EngineSelector::Options options;
        options.keys = EngineListSettings::kifuAnalysisSelection();
        EngineSelector selector({&combo, &settingsButton, &hint}, options, nullptr);
        QCOMPARE(combo.count(), 2);
        QCOMPARE(selector.selected()->name, QStringLiteral("Alpha"));
        QVERIFY(settingsButton.isEnabled());
        QVERIFY(hint.isHidden());

        combo.setCurrentIndex(1);
        selector.saveSelection();
        QComboBox reopenedCombo;
        EngineSelector reopened({&reopenedCombo, nullptr, nullptr}, options, nullptr);
        QCOMPARE(reopened.selected()->name, QStringLiteral("Beta"));
        // 作者名も一覧の値のまま渡す
        QCOMPARE(reopened.selected()->author, QStringLiteral("Author of Beta"));
    }

    /// 以前の版が保存した番号でも同じエンジンを選ぶ（通信対局）
    void readsLegacyIndex()
    {
        registerEngines({QStringLiteral("Alpha"), QStringLiteral("Beta")});
        settings().setValue(SettingsKeys::kCsaEngineNumber, 1);
        QComboBox combo;
        EngineSelector::Options options;
        options.keys = EngineListSettings::csaGameSelection();
        EngineSelector selector({&combo, nullptr, nullptr}, options, nullptr);
        QCOMPARE(selector.selected()->name, QStringLiteral("Beta"));
        selector.saveSelection();
        QCOMPARE(settings().value(SettingsKeys::kCsaEngineName).toString(), QStringLiteral("Beta"));
        QVERIFY(!settings().contains(SettingsKeys::kCsaEngineNumber));
    }

    /// エンジンが無ければ設定ボタンを押せず、案内を出す
    void noEngines()
    {
        QComboBox combo;
        QPushButton settingsButton;
        QLabel hint;
        EngineSelector selector({&combo, &settingsButton, &hint}, {}, nullptr);
        QVERIFY(!selector.hasEngines());
        QVERIFY(!selector.selected());
        QVERIFY(!settingsButton.isEnabled());
        QVERIFY(!combo.isEnabled());
        QVERIFY(!hint.isHidden());
    }

    /// 内蔵エンジンの項目と絞り込み。未保存なら登録エンジンを優先し、内蔵を選んだことは空の名前で残す
    void builtinItemAndFilter()
    {
        registerEngines({QStringLiteral("Alpha"), QStringLiteral("Mate Solver")});
        EngineSelector::Options options;
        options.keys = EngineListSettings::tsumeCollectionSelection();
        options.builtinLabel = QStringLiteral("Builtin");
        options.filter = &isMateEngine;
        QComboBox combo;
        QPushButton settingsButton;
        EngineSelector selector({&combo, &settingsButton, nullptr}, options, nullptr);
        QCOMPARE(combo.count(), 2);
        QCOMPARE(selector.engines().entries().size(), 1);
        QCOMPARE(selector.selected()->name, QStringLiteral("Mate Solver"));

        combo.setCurrentIndex(0);
        QVERIFY(selector.isBuiltinSelected());
        QVERIFY(!selector.selected());
        QVERIFY(!settingsButton.isEnabled());
        selector.saveSelection();
        QComboBox reopenedCombo;
        EngineSelector reopened({&reopenedCombo, nullptr, nullptr}, options, nullptr);
        QVERIFY(reopened.isBuiltinSelected());
    }

    /// 詰将棋問題一覧の以前の版の印（内蔵は "@hayanagi"、登録エンジンはパス）を読む
    void tsumeCollectionLegacySelection()
    {
        registerEngines({QStringLiteral("Mate Solver")});
        EngineSelector::Options options;
        options.keys = EngineListSettings::tsumeCollectionSelection();
        options.builtinLabel = QStringLiteral("Builtin");
        options.filter = &isMateEngine;

        settings().setValue(SettingsKeys::kTsumeCollectionEngine, QStringLiteral("@hayanagi"));
        QComboBox builtinCombo;
        EngineSelector builtin({&builtinCombo, nullptr, nullptr}, options, nullptr);
        QVERIFY(builtin.isBuiltinSelected());

        settings().clear();
        registerEngines({QStringLiteral("Mate Solver")});
        settings().setValue(SettingsKeys::kTsumeCollectionEngine, QStringLiteral("/engines/Mate Solver"));
        QComboBox pathCombo;
        EngineSelector byPath({&pathCombo, nullptr, nullptr}, options, nullptr);
        QCOMPARE(byPath.selected()->name, QStringLiteral("Mate Solver"));
    }

    /// 「設定」は選んだエンジンを知らせ、読み直しても選択を保つ
    void settingsAndReload()
    {
        registerEngines({QStringLiteral("Alpha"), QStringLiteral("Beta")});
        QComboBox combo;
        QPushButton settingsButton;
        EngineSelector selector({&combo, &settingsButton, nullptr}, {}, nullptr);
        QSignalSpy requested(&selector, &EngineSelector::settingsRequested);
        combo.setCurrentIndex(1);
        settingsButton.click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.at(0).at(0).value<EngineEntry>().name, QStringLiteral("Beta"));

        registerEngines({QStringLiteral("Gamma"), QStringLiteral("Beta")});
        selector.reload();
        QCOMPARE(combo.count(), 2);
        QCOMPARE(selector.selected()->name, QStringLiteral("Beta"));
        selector.setEnabled(false);
        QVERIFY(!combo.isEnabled());
        QVERIFY(!settingsButton.isEnabled());
    }
};

QTEST_MAIN(TestEngineSelector)
#include "tst_engineselector.moc"
