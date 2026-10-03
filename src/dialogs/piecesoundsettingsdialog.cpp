/// @file piecesoundsettingsdialog.cpp
/// @brief 駒音の設定（音量・音の高さ・イコライザー）ダイアログの実装

#include "piecesoundsettingsdialog.h"
#include "piecesoundplayer.h"
#include "dialogfontscale.h"
#include <QSpinBox>
#include <QSignalBlocker>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

PieceSoundSettingsDialog::PieceSoundSettingsDialog(PieceSoundPlayer* player, QWidget* parent)
    : QDialog(parent)
    , m_player(player)
    , m_initialVolume(player ? player->volume() : PieceSoundPlayer::kDefaultVolume)
    , m_initialTone(player ? player->tone() : PieceSoundTone{})
    , m_volumeSlider(new QSlider(Qt::Horizontal, this))
    , m_pitchSlider(new QSlider(Qt::Horizontal, this))
    , m_lowSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_midSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_highSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_volumeValue(new QSpinBox(this))
    , m_pitchValue(new QSpinBox(this))
    , m_lowValue(new QSpinBox(this))
    , m_midValue(new QSpinBox(this))
    , m_highValue(new QSpinBox(this))
{
    setWindowTitle(tr("駒音の設定"));
    m_volumeSlider->setObjectName(QStringLiteral("pieceSoundVolume"));
    m_pitchSlider->setObjectName(QStringLiteral("pieceSoundPitch"));
    m_lowSlider->setObjectName(QStringLiteral("pieceSoundLow"));
    m_midSlider->setObjectName(QStringLiteral("pieceSoundMid"));
    m_highSlider->setObjectName(QStringLiteral("pieceSoundHigh"));
    setSizeGripEnabled(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // --- 音量 / 音の高さ（横スライダー） ---
    auto* grid = new QGridLayout();
    m_volumeSlider->setRange(PieceSoundPlayer::kMinVolume, PieceSoundPlayer::kMaxVolume);
    m_volumeSlider->setPageStep(10);
    m_volumeSlider->setTickInterval(10);
    m_volumeSlider->setTickPosition(QSlider::TicksBelow);
    m_volumeSlider->setMinimumWidth(280);

    m_pitchSlider->setRange(-PieceSoundTone::kMaxSemitones, PieceSoundTone::kMaxSemitones);
    m_pitchSlider->setPageStep(3);
    m_pitchSlider->setTickInterval(3);
    m_pitchSlider->setTickPosition(QSlider::TicksBelow);
    m_pitchSlider->setMinimumWidth(280);

    auto* volumeTitle = new QLabel(tr("音量"), this);
    auto* pitchTitle = new QLabel(tr("音の高さ"), this);
    volumeTitle->setBuddy(m_volumeValue);
    pitchTitle->setBuddy(m_pitchValue);
    grid->addWidget(volumeTitle, 0, 0);
    grid->addWidget(m_volumeSlider, 0, 1);
    grid->addWidget(m_volumeValue, 0, 2);
    grid->addWidget(pitchTitle, 1, 0);
    grid->addWidget(m_pitchSlider, 1, 1);
    grid->addWidget(m_pitchValue, 1, 2);
    grid->setColumnStretch(1, 1);
    mainLayout->addLayout(grid);

    // --- イコライザー（縦スライダー） ---
    auto* eqBox = new QGroupBox(tr("イコライザー"), this);
    auto* eqLayout = new QHBoxLayout(eqBox);
    const struct {
        QSlider* slider;
        QSpinBox* valueInput;
        QString title;
    } bands[] = {
        {m_lowSlider, m_lowValue, tr("低音")},
        {m_midSlider, m_midValue, tr("中音")},
        {m_highSlider, m_highValue, tr("高音")},
    };
    for (const auto& band : bands) {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel(band.title, eqBox);
        title->setAlignment(Qt::AlignHCenter);
        title->setBuddy(band.valueInput);
        column->addWidget(title);
        column->addWidget(band.slider, 0, Qt::AlignHCenter);
        column->addWidget(band.valueInput);
        eqLayout->addLayout(column);
    }
    mainLayout->addWidget(eqBox);

    // --- 説明 ---
    auto* hintLabel = new QLabel(tr("スライダーを動かすと駒音が鳴ります。"), this);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    // --- ボタン ---
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* defaultsButton = buttonBox->addButton(tr("標準に戻す"), QDialogButtonBox::ResetRole);
    auto* previewButton = buttonBox->addButton(tr("試聴"), QDialogButtonBox::ActionRole);
    mainLayout->addWidget(buttonBox);

    const struct {
        QSlider* slider;
        QSpinBox* input;
        QString title;
        QString suffix;
    } controls[] = {
        {m_volumeSlider, m_volumeValue, tr("音量"), QStringLiteral(" %")},
        {m_pitchSlider, m_pitchValue, tr("音の高さ"), tr(" 半音")},
        {m_lowSlider, m_lowValue, tr("低音"), QStringLiteral(" dB")},
        {m_midSlider, m_midValue, tr("中音"), QStringLiteral(" dB")},
        {m_highSlider, m_highValue, tr("高音"), QStringLiteral(" dB")},
    };
    for (const auto& control : controls) {
        control.input->setRange(control.slider->minimum(), control.slider->maximum());
        control.input->setSuffix(control.suffix);
        control.input->setKeyboardTracking(false);
        control.input->setObjectName(control.slider->objectName() + QStringLiteral("Value"));
        control.input->setAccessibleName(control.title);
        control.slider->setAccessibleName(control.title);
        connect(control.input, &QSpinBox::valueChanged, control.slider, &QSlider::setValue);
    }
    DialogFontScale::install(this, QStringLiteral("pieceSound"), true);

    // 初期値を反映してから接続する（初期化で試聴しないため）
    applySliders(m_initialVolume, m_initialTone);

    connect(m_volumeSlider, &QSlider::valueChanged, this, &PieceSoundSettingsDialog::onVolumeChanged);
    for (QSlider* slider : {m_pitchSlider, m_lowSlider, m_midSlider, m_highSlider}) {
        connect(slider, &QSlider::valueChanged, this, &PieceSoundSettingsDialog::onToneSliderChanged);
    }
    for (QSlider* slider : {m_volumeSlider, m_pitchSlider, m_lowSlider, m_midSlider, m_highSlider}) {
        connect(slider, &QSlider::sliderReleased, this, &PieceSoundSettingsDialog::onSliderReleased);
    }
    connect(defaultsButton, &QPushButton::clicked, this, &PieceSoundSettingsDialog::restoreDefaults);
    connect(previewButton, &QPushButton::clicked, this, &PieceSoundSettingsDialog::previewSound);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PieceSoundSettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PieceSoundSettingsDialog::reject);
}

