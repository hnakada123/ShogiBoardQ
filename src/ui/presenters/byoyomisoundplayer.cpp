/// @file byoyomisoundplayer.cpp
/// @brief 対局時計の秒読み・カウントダウン音声再生サービスの実装

#include "byoyomisoundplayer.h"

#include "apppaths.h"
#include "appsettings.h"
#include "logcategories.h"
#include "shogiclock.h"

#include <QAudio>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSoundEffect>
#include <QUrl>

#include <cmath>

namespace {
constexpr int kSampleRate = 44100;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr char kCacheSubdir[] = "byoyomi-sound";

/// 滑らかな立ち上がりと指数減衰を持つ単音を加算する
void addTone(QVector<float>& samples, double startSec, double durationSec, double freqHz, double amplitude,
             double decayRate, double harmonicRatio = 0.2)
{
    const int startSample = qMax(0, static_cast<int>(startSec * kSampleRate));
    const int count = static_cast<int>(durationSec * kSampleRate);
    const int endSample = qMin(static_cast<int>(samples.size()), startSample + count);
    const double attackSamples = qMax(1.0, 0.004 * kSampleRate);
    const double releaseSamples = qMax(1.0, 0.010 * kSampleRate);

    for (int i = startSample; i < endSample; ++i) {
        const int local = i - startSample;
        const double t = static_cast<double>(local) / kSampleRate;
        double env = std::exp(-decayRate * t);
        if (local < attackSamples) {
            env *= static_cast<double>(local) / attackSamples;
        }
        const int remain = endSample - 1 - i;
        if (remain < releaseSamples) {
            env *= static_cast<double>(remain) / releaseSamples;
        }
        const double wave = std::sin(kTwoPi * freqHz * t)
            + harmonicRatio * std::sin(kTwoPi * freqHz * 2.0 * t);
        samples[i] = static_cast<float>(qBound(-0.95, static_cast<double>(samples[i]) + amplitude * env * wave, 0.95));
    }
}

QString cueFileName(ByoyomiSoundPlayer::Cue cue)
{
    switch (cue) {
    case ByoyomiSoundPlayer::Cue::Notice:  return QStringLiteral("notice_v1.wav");
    case ByoyomiSoundPlayer::Cue::Tick:    return QStringLiteral("tick_v1.wav");
    case ByoyomiSoundPlayer::Cue::Urgent:  return QStringLiteral("urgent_v1.wav");
    case ByoyomiSoundPlayer::Cue::Timeout: return QStringLiteral("timeout_v1.wav");
    case ByoyomiSoundPlayer::Cue::None:    return QString();
    }
    return QString();
}

int previewSecondsFor(ByoyomiSoundPlayer::Cue cue)
{
    switch (cue) {
    case ByoyomiSoundPlayer::Cue::Notice:  return 10;
    case ByoyomiSoundPlayer::Cue::Tick:    return 7;
    case ByoyomiSoundPlayer::Cue::Urgent:  return 3;
    case ByoyomiSoundPlayer::Cue::Timeout: return 0;
    case ByoyomiSoundPlayer::Cue::None:    return 0;
    }
    return 0;
}
} // namespace

ByoyomiSoundPlayer::ByoyomiSoundPlayer(const ShogiClock* clock, QObject* parent)
    : QObject(parent)
    , m_clock(clock)
    , m_enabled(AppSettings::byoyomiSoundEnabled())
    , m_volume(AppSettings::byoyomiSoundVolume())
    , m_voiceDir(AppSettings::byoyomiSoundVoiceDir())
{
    if (m_clock) {
        connect(m_clock, &ShogiClock::timeUpdated, this, &ByoyomiSoundPlayer::onTimeUpdated);
        connect(m_clock, &ShogiClock::playerTimeOut, this, &ByoyomiSoundPlayer::onPlayerTimeOut);
    }
}

