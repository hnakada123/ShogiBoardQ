/// @file tst_byoyomi_sound.cpp
/// @brief 秒読み音声（判定ロジック・波形合成・カスタムWAV解決・設定ダイアログ）のテスト

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>

#include "appsettings.h"
#include "byoyomisoundplayer.h"
#include "byoyomisoundsettingsdialog.h"
#include "piecesoundprocessor.h"
#include "settingscommon.h"

class TestByoyomiSound : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("byoyomisound-test"));
        QCoreApplication::setOrganizationName(QStringLiteral("ShogiBoardQ-tests"));
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void evaluateByoyomiCountdown()
    {
        using Cue = ByoyomiSoundPlayer::Cue;
        ByoyomiSoundPlayer::TickState state;

        // 10秒将棋：着手直後の満額10秒では鳴らない
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 10000, true, true).cue, Cue::None);
        // 9秒へ減ったら Tick
        auto d = ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 9000, true, true);
        QCOMPARE(d.cue, Cue::Tick);
        QCOMPARE(d.seconds, 9);
        // 同じ切り上げ秒（8500ms → 9秒）では重複再生しない
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 8500, true, true).cue, Cue::None);
        // 6秒は Tick、5秒〜1秒は Urgent
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 6000, true, true).cue, Cue::Tick);
        d = ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 5000, true, true);
        QCOMPARE(d.cue, Cue::Urgent);
        QCOMPARE(d.seconds, 5);
        d = ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 1000, true, true);
        QCOMPARE(d.cue, Cue::Urgent);
        QCOMPARE(d.seconds, 1);
        // 0秒は timeUpdated では鳴らさない（playerTimeOut で鳴らす）
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 0, true, true).cue, Cue::None);

        // 手番交代直後の満額30秒では鳴らない → 20秒・10秒で Notice
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::White, 30000, true, true).cue, Cue::None);
        d = ByoyomiSoundPlayer::evaluateTick(state, Turn::White, 20000, true, true);
        QCOMPARE(d.cue, Cue::Notice);
        QCOMPARE(d.seconds, 20);
        d = ByoyomiSoundPlayer::evaluateTick(state, Turn::White, 10000, true, true);
        QCOMPARE(d.cue, Cue::Notice);
        QCOMPARE(d.seconds, 10);
    }

    void evaluateMainTimeToByoyomiTransition()
    {
        using Cue = ByoyomiSoundPlayer::Cue;
        ByoyomiSoundPlayer::TickState state;

        // 秒読みありの持ち時間消費中：30・20・10秒のみ予告音、9〜1秒は鳴らさない
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 31000, false, true).cue, Cue::None);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 30000, false, true).cue, Cue::Notice);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 10000, false, true).cue, Cue::Notice);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 5000, false, true).cue, Cue::None);
        // 秒読みに入った瞬間は Notice
        const auto d = ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 30000, true, true);
        QCOMPARE(d.cue, Cue::Notice);
        QCOMPARE(d.seconds, 30);
    }

    void evaluateSuddenDeathCountdown()
    {
        using Cue = ByoyomiSoundPlayer::Cue;
        ByoyomiSoundPlayer::TickState state;

        // 切れ負け（hasByoyomi = false）：30/20/10秒 Notice、9..6秒 Tick、5..1秒 Urgent
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 31000, false, false).cue, Cue::None);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 30000, false, false).cue, Cue::Notice);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 8000, false, false).cue, Cue::Tick);
        QCOMPARE(ByoyomiSoundPlayer::evaluateTick(state, Turn::Black, 3000, false, false).cue, Cue::Urgent);
    }

    void synthesizeBuiltInCues()
    {
        using Cue = ByoyomiSoundPlayer::Cue;
        for (const Cue cue : {Cue::Notice, Cue::Tick, Cue::Urgent, Cue::Timeout}) {
            const PieceSoundProcessor::Pcm pcm = ByoyomiSoundPlayer::synthesizeCue(cue);
            QVERIFY(pcm.isValid());
            QCOMPARE(pcm.sampleRate, 44100);
            const QByteArray wav = PieceSoundProcessor::encodeWav(pcm);
            const PieceSoundProcessor::Pcm decoded = PieceSoundProcessor::decodeWav(wav);
            QVERIFY(decoded.isValid());
            QCOMPARE(decoded.samples.size(), pcm.samples.size());
        }
    }

    void resolveCustomWavFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto touch = [&dir](const QString& name) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("RIFF");
        };
        touch(QStringLiteral("10.wav"));
        touch(QStringLiteral("05.wav"));
        touch(QStringLiteral("timeout.wav"));

        QCOMPARE(ByoyomiSoundPlayer::resolveCustomWav(dir.path(), ByoyomiSoundPlayer::Cue::Notice, 10),
                 dir.filePath(QStringLiteral("10.wav")));
        QCOMPARE(ByoyomiSoundPlayer::resolveCustomWav(dir.path(), ByoyomiSoundPlayer::Cue::Urgent, 5),
                 dir.filePath(QStringLiteral("05.wav")));
        QCOMPARE(ByoyomiSoundPlayer::resolveCustomWav(dir.path(), ByoyomiSoundPlayer::Cue::Timeout, 0),
                 dir.filePath(QStringLiteral("timeout.wav")));
        QVERIFY(ByoyomiSoundPlayer::resolveCustomWav(dir.path(), ByoyomiSoundPlayer::Cue::Tick, 7).isEmpty());
    }

    void settingsDialogEmitsAndRestoresOnCancel()
    {
        ByoyomiSoundSettingsDialog dialog(60, QStringLiteral("/tmp/custom-voice"));
        QSignalSpy volumes(&dialog, &ByoyomiSoundSettingsDialog::volumeChanged);
        QSignalSpy dirs(&dialog, &ByoyomiSoundSettingsDialog::voiceDirectoryChanged);

        auto* slider = dialog.findChild<QSlider*>(QStringLiteral("byoyomiSoundVolume"));
        auto* edit = dialog.findChild<QLineEdit*>(QStringLiteral("byoyomiVoiceDir"));
        QVERIFY(slider && edit);

        slider->setValue(80);
        QCOMPARE(volumes.count(), 1);
        QCOMPARE(volumes.last().at(0).toInt(), 80);

        dialog.reject();
        QCOMPARE(volumes.last().at(0).toInt(), 60);
        QCOMPARE(dirs.last().at(0).toString(), QStringLiteral("/tmp/custom-voice"));
    }
};

QTEST_MAIN(TestByoyomiSound)
#include "tst_byoyomi_sound.moc"
