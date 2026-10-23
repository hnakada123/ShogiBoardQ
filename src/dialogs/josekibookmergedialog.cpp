/// @file josekibookmergedialog.cpp
/// @brief 別の定跡ファイルを今の定跡に足すダイアログの実装

#include "josekibookmergedialog.h"

#include "dialogfontscale.h"
#include "josekisettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

JosekiBookMergeDialog::JosekiBookMergeDialog(const QString &directory, QWidget *parent)
    : QDialog(parent)
    , m_directory(directory)
{
    setWindowTitle(tr("定跡ファイルを足す"));
    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    mainLayout->addLayout(form);

    m_path = new QLineEdit(this);
    m_path->setObjectName(QStringLiteral("josekiBookMergePath"));
    auto *browseButton = new QPushButton(tr("参照..."), this);
    browseButton->setObjectName(QStringLiteral("josekiBookMergeBrowse"));
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browseButton);
    form->addRow(tr("足す定跡ファイル:"), pathRow);

    m_preferCurrent = new QRadioButton(tr("予想応手・評価値・深さ・コメントは今の定跡の値を残す"), this);
    m_preferCurrent->setObjectName(QStringLiteral("josekiBookMergePreferCurrent"));
    m_preferIncoming = new QRadioButton(tr("予想応手・評価値・深さ・コメントは足す定跡の値にする"), this);
    m_preferIncoming->setObjectName(QStringLiteral("josekiBookMergePreferIncoming"));
    m_form.add(m_preferCurrent, m_preferIncoming, JosekiSettings::josekiBookMergePreferIncoming);
    m_sumFrequency = new QCheckBox(tr("出現頻度を足し合わせる"), this);
    m_sumFrequency->setObjectName(QStringLiteral("josekiBookMergeSumFrequency"));
    m_form.add(m_sumFrequency, JosekiSettings::josekiBookMergeSumFrequency);
    m_form.load();
    m_sumFrequency->setToolTip(tr("足し合わせないときは、上で選んだ側の出現頻度にします"));
    auto *sameMoveRows = new QVBoxLayout;
    sameMoveRows->addWidget(m_preferCurrent);
    sameMoveRows->addWidget(m_preferIncoming);
    sameMoveRows->addWidget(m_sumFrequency);
    form->addRow(tr("同じ指し手:"), sameMoveRows);

    auto *note = new QLabel(tr("今の定跡に無い局面と指し手はそのまま足します（手数違いの登録も残します）。"
                               "足した後は定跡ウィンドウで保存してください。"),
                            this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);
    mainLayout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("足す"));
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseButton, &QPushButton::clicked, this, &JosekiBookMergeDialog::browse);
    connect(m_path, &QLineEdit::textChanged, this, &JosekiBookMergeDialog::updateInputs);
    updateInputs();

    DialogFontScale::Options fontOptions;
    fontOptions.size = &JosekiSettings::josekiBookMergeDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiBookMergeDialogFontSize, fontOptions);
}

JosekiBookMergeDialog::~JosekiBookMergeDialog() = default;

QString JosekiBookMergeDialog::filePath() const
{
    return m_path->text().trimmed();
}

JosekiBook::MergeOptions JosekiBookMergeDialog::options() const
{
    JosekiBook::MergeOptions options;
    options.preferIncoming = m_preferIncoming->isChecked();
    options.sumFrequency = m_sumFrequency->isChecked();
    return options;
}

void JosekiBookMergeDialog::done(int result)
{
    if (result == QDialog::Accepted) m_form.save();
    QDialog::done(result);
}

void JosekiBookMergeDialog::browse()
{
    const QString start = filePath().isEmpty() ? m_directory : QFileInfo(filePath()).absolutePath();
    const QString file = QFileDialog::getOpenFileName(this, tr("足す定跡ファイルを選択"), start,
                                                      tr("定跡ファイル (*.db);;すべてのファイル (*)"));
    if (!file.isEmpty()) m_path->setText(file);
}

void JosekiBookMergeDialog::updateInputs()
{
    m_okButton->setEnabled(QFileInfo(filePath()).isFile());
}
