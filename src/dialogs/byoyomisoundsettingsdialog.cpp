/// @file byoyomisoundsettingsdialog.cpp
/// @brief 秒読み音声設定ダイアログ（音量・カスタム音声フォルダ・試聴）の実装

#include "byoyomisoundsettingsdialog.h"

#include "appsettings.h"
#include "dialogfontscale.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

ByoyomiSoundSettingsDialog::ByoyomiSoundSettingsDialog(int volume, const QString& voiceDir, QWidget* parent)
    : QDialog(parent)
    , m_initialVolume(volume)
    , m_initialVoiceDir(voiceDir)
{
    setWindowTitle(tr("秒読み音声の設定"));
    setModal(true);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName(QStringLiteral("byoyomiSoundVolume"));
    m_volumeSlider->setRange(AppSettings::byoyomiSoundVolume.minimum(), AppSettings::byoyomiSoundVolume.maximum());
    m_volumeSlider->setValue(volume);
    m_volumeSlider->setMinimumWidth(200);

    m_volumeValue = new QLabel(this);
    m_volumeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_volumeValue->setMinimumWidth(44);
    updateVolumeLabel();

    m_voiceDirEdit = new QLineEdit(voiceDir, this);
    m_voiceDirEdit->setObjectName(QStringLiteral("byoyomiVoiceDir"));
    m_voiceDirEdit->setPlaceholderText(tr("（内蔵の対局時計音を使用）"));
    auto* browseButton = new QPushButton(tr("参照…"), this);
    auto* clearButton = new QPushButton(tr("クリア"), this);

    auto* dirRow = new QHBoxLayout;
    dirRow->setContentsMargins(0, 0, 0, 0);
    dirRow->addWidget(m_voiceDirEdit, 1);
    dirRow->addWidget(browseButton);
    dirRow->addWidget(clearButton);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->addWidget(new QLabel(tr("音量"), this), 0, 0);
    grid->addWidget(m_volumeSlider, 0, 1);
    grid->addWidget(m_volumeValue, 0, 2);
    grid->addWidget(new QLabel(tr("音声フォルダ"), this), 1, 0);
    grid->addLayout(dirRow, 1, 1, 1, 2);
    grid->setColumnStretch(1, 1);

    auto* note = new QLabel(
        tr("空欄のときは内蔵の対局時計音（予告・カウント・ラスト5秒・時間切れ）を鳴らします。"
           "音声フォルダを指定すると、フォルダ内の 1.wav〜10.wav・20.wav・30.wav・timeout.wav を優先して再生します。"),
        this);
    note->setWordWrap(true);

    auto* previewNoticeBtn = new QPushButton(tr("試聴: 予告"), this);
    auto* previewTickBtn = new QPushButton(tr("試聴: カウント"), this);
    auto* previewUrgentBtn = new QPushButton(tr("試聴: ラスト5秒"), this);
    auto* previewTimeoutBtn = new QPushButton(tr("試聴: 時間切れ"), this);
    previewNoticeBtn->setAutoDefault(false);
    previewTickBtn->setAutoDefault(false);
    previewUrgentBtn->setAutoDefault(false);
    previewTimeoutBtn->setAutoDefault(false);

    auto* previewRow = new QHBoxLayout;
    previewRow->addWidget(previewNoticeBtn);
    previewRow->addWidget(previewTickBtn);
    previewRow->addWidget(previewUrgentBtn);
    previewRow->addWidget(previewTimeoutBtn);
    previewRow->addStretch();

    auto* defaultsButton = new QPushButton(tr("既定に戻す"), this);
    defaultsButton->setAutoDefault(false);
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* bottomRow = new QHBoxLayout;
    bottomRow->addWidget(defaultsButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttonBox);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addLayout(grid);
    layout->addWidget(note);
    layout->addLayout(previewRow);
    layout->addLayout(bottomRow);

    DialogFontScale::install(this, AppSettings::byoyomiSoundDialogFontSize, {&AppSettings::byoyomiSoundDialogSize});

    connect(m_volumeSlider, &QSlider::valueChanged, this, &ByoyomiSoundSettingsDialog::onVolumeChanged);
    connect(browseButton, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::onBrowseVoiceDir);
    connect(clearButton, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::onClearVoiceDir);
    connect(m_voiceDirEdit, &QLineEdit::editingFinished, this, &ByoyomiSoundSettingsDialog::onVoiceDirEdited);
    connect(previewNoticeBtn, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::previewNotice);
    connect(previewTickBtn, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::previewTick);
    connect(previewUrgentBtn, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::previewUrgent);
    connect(previewTimeoutBtn, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::previewTimeout);
    connect(defaultsButton, &QPushButton::clicked, this, &ByoyomiSoundSettingsDialog::restoreDefaults);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ByoyomiSoundSettingsDialog::reject()
{
    emit volumeChanged(m_initialVolume);
    emit voiceDirectoryChanged(m_initialVoiceDir);
    QDialog::reject();
}

void ByoyomiSoundSettingsDialog::onVolumeChanged(int value)
{
    updateVolumeLabel();
    emit volumeChanged(value);
}

void ByoyomiSoundSettingsDialog::onBrowseVoiceDir()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this, tr("秒読み音声のフォルダを選択"), m_voiceDirEdit->text());
    if (selected.isEmpty()) return;
    m_voiceDirEdit->setText(selected);
    emit voiceDirectoryChanged(selected);
}

void ByoyomiSoundSettingsDialog::onClearVoiceDir()
{
    if (m_voiceDirEdit->text().isEmpty()) return;
    m_voiceDirEdit->clear();
    emit voiceDirectoryChanged(QString());
}

void ByoyomiSoundSettingsDialog::onVoiceDirEdited()
{
    emit voiceDirectoryChanged(m_voiceDirEdit->text().trimmed());
}

void ByoyomiSoundSettingsDialog::previewNotice()
{
    emit previewRequested(ByoyomiSoundCue::Notice);
}

void ByoyomiSoundSettingsDialog::previewTick()
{
    emit previewRequested(ByoyomiSoundCue::Tick);
}

void ByoyomiSoundSettingsDialog::previewUrgent()
{
    emit previewRequested(ByoyomiSoundCue::Urgent);
}

void ByoyomiSoundSettingsDialog::previewTimeout()
{
    emit previewRequested(ByoyomiSoundCue::Timeout);
}

void ByoyomiSoundSettingsDialog::restoreDefaults()
{
    const int defaultVolume = AppSettings::byoyomiSoundVolume.defaultValue();
    {
        const QSignalBlocker blockVol(m_volumeSlider);
        m_volumeSlider->setValue(defaultVolume);
    }
    m_voiceDirEdit->clear();
    updateVolumeLabel();
    emit volumeChanged(defaultVolume);
    emit voiceDirectoryChanged(QString());
}

void ByoyomiSoundSettingsDialog::updateVolumeLabel()
{
    m_volumeValue->setText(QStringLiteral("%1%").arg(m_volumeSlider->value()));
}
