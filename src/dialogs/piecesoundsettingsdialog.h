#ifndef PIECESOUNDSETTINGSDIALOG_H
#define PIECESOUNDSETTINGSDIALOG_H

/// @file piecesoundsettingsdialog.h
/// @brief 駒音の設定（音量・音の高さ・イコライザー）ダイアログの定義

#include <QDialog>

#include "piecesoundtone.h"

class QSlider;
class QSpinBox;

/**
 * @brief 駒音の音量・音の高さ・3バンドイコライザーをスライダーで調整するダイアログ
 *
 * 各スライダーの変更はすぐに volumeChanged・toneChanged で知らせ、操作のたびに previewRequested で試聴を頼む
 * （駒音を鳴らすのは受け手。ui の DialogLauncher が PieceSoundPlayer に結ぶ）。
 * 「標準に戻す」で全項目を既定値へ、キャンセルで開いたときの値へ戻す（戻した値も知らせる）。
 */
class PieceSoundSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// volume・tone は開いたときの音量と音質
    PieceSoundSettingsDialog(int volume, const PieceSoundTone& tone, QWidget* parent = nullptr);

public slots:
    /// キャンセル: 開いたときの音量・音質へ戻して閉じる
    void reject() override;

signals:
    /// 音量が変わった（0〜100 %）
    void volumeChanged(int percent);
    /// 音質が変わった
    void toneChanged(const PieceSoundTone& tone);
    /// 今の設定で駒音を鳴らしてほしい（試聴）
    void previewRequested();

private slots:
    /// 音量スライダーの変更を知らせ、ドラッグ中でなければ試聴を頼む
    void onVolumeChanged(int value);

    /// 音の高さ / EQ スライダーの変更を知らせ、ドラッグ中でなければ試聴を頼む
    void onToneSliderChanged();

    /// ドラッグ終了時に試聴する
    void onSliderReleased();

    /// 今の設定での試聴を頼む
    void previewSound();

    /// 全項目を既定値へ戻す
    void restoreDefaults();

private:
    QSlider* makeGainSlider(int maxAbs, QWidget* parent);
    PieceSoundTone toneFromSliders() const;
    void applySliders(int volume, const PieceSoundTone& tone);
    void updateLabels();
    bool anySliderDown() const;

    int m_initialVolume;               ///< 開いたときの音量（キャンセル時に復元）
    PieceSoundTone m_initialTone;      ///< 開いたときの音質（キャンセル時に復元）
    bool m_updating = false;           ///< スライダー一括更新中は試聴を抑止する

    QSlider* m_volumeSlider;
    QSlider* m_pitchSlider;
    QSlider* m_lowSlider;
    QSlider* m_midSlider;
    QSlider* m_highSlider;
    QSpinBox* m_volumeValue;
    QSpinBox* m_pitchValue;
    QSpinBox* m_lowValue;
    QSpinBox* m_midValue;
    QSpinBox* m_highValue;
};

#endif // PIECESOUNDSETTINGSDIALOG_H
