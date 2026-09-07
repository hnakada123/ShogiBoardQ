/// @file piecesoundplayer.cpp
/// @brief 駒音（駒を指したときの効果音）再生サービスの実装

#include "piecesoundplayer.h"
#include "appsettings.h"
#include "logcategories.h"
#include "piecesoundprocessor.h"

#include <QAudio>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>

namespace {
/// 駒音リソースのパス（resources/sounds/piece_move.wav、scripts/generate-piece-sound.py で生成）
constexpr char kMoveSoundUrl[] = "qrc:/sounds/piece_move.wav";
constexpr char kMoveSoundResource[] = ":/sounds/piece_move.wav";
/// 加工済み WAV を置くキャッシュのサブディレクトリ
constexpr char kCacheSubdir[] = "piece-sound";
constexpr char kCacheFilePattern[] = "piece_move_*.wav";
} // namespace

PieceSoundPlayer::PieceSoundPlayer(QObject* parent)
    : QObject(parent)
    , m_enabled(AppSettings::pieceSoundEnabled())
    , m_volume(AppSettings::pieceSoundVolume())
    , m_tone(AppSettings::pieceSoundTone())
{
    // 初回着手時の読み込み待ちを避けるため、有効なら起動時に読み込んでおく
    if (m_enabled) {
        ensureEffect();
    }
}

void PieceSoundPlayer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;

    m_enabled = enabled;
    AppSettings::setPieceSoundEnabled(enabled);
    if (enabled) {
        ensureEffect();
    }
    emit enabledChanged(enabled);
}

void PieceSoundPlayer::setVolume(int percent)
{
    const int clamped = qBound(kMinVolume, percent, kMaxVolume);
    if (m_volume == clamped) return;

    m_volume = clamped;
    AppSettings::setPieceSoundVolume(clamped);
    applyVolume();
    emit volumeChanged(clamped);
}

void PieceSoundPlayer::setTone(const PieceSoundTone& tone)
{
    const PieceSoundTone clamped = tone.clamped();
    if (m_tone == clamped) return;

    m_tone = clamped;
    AppSettings::setPieceSoundTone(clamped);
    updateSource();
    emit toneChanged(clamped);
}

void PieceSoundPlayer::playMoveSound()
{
    if (!m_enabled) return;
    playNow();
}

void PieceSoundPlayer::preview()
{
    playNow();
}

void PieceSoundPlayer::playNow()
{
    ensureEffect();
    if (m_effect->status() == QSoundEffect::Error) {
        qCWarning(lcUi) << "PieceSoundPlayer: cannot play, sound failed to load:" << m_effect->source();
        return;
    }
    // 読み込み中でも QSoundEffect が完了後に再生を開始する
    m_effect->play();
}

void PieceSoundPlayer::ensureEffect()
{
    if (m_effect) return;

    // Lifetime: owned by this (QObject parent)
    m_effect = new QSoundEffect(this);
    applyVolume();
    updateSource();
}

void PieceSoundPlayer::applyVolume()
{
    if (!m_effect) return;

    // スライダーの % 値は知覚に合わせた対数スケールとして扱い、線形振幅へ変換する
    const float perceived = static_cast<float>(m_volume) / static_cast<float>(kMaxVolume);
    const float linear = QtAudio::convertVolume(perceived,
                                                QtAudio::LogarithmicVolumeScale,
                                                QtAudio::LinearVolumeScale);
    m_effect->setVolume(linear);
}

void PieceSoundPlayer::updateSource()
{
    if (!m_effect) return;

    QUrl url(QString::fromLatin1(kMoveSoundUrl));
    if (!m_tone.isDefault()) {
        const QString path = renderProcessedFile();
        if (!path.isEmpty()) {
            url = QUrl::fromLocalFile(path);
        }
    }
    if (m_effect->source() != url) {
        m_effect->setSource(url);
        qCDebug(lcUi) << "PieceSoundPlayer: source =" << url;
    }
}

QString PieceSoundPlayer::renderProcessedFile() const
{
    QFile resource(QString::fromLatin1(kMoveSoundResource));
    if (!resource.open(QIODevice::ReadOnly)) {
        qCWarning(lcUi) << "PieceSoundPlayer: cannot open resource" << kMoveSoundResource;
        return {};
    }
    const PieceSoundProcessor::Pcm source = PieceSoundProcessor::decodeWav(resource.readAll());
    if (!source.isValid()) {
        qCWarning(lcUi) << "PieceSoundPlayer: unsupported WAV format in" << kMoveSoundResource;
        return {};
    }

    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        qCWarning(lcUi) << "PieceSoundPlayer: no writable cache location";
        return {};
    }
    QDir dir(cacheRoot + QLatin1Char('/') + QLatin1String(kCacheSubdir));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qCWarning(lcUi) << "PieceSoundPlayer: cannot create cache dir" << dir.absolutePath();
        return {};
    }

    // ファイル名にパラメータを含め、内容と URL が 1 対 1 に対応するようにする
    // （QSoundEffect は URL 単位で波形をキャッシュするため）
    const QString fileName = QStringLiteral("piece_move_p%1_l%2_m%3_h%4.wav")
                                 .arg(m_tone.pitchSemitones)
                                 .arg(m_tone.lowDb)
                                 .arg(m_tone.midDb)
                                 .arg(m_tone.highDb);
    const QString path = dir.absoluteFilePath(fileName);
    if (QFile::exists(path)) {
        return path;
    }

    // 古い加工済みファイルを片付ける
    const QStringList stale = dir.entryList({QString::fromLatin1(kCacheFilePattern)}, QDir::Files);
    for (const QString& name : stale) {
        dir.remove(name);
    }

    const PieceSoundProcessor::Pcm processed = PieceSoundProcessor::apply(source, m_tone);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcUi) << "PieceSoundPlayer: cannot write" << path;
        return {};
    }
    file.write(PieceSoundProcessor::encodeWav(processed));
    if (!file.commit()) {
        qCWarning(lcUi) << "PieceSoundPlayer: cannot commit" << path;
        return {};
    }
    qCDebug(lcUi) << "PieceSoundPlayer: rendered" << path;
    return path;
}