ByoyomiSoundPlayer::Decision ByoyomiSoundPlayer::evaluateTick(TickState& state, Turn turn, qint64 remainingMs,
                                                              bool inByoyomi, bool hasByoyomi)
{
    const int sec = remainingMs <= 0 ? 0 : static_cast<int>((remainingMs + 999) / 1000);
    const bool sameTurn = state.valid && state.turn == turn;
    const bool enteredByoyomi = sameTurn && !state.inByoyomi && inByoyomi;
    const bool decreased = sameTurn && state.inByoyomi == inByoyomi && sec < state.sec;

    state.valid = true;
    state.turn = turn;
    state.sec = sec;
    state.inByoyomi = inByoyomi;

    if (sec <= 0) return {};

    // 持ち時間から秒読みに入った瞬間は予告音を鳴らす
    if (enteredByoyomi) {
        return {Cue::Notice, sec};
    }

    // 同じ手番で秒数が減ったときだけカウント音を鳴らす（着手直後の満額秒数での誤鳴動を防ぐ）
    if (!decreased) return {};

    // 秒読み付きの持ち時間消費中は、節目の予告音のみ鳴らす
    if (hasByoyomi && !inByoyomi) {
        if (sec == 30 || sec == 20 || sec == 10) return {Cue::Notice, sec};
        return {};
    }

    // 秒読み中、または切れ負け・フィッシャーの終盤カウントダウン
    if (sec == 30 || sec == 20 || sec == 10) return {Cue::Notice, sec};
    if (sec >= 6 && sec <= 9) return {Cue::Tick, sec};
    if (sec >= 1 && sec <= 5) return {Cue::Urgent, sec};
    return {};
}

PieceSoundProcessor::Pcm ByoyomiSoundPlayer::synthesizeCue(Cue cue)
{
    PieceSoundProcessor::Pcm pcm;
    pcm.sampleRate = kSampleRate;

    switch (cue) {
    case Cue::Notice: {
        // 柔らかな2音チャイム（A5 -> C#6）
        pcm.samples.fill(0.0f, static_cast<int>(0.22 * kSampleRate));
        addTone(pcm.samples, 0.00, 0.11, 880.0, 0.42, 16.0, 0.18);
        addTone(pcm.samples, 0.075, 0.14, 1108.73, 0.45, 14.0, 0.15);
        break;
    }
    case Cue::Tick: {
        // 澄んだ短音（E5）
        pcm.samples.fill(0.0f, static_cast<int>(0.10 * kSampleRate));
        addTone(pcm.samples, 0.00, 0.095, 659.25, 0.45, 24.0, 0.22);
        break;
    }
    case Cue::Urgent: {
        // 明瞭な高めのカウント音（A5）
        pcm.samples.fill(0.0f, static_cast<int>(0.11 * kSampleRate));
        addTone(pcm.samples, 0.00, 0.105, 880.0, 0.55, 20.0, 0.28);
        break;
    }
    case Cue::Timeout: {
        // 低めの2連ブザー音（A4 x 2）
        pcm.samples.fill(0.0f, static_cast<int>(0.36 * kSampleRate));
        addTone(pcm.samples, 0.00, 0.14, 440.0, 0.52, 8.0, 0.35);
        addTone(pcm.samples, 0.17, 0.18, 440.0, 0.52, 7.0, 0.35);
        break;
    }
    case Cue::None:
        break;
    }
    return pcm;
}

QString ByoyomiSoundPlayer::resolveCustomWav(const QString& voiceDir, Cue cue, int seconds)
{
    if (voiceDir.isEmpty()) return {};
    const QDir dir(voiceDir);
    if (!dir.exists()) return {};

    QStringList candidates;
    if (cue == Cue::Timeout) {
        candidates << QStringLiteral("timeout.wav") << QStringLiteral("timeup.wav");
    } else if (seconds > 0) {
        candidates << QStringLiteral("%1.wav").arg(seconds)
                   << QStringLiteral("%1.wav").arg(seconds, 2, 10, QLatin1Char('0'));
    }
    for (const QString& name : candidates) {
        const QString path = dir.filePath(name);
        if (QFileInfo::exists(path)) return path;
    }
    return {};
}

void ByoyomiSoundPlayer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    AppSettings::byoyomiSoundEnabled.set(enabled);
    if (!enabled) m_state = TickState{};
}

