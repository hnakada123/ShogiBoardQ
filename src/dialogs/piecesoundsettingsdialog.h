#ifndef PIECESOUNDSETTINGSDIALOG_H
#define PIECESOUNDSETTINGSDIALOG_H

/// @file piecesoundsettingsdialog.h
/// @brief 駒音の設定（音量・音の高さ・イコライザー）ダイアログの定義

#include <QDialog>

#include "piecesoundtone.h"

class PieceSoundPlayer;
class QSlider;
class QLabel;

/**
 * @brief 駒音の音量・音の高さ・3バンドイコライザーをスライダーで調整するダイアログ
 *
 * 各スライダーの変更は PieceSoundPlayer に即時反映され、操作のたびに駒音を試聴できる。
 * 「標準に戻す」で全項目を既定値へ、キャンセルで開いたときの値へ戻す。
 */
class PieceSoundSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PieceSoundSettingsDialog(PieceSoundPlayer* player, QWidget* parent = nullptr);

public slots:
    /// キャンセル: 開いたときの音量・音質へ戻して閉じる
    void reject() override;

private slots:
    /// 音量スライダーの変更をプレイヤーへ反映し、ドラッグ中でなければ試聴する
    void onVolumeChanged(int value);

    /// 音の高さ / EQ スライダーの変更をプレイヤーへ反映し、ドラッグ中でなければ試聴する
    void onToneSliderChanged();

    /// ドラッグ終了時に試聴する
    void onSliderReleased();

    /// 現在の設定で駒音を鳴らす
    void previewSound();

    /// 全項目を既定値へ戻す
    void restoreDefaults();

private:
    QSlider* makeGainSlider(int maxAbs, QWidget* parent);
    PieceSoundTone toneFromSliders() const;
    void applySliders(int volume, const PieceSoundTone& tone);
    void updateLabels();
    bool anySliderDown() const;

    PieceSoundPlayer* m_player;        ///< 駒音プレイヤー（非所有）
    int m_initialVolume;               ///< 開いたときの音量（キャンセル時に復元）
    PieceSoundTone m_initialTone;      ///< 開いたときの音質（キャンセル時に復元）
    bool m_updating = false;           ///< スライダー一括更新中は試聴を抑止する

    QSlider* m_volumeSlider;
    QSlider* m_pitchSlider;
    QSlider* m_lowSlider;
    QSlider* m_midSlider;
    QSlider* m_highSlider;
    QLabel* m_volumeLabel;
    QLabel* m_pitchLabel;
    QLabel* m_lowLabel;
    QLabel* m_midLabel;
    QLabel* m_highLabel;
};

#endif // PIECESOUNDSETTINGSDIALOG_H
