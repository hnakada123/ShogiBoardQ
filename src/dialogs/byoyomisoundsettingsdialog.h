#ifndef BYOYOMISOUNDSETTINGSDIALOG_H
#define BYOYOMISOUNDSETTINGSDIALOG_H

/// @file byoyomisoundsettingsdialog.h
/// @brief 秒読み音声設定ダイアログ（音量・カスタム音声フォルダ・試聴）の定義

#include <QDialog>
#include <QString>

#include "byoyomisoundplayer.h"

class QLabel;
class QLineEdit;
class QSlider;

/**
 * @brief 秒読み音声の音量・カスタム音声フォルダを調整するモーダルダイアログ
 *
 * 変更は即座にシグナルで通知し、試聴ボタンで音を確認できる。
 * キャンセルで閉じた場合は開いたときの値に戻す。
 */
class ByoyomiSoundSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    ByoyomiSoundSettingsDialog(int volume, const QString& voiceDir, QWidget* parent = nullptr);

signals:
    /// 音量が変わった（0〜100 %）
    void volumeChanged(int percent);

    /// カスタム音声フォルダが変わった（空なら内蔵の対局時計音）
    void voiceDirectoryChanged(const QString& directory);

    /// 試聴ボタンが押された
    void previewRequested(ByoyomiSoundPlayer::Cue cue);

public slots:
    void reject() override;

private slots:
    void onVolumeChanged(int value);
    void onBrowseVoiceDir();
    void onClearVoiceDir();
    void onVoiceDirEdited();
    void previewNotice();
    void previewTick();
    void previewUrgent();
    void previewTimeout();
    void restoreDefaults();

private:
    void updateVolumeLabel();

    int m_initialVolume;
    QString m_initialVoiceDir;

    QSlider* m_volumeSlider = nullptr;
    QLabel* m_volumeValue = nullptr;
    QLineEdit* m_voiceDirEdit = nullptr;
};

#endif // BYOYOMISOUNDSETTINGSDIALOG_H