void ByoyomiSoundPlayer::setVolume(int percent)
{
    AppSettings::byoyomiSoundVolume.set(percent);
    const int clamped = AppSettings::byoyomiSoundVolume();
    if (m_volume == clamped) return;
    m_volume = clamped;
    applyVolumeToAll();
}

void ByoyomiSoundPlayer::setVoiceDirectory(const QString& directory)
{
    const QString trimmed = directory.trimmed();
    if (m_voiceDir == trimmed) return;
    m_voiceDir = trimmed;
    AppSettings::byoyomiSoundVoiceDir.set(trimmed);
}

void ByoyomiSoundPlayer::preview(Cue cue)
{
    if (cue == Cue::None) return;
    playCue(cue, previewSecondsFor(cue));
}

void ByoyomiSoundPlayer::onTimeUpdated()
{
    if (!m_clock) return;
    if (!m_clock->isRunning() || m_clock->isUnlimited() || m_clock->isGameOver()) {
        m_state = TickState{};
        return;
    }
    const Turn turn = m_clock->currentPlayer();
    const Decision decision = evaluateTick(m_state, turn, m_clock->remainingMs(turn),
                                           m_clock->byoyomiApplied(turn), m_clock->hasByoyomi(turn));
    if (!m_enabled || decision.cue == Cue::None) return;
    playCue(decision.cue, decision.seconds);
}

void ByoyomiSoundPlayer::onPlayerTimeOut(Turn /*player*/)
{
    m_state = TickState{};
    if (!m_enabled) return;
    playCue(Cue::Timeout, 0);
}

QSoundEffect* ByoyomiSoundPlayer::effectForUrl(const QUrl& url)
{
    const QString key = url.toString();
    if (auto* existing = m_effects.value(key, nullptr)) return existing;

    // 同じ URL の効果音を使い回す（キャッシュ数が膨らみすぎないよう上限を設ける）
    if (m_effects.size() >= 32) {
        qDeleteAll(m_effects);
        m_effects.clear();
    }
    auto* effect = new QSoundEffect(this);
    const float perceived = static_cast<float>(m_volume) / static_cast<float>(AppSettings::byoyomiSoundVolume.maximum());
    const float linear = QtAudio::convertVolume(perceived, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
    effect->setVolume(linear);
    effect->setSource(url);
    m_effects.insert(key, effect);
    return effect;
}

void ByoyomiSoundPlayer::applyVolumeToAll()
{
    const float perceived = static_cast<float>(m_volume) / static_cast<float>(AppSettings::byoyomiSoundVolume.maximum());
    const float linear = QtAudio::convertVolume(perceived, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
    for (QSoundEffect* effect : std::as_const(m_effects)) {
        if (effect) effect->setVolume(linear);
    }
}

QString ByoyomiSoundPlayer::builtInWavPath(Cue cue) const
{
    const QString name = cueFileName(cue);
    if (name.isEmpty()) return {};
    const QString cacheRoot = AppPaths::cacheDirectory();
    if (cacheRoot.isEmpty()) return {};
    QDir dir(cacheRoot + QLatin1Char('/') + QLatin1String(kCacheSubdir));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) return {};
    const QString path = dir.absoluteFilePath(name);
    if (QFile::exists(path)) return path;

    const PieceSoundProcessor::Pcm pcm = synthesizeCue(cue);
    if (!pcm.isValid()) return {};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcUi) << "ByoyomiSoundPlayer: cannot write" << path;
        return {};
    }
    file.write(PieceSoundProcessor::encodeWav(pcm));
    if (!file.commit()) {
        qCWarning(lcUi) << "ByoyomiSoundPlayer: cannot commit" << path;
        return {};
    }
    return path;
}

void ByoyomiSoundPlayer::playCue(Cue cue, int seconds)
{
    QString path = resolveCustomWav(m_voiceDir, cue, seconds);
    if (path.isEmpty()) path = builtInWavPath(cue);
    if (path.isEmpty()) return;

    QSoundEffect* effect = effectForUrl(QUrl::fromLocalFile(path));
    if (effect->status() == QSoundEffect::Error) {
        qCWarning(lcUi) << "ByoyomiSoundPlayer: sound failed to load:" << effect->source();
        return;
    }
    effect->play();
}
