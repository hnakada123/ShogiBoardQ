/// @file piecesoundsettingsdialog.cpp
/// @brief 駒音の設定（音量・音の高さ・イコライザー）ダイアログの実装

#include "piecesoundsettingsdialog.h"
#include "piecesoundplayer.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {

/// 符号付きの整数表記（"+3" / "0" / "-2"）
QString signedNumber(int value)
{
    return value > 0 ? QStringLiteral("+%1").arg(value) : QString::number(value);
}

} // namespace

PieceSoundSettingsDialog::PieceSoundSettingsDialog(PieceSoundPlayer* player, QWidget* parent)
    : QDialog(parent)
    , m_player(player)
    , m_initialVolume(player ? player->volume() : PieceSoundPlayer::kMaxVolume)
    , m_initialTone(player ? player->tone() : PieceSoundTone{})
    , m_volumeSlider(new QSlider(Qt::Horizontal, this))
    , m_pitchSlider(new QSlider(Qt::Horizontal, this))
    , m_lowSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_midSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_highSlider(makeGainSlider(PieceSoundTone::kMaxGainDb, this))
    , m_volumeLabel(new QLabel(this))
    , m_pitchLabel(new QLabel(this))
    , m_lowLabel(new QLabel(this))
    , m_midLabel(new QLabel(this))
    , m_highLabel(new QLabel(this))
{
    setWindowTitle(tr("駒音の設定"));
    setSizeGripEnabled(false);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

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

    const int valueWidth = fontMetrics().horizontalAdvance(QStringLiteral("+12 半音")) + 8;
    for (QLabel* label : {m_volumeLabel, m_pitchLabel}) {
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setMinimumWidth(valueWidth);
    }

    grid->addWidget(new QLabel(tr("音量"), this), 0, 0);
    grid->addWidget(m_volumeSlider, 0, 1);
    grid->addWidget(m_volumeLabel, 0, 2);
    grid->addWidget(new QLabel(tr("音の高さ"), this), 1, 0);
    grid->addWidget(m_pitchSlider, 1, 1);
    grid->addWidget(m_pitchLabel, 1, 2);
    grid->setColumnStretch(1, 1);
    mainLayout->addLayout(grid);

    // --- イコライザー（縦スライダー） ---
    auto* eqBox = new QGroupBox(tr("イコライザー"), this);
    auto* eqLayout = new QHBoxLayout(eqBox);
    const struct {
        QSlider* slider;
        QLabel* valueLabel;
        QString title;
    } bands[] = {
        {m_lowSlider, m_lowLabel, tr("低音")},
        {m_midSlider, m_midLabel, tr("中音")},
        {m_highSlider, m_highLabel, tr("高音")},
    };
    for (const auto& band : bands) {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel(band.title, eqBox);
        title->setAlignment(Qt::AlignHCenter);
        band.valueLabel->setAlignment(Qt::AlignHCenter);
        column->addWidget(title);
        column->addWidget(band.slider, 0, Qt::AlignHCenter);
        column->addWidget(band.valueLabel);
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
    applySliders(PieceSoundPlayer::kMaxVolume, PieceSoundTone{});
    if (m_player) {
        m_player->setVolume(PieceSoundPlayer::kMaxVolume);
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
    m_volumeLabel->setText(tr("%1%").arg(m_volumeSlider->value()));
    m_pitchLabel->setText(tr("%1 半音").arg(signedNumber(m_pitchSlider->value())));
    m_lowLabel->setText(tr("%1 dB").arg(signedNumber(m_lowSlider->value())));
    m_midLabel->setText(tr("%1 dB").arg(signedNumber(m_midSlider->value())));
    m_highLabel->setText(tr("%1 dB").arg(signedNumber(m_highSlider->value())));
}

bool PieceSoundSettingsDialog::anySliderDown() const
{
    for (const QSlider* slider : {m_volumeSlider, m_pitchSlider, m_lowSlider, m_midSlider, m_highSlider}) {
        if (slider->isSliderDown()) return true;
    }
    return false;
}
