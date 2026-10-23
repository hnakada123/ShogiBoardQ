/// @file tst_settings_form_binding.cpp
/// @brief 入力欄と設定の宣言の対応（SettingsFormBinding）のテスト

#include <QtTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QTemporaryDir>

#include "settingsformbinding.h"

namespace {
const Setting<double> ratio{"FormBindingTest/ratio", 2.5};
const Setting<int> mode{"FormBindingTest/mode", 1};
const Setting<bool> preferSecond{"FormBindingTest/preferSecond", false};
const Setting<int> choice{"FormBindingTest/choice", 0};
const Setting<QString> folder{"FormBindingTest/folder", QString()};
} // namespace

class TestSettingsFormBinding : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 小数の欄・ラジオボタンの組・2択のラジオ・コンボ・文字の欄を、同じ表で読み込み・保存・既定値に戻す
    void loadSaveAndResetEveryKind()
    {
        QDoubleSpinBox spin;
        spin.setRange(0.0, 100.0);
        spin.setDecimals(1);
        QWidget radios;
        auto* first = new QRadioButton(&radios);
        auto* second = new QRadioButton(&radios);
        auto* third = new QRadioButton(&radios);
        QWidget pair;
        auto* off = new QRadioButton(&pair);
        auto* on = new QRadioButton(&pair);
        QComboBox combo;
        combo.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
        QLineEdit edit;

        SettingsFormBinding form;
        form.add(&spin, ratio);
        form.add({first, second, third}, mode);
        form.add(off, on, preferSecond);
        form.add(&combo, choice);
        form.add(&edit, folder);

        form.load();
        QCOMPARE(spin.value(), 2.5);
        QVERIFY(second->isChecked());
        QVERIFY(off->isChecked() && !on->isChecked());

        spin.setValue(12.3);
        third->setChecked(true);
        on->setChecked(true);
        combo.setCurrentIndex(2);
        edit.setText(QStringLiteral("  /tmp/kifu  "));
        form.save();
        QCOMPARE(ratio(), 12.3);
        QCOMPARE(mode(), 2);
        QCOMPARE(preferSecond(), true);
        QCOMPARE(choice(), 2);
        QCOMPARE(folder(), QStringLiteral("/tmp/kifu"));   // 前後の空白は除いて保存する

        form.reset();
        QCOMPARE(spin.value(), 2.5);
        QVERIFY(second->isChecked());
        QVERIFY(off->isChecked());
        QCOMPARE(combo.currentIndex(), 0);
        QVERIFY(edit.text().isEmpty());
    }

    /// 範囲外の保存値は、ラジオは先頭、コンボは端に寄せる（選んでいない状態にしない）
    void outOfRangeValuesStaySelected()
    {
        mode.set(9);
        choice.set(7);
        QWidget radios;
        auto* first = new QRadioButton(&radios);
        auto* second = new QRadioButton(&radios);
        QComboBox combo;
        combo.addItems({QStringLiteral("a"), QStringLiteral("b")});
        SettingsFormBinding form;
        form.add({first, second}, mode);
        form.add(&combo, choice);
        form.load();
        QVERIFY(first->isChecked());
        QCOMPARE(combo.currentIndex(), 1);
    }
};

QTEST_MAIN(TestSettingsFormBinding)
#include "tst_settings_form_binding.moc"
