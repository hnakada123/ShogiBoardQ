/// @file josekianalysisregisterdialog.cpp
/// @brief 棋譜解析の結果をまとめて定跡に登録する範囲のダイアログの実装

#include "josekianalysisregisterdialog.h"

#include "dialogfontscale.h"
#include "josekisettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

JosekiAnalysisRegisterDialog::JosekiAnalysisRegisterDialog(int minPly, int maxPly, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("結果をまとめて定跡に登録"));
    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    mainLayout->addLayout(form);

    m_fromPly = new QSpinBox(this);
    m_fromPly->setObjectName(QStringLiteral("josekiRegisterFromPly"));
    m_toPly = new QSpinBox(this);
    m_toPly->setObjectName(QStringLiteral("josekiRegisterToPly"));
    for (QSpinBox *spin : {m_fromPly, m_toPly}) spin->setRange(minPly, qMax(minPly, maxPly));
    m_fromPly->setValue(minPly);
    m_toPly->setValue(qMax(minPly, maxPly));
    auto *plyRow = new QHBoxLayout;
    plyRow->addWidget(m_fromPly, 1);
    plyRow->addWidget(new QLabel(tr("〜"), this));
    plyRow->addWidget(m_toPly, 1);
    form->addRow(tr("局面の手数:"), plyRow);

    m_black = new QCheckBox(tr("先手番の局面"), this);
    m_black->setObjectName(QStringLiteral("josekiRegisterBlack"));
    m_black->setChecked(JosekiSettings::josekiAnalysisRegisterBlack());
    m_white = new QCheckBox(tr("後手番の局面"), this);
    m_white->setObjectName(QStringLiteral("josekiRegisterWhite"));
    m_white->setChecked(JosekiSettings::josekiAnalysisRegisterWhite());
    auto *sideRow = new QHBoxLayout;
    sideRow->addWidget(m_black);
    sideRow->addWidget(m_white);
    sideRow->addStretch();
    form->addRow(tr("登録する局面:"), sideRow);

    auto *note = new QLabel(tr("各局面の読み筋の最初の手を、その局面の評価値・深さと読み筋の2手目（予想応手）で登録します。"
                               "登録済みの手は評価値・深さ・予想応手だけを書き換えます（出現頻度とコメントは残します）。"
                               "読み筋のない局面（エンジンが定跡で指した局面など）は登録しません。"),
                            this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);
    mainLayout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("登録"));
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fromPly, &QSpinBox::valueChanged, this, &JosekiAnalysisRegisterDialog::updateInputs);
    connect(m_black, &QCheckBox::toggled, this, &JosekiAnalysisRegisterDialog::updateInputs);
    connect(m_white, &QCheckBox::toggled, this, &JosekiAnalysisRegisterDialog::updateInputs);
    updateInputs();
    // 初めは全体を登録するので、Enter で登録できるようにする
    m_okButton->setDefault(true);
    m_okButton->setFocus();

    DialogFontScale::Options fontOptions;
    fontOptions.size = &JosekiSettings::josekiAnalysisRegisterDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiAnalysisRegisterDialogFontSize, fontOptions);
}

JosekiAnalysisRegisterDialog::~JosekiAnalysisRegisterDialog() = default;

JosekiAnalysisRegisterDialog::Range JosekiAnalysisRegisterDialog::range() const
{
    Range range;
    range.fromPly = m_fromPly->value();
    range.toPly = m_toPly->value();
    range.black = m_black->isChecked();
    range.white = m_white->isChecked();
    return range;
}

void JosekiAnalysisRegisterDialog::done(int result)
{
    if (result == QDialog::Accepted) {
        JosekiSettings::josekiAnalysisRegisterBlack.set(m_black->isChecked());
        JosekiSettings::josekiAnalysisRegisterWhite.set(m_white->isChecked());
    }
    QDialog::done(result);
}

void JosekiAnalysisRegisterDialog::updateInputs()
{
    m_toPly->setMinimum(m_fromPly->value());
    m_okButton->setEnabled(m_black->isChecked() || m_white->isChecked());
}