QSlider* PieceSoundSettingsDialog::makeGainSlider(int maxAbs, QWidget* parent)
{
    auto* slider = new QSlider(Qt::Vertical, parent);
    slider->setRange(-maxAbs, maxAbs);
    slider->setPageStep(3);
    slider->setTickInterval(3);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setMinimumHeight(140);
    return slider;
}

void PieceSoundSettingsDialog::reject()
{
    if (m_player) {
        m_player->setVolume(m_initialVolume);
        m_player->setTone(m_initialTone);
    }
    QDialog::reject();
}

void PieceSoundSettingsDialog::onVolumeChanged(int value)
{
    updateLabels();
    if (!m_player || m_updating) return;

    m_player->setVolume(value);
    // ドラッグ中は連続して鳴らさず、離したとき（onSliderReleased）に試聴する
    if (!anySliderDown()) {
        m_player->preview();
    }
}

void PieceSoundSettingsDialog::onToneSliderChanged()
{
    updateLabels();
    if (!m_player || m_updating) return;

    m_player->setTone(toneFromSliders());
    if (!anySliderDown()) {
        m_player->preview();
    }
}

void PieceSoundSettingsDialog::onSliderReleased()
{
    previewSound();
}

void PieceSoundSettingsDialog::previewSound()
{
    if (m_player) {
        m_player->preview();
    }
}

void PieceSoundSettingsDialog::restoreDefaults()
{
    applySliders(PieceSoundPlayer::kDefaultVolume, PieceSoundTone{});
    if (m_player) {
        m_player->setVolume(PieceSoundPlayer::kDefaultVolume);
        m_player->setTone(PieceSoundTone{});
        m_player->preview();
    }
}

PieceSoundTone PieceSoundSettingsDialog::toneFromSliders() const
{
    PieceSoundTone tone;
    tone.pitchSemitones = m_pitchSlider->value();
    tone.lowDb = m_lowSlider->value();
    tone.midDb = m_midSlider->value();
    tone.highDb = m_highSlider->value();
    return tone;
}

void PieceSoundSettingsDialog::applySliders(int volume, const PieceSoundTone& tone)
{
    m_updating = true;
    m_volumeSlider->setValue(volume);
    m_pitchSlider->setValue(tone.pitchSemitones);
    m_lowSlider->setValue(tone.lowDb);
    m_midSlider->setValue(tone.midDb);
    m_highSlider->setValue(tone.highDb);
    m_updating = false;
    updateLabels();
}

void PieceSoundSettingsDialog::updateLabels()
{
    const QList<QPair<QSpinBox*, QSlider*>> controls{
        {m_volumeValue, m_volumeSlider}, {m_pitchValue, m_pitchSlider},
        {m_lowValue, m_lowSlider}, {m_midValue, m_midSlider}, {m_highValue, m_highSlider}};
    for (const auto& control : controls) {
        const QSignalBlocker blocker(control.first);
        control.first->setValue(control.second->value());
    }
}

bool PieceSoundSettingsDialog::anySliderDown() const
{
    for (const QSlider* slider : {m_volumeSlider, m_pitchSlider, m_lowSlider, m_midSlider, m_highSlider}) {
        if (slider->isSliderDown()) return true;
    }
    return false;
}
