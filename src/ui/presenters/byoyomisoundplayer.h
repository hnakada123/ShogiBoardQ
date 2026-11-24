#ifndef BYOYOMISOUNDPLAYER_H
#define BYOYOMISOUNDPLAYER_H

/// @file byoyomisoundplayer.h
/// @brief 対局時計の秒読み・カウントダウン音声再生サービスの定義

#include <QHash>
#include <QObject>
#include <QString>

#include "piecesoundprocessor.h"
#include "shogitypes.h"

class QSoundEffect;
class ShogiClock;

/**
 * @brief 対局時計（ShogiClock）の残り時間に応じて秒読み音声を再生するサービス
 *
 * 責務:
 * - ShogiClock::timeUpdated / playerTimeOut を受けて、適切な節目の秒数・時間切れで音を鳴らす
 * - 有効/無効・音量・カスタム音声フォルダの保持と AppSettings への永続化
 * - デフォルトでは内蔵の対局時計音（予告・カウント・ラスト5秒・時間切れ）を合成して再生する
 * - カスタム音声フォルダが設定されている場合、"{秒数}.wav" や "timeout.wav" があれば優先再生する
 *
 * 無効のあいだは QSoundEffect を生成しないため、オーディオデバイスを開かない。
 */
class ByoyomiSoundPlayer : public QObject
{
    Q_OBJECT

public:
    /// 鳴らす音の種類
    enum class Cue {
        None,     ///< 鳴らさない
        Notice,   ///< 予告音（残り 30・20・10 秒、または秒読み突入時）
        Tick,     ///< カウント音（残り 9〜6 秒）
        Urgent,   ///< 切迫カウント音（残り 5〜1 秒）
        Timeout,  ///< 時間切れ音
    };

    /// 重複再生を防ぐための前回通知状態
    struct TickState {
        bool valid = false;
        Turn turn = Turn::Black;
        int sec = -1;
        bool inByoyomi = false;
    };

    /// 1回の更新で鳴らすべき音の判定結果
    struct Decision {
        Cue cue = Cue::None;
        int seconds = 0;
    };

    explicit ByoyomiSoundPlayer(const ShogiClock* clock, QObject* parent = nullptr);

    /// 秒読み音声が有効かどうか
    bool isEnabled() const { return m_enabled; }

    /// 音量（0〜100 %）
    int volume() const { return m_volume; }

    /// カスタム音声フォルダ（空なら内蔵の対局時計音を使用）
    QString voiceDirectory() const { return m_voiceDir; }

    /// 残り時間と秒読み状態から鳴らすべき音を判定する（テスト可能な純粋ロジック）
    static Decision evaluateTick(TickState& state, Turn turn, qint64 remainingMs, bool inByoyomi, bool hasByoyomi);

    /// 内蔵の対局時計音の PCM 波形を合成する
    static PieceSoundProcessor::Pcm synthesizeCue(Cue cue);

    /// カスタム音声フォルダ内で該当する WAV ファイルのパスを返す（なければ空）
    static QString resolveCustomWav(const QString& voiceDir, Cue cue, int seconds);

public slots:
    /// 秒読み音声の有効/無効を切り替え、設定に保存する
    void setEnabled(bool enabled);

    /// 音量（0〜100 %）を設定し、設定に保存する
    void setVolume(int percent);

    /// カスタム音声フォルダを設定し、設定に保存する
    void setVoiceDirectory(const QString& directory);

    /// 有効/無効にかかわらず指定の音を再生する（設定ダイアログの試聴用）
    void preview(Cue cue);

private slots:
    void onTimeUpdated();
    void onPlayerTimeOut(Turn player);

private:
    QSoundEffect* effectForUrl(const QUrl& url);
    void applyVolumeToAll();
    QString builtInWavPath(Cue cue) const;
    void playCue(Cue cue, int seconds);

    const ShogiClock* m_clock = nullptr;
    QHash<QString, QSoundEffect*> m_effects;  ///< URL文字列ごとの効果音キャッシュ（親=this）
    bool m_enabled;
    int m_volume;
    QString m_voiceDir;
    TickState m_state;
};

#endif // BYOYOMISOUNDPLAYER_H
