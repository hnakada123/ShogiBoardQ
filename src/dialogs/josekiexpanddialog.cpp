/// @file josekiexpanddialog.cpp
/// @brief 定跡を棋譜の変化に展開する条件のダイアログの実装

#include "josekiexpanddialog.h"

#include "dialogfontscale.h"
#include "josekisettings.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

JosekiExpandDialog::JosekiExpandDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("定跡を棋譜に展開"));
    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    mainLayout->addLayout(form);

    m_maxPlies = new QSpinBox(this);
    m_maxPlies->setObjectName(QStringLiteral("josekiExpandMaxPlies"));
    m_maxPlies->setRange(1, 60);
    m_maxPlies->setSuffix(tr(" 手先まで"));
    m_maxPlies->setValue(qBound(1, JosekiSettings::josekiExpandMaxPlies(), 60));
    form->addRow(tr("展開する手数:"), m_maxPlies);

    m_movesPerPosition = new QSpinBox(this);
    m_movesPerPosition->setObjectName(QStringLiteral("josekiExpandMovesPerPosition"));
    m_movesPerPosition->setRange(1, 20);
    m_movesPerPosition->setSuffix(tr(" 手まで"));
    m_movesPerPosition->setValue(qBound(1, JosekiSettings::josekiExpandMovesPerPosition(), 20));
    m_movesPerPosition->setToolTip(tr("1つの局面から展開する定跡手の数（定跡の表の上から）"));
    form->addRow(tr("1局面の定跡手:"), m_movesPerPosition);

    m_minRatio = new QDoubleSpinBox(this);
    m_minRatio->setObjectName(QStringLiteral("josekiExpandMinRatio"));
    m_minRatio->setRange(0.0, 100.0);
    m_minRatio->setDecimals(1);
    m_minRatio->setSuffix(QStringLiteral(" %"));
    m_minRatio->setValue(qBound(0.0, JosekiSettings::josekiExpandMinRatio(), 100.0));
    m_minRatio->setToolTip(tr("その局面の出現頻度の合計に対する割合（定跡ウィンドウの「比率」）がこれより低い手は展開しません"));
    form->addRow(tr("比率の下限:"), m_minRatio);

    auto *note = new QLabel(tr("表示中の局面から定跡手をたどり、棋譜の変化として足します（表示中の手は変わりません）。"
                               "別の手順で同じ局面に着いたら、そこで止めます。棋譜にすでにある手は足しません。"),
                            this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);
    mainLayout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("展開"));
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    DialogFontScale::Options fontOptions;
    fontOptions.size = &JosekiSettings::josekiExpandDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiExpandDialogFontSize, fontOptions);
}

JosekiExpandDialog::~JosekiExpandDialog() = default;

JosekiLines::ExpandOptions JosekiExpandDialog::options() const
{
    JosekiLines::ExpandOptions options;
    options.maxPlies = m_maxPlies->value();
    options.maxMovesPerPosition = m_movesPerPosition->value();
    options.minRatioPercent = m_minRatio->value();
    return options;
}

void JosekiExpandDialog::done(int result)
{
    if (result == QDialog::Accepted) {
        JosekiSettings::josekiExpandMaxPlies.set(m_maxPlies->value());
        JosekiSettings::josekiExpandMovesPerPosition.set(m_movesPerPosition->value());
        JosekiSettings::josekiExpandMinRatio.set(m_minRatio->value());
    }
    QDialog::done(result);
}
