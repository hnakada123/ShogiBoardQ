/// @file piecesoundprocessor.cpp
/// @brief 駒音 WAV の音質加工（音の高さ・3バンドイコライザー）の実装

#include "piecesoundprocessor.h"

#include <QtEndian>

#include <cmath>
#include <utility>

namespace {

constexpr int kWavHeaderSize = 44;
constexpr quint16 kPcmFormat = 1;
constexpr quint16 kBitsPerSample = 16;

quint32 readU32(const char* p) { return qFromLittleEndian<quint32>(p); }
quint16 readU16(const char* p) { return qFromLittleEndian<quint16>(p); }

void appendU32(QByteArray& out, quint32 v)
{
    char buf[4];
    qToLittleEndian(v, buf);
    out.append(buf, 4);
}

void appendU16(QByteArray& out, quint16 v)
{
    char buf[2];
    qToLittleEndian(v, buf);
    out.append(buf, 2);
}

} // namespace

// ---------------------------------------------------------------------------
// WAV デコード / エンコード
// ---------------------------------------------------------------------------

PieceSoundProcessor::Pcm PieceSoundProcessor::decodeWav(const QByteArray& wav)
{
    Pcm pcm;
    if (wav.size() < 12 || !wav.startsWith("RIFF") || wav.mid(8, 4) != "WAVE") {
        return pcm;
    }

    quint16 channels = 0;
    quint16 bits = 0;
    quint32 sampleRate = 0;
    bool formatOk = false;
    const char* data = wav.constData();
    qsizetype pos = 12;
    while (pos + 8 <= wav.size()) {
        const QByteArray id = wav.mid(pos, 4);
        const quint32 chunkSize = readU32(data + pos + 4);
        const qsizetype body = pos + 8;
        if (body + static_cast<qsizetype>(chunkSize) > wav.size()) {
            return Pcm{};
        }
        if (id == "fmt " && chunkSize >= 16) {
            const quint16 format = readU16(data + body);
            channels = readU16(data + body + 2);
            sampleRate = readU32(data + body + 4);
            bits = readU16(data + body + 14);
            formatOk = (format == kPcmFormat && channels == 1 && bits == kBitsPerSample && sampleRate > 0);
        } else if (id == "data") {
            if (!formatOk) return Pcm{};
            const qsizetype frames = static_cast<qsizetype>(chunkSize) / 2;
            pcm.samples.resize(frames);
            for (qsizetype i = 0; i < frames; ++i) {
                const auto v = static_cast<qint16>(readU16(data + body + i * 2));
                pcm.samples[i] = static_cast<float>(v) / 32768.0f;
            }
            pcm.sampleRate = static_cast<int>(sampleRate);
            return pcm;
        }
        pos = body + static_cast<qsizetype>(chunkSize) + (chunkSize & 1u);  // チャンクは 2 バイト境界
    }
    return Pcm{};
}

QByteArray PieceSoundProcessor::encodeWav(const Pcm& pcm)
{
    QByteArray out;
    const quint32 dataBytes = static_cast<quint32>(pcm.samples.size()) * 2u;
    out.reserve(kWavHeaderSize + static_cast<int>(dataBytes));

    out.append("RIFF");
    appendU32(out, 36u + dataBytes);
    out.append("WAVE");
    out.append("fmt ");
    appendU32(out, 16u);
    appendU16(out, kPcmFormat);
    appendU16(out, 1u);  // channels
    appendU32(out, static_cast<quint32>(pcm.sampleRate));
    appendU32(out, static_cast<quint32>(pcm.sampleRate) * 2u);  // byte rate
    appendU16(out, 2u);  // block align
    appendU16(out, kBitsPerSample);
    out.append("data");
    appendU32(out, dataBytes);
    for (const float s : pcm.samples) {
        const float clamped = std::max(-1.0f, std::min(1.0f, s));
        const auto v = static_cast<qint16>(std::lround(clamped * 32767.0f));
        appendU16(out, static_cast<quint16>(v));
    }
    return out;
}

// ---------------------------------------------------------------------------
// 加工
// ---------------------------------------------------------------------------

double PieceSoundProcessor::pitchRatio(int semitones)
{
    return std::pow(2.0, static_cast<double>(semitones) / 12.0);
}

PieceSoundProcessor::Pcm PieceSoundProcessor::apply(const Pcm& input, const PieceSoundTone& toneIn)
{
    const PieceSoundTone tone = toneIn.clamped();
    if (!input.isValid() || tone.isDefault()) {
        return input;
    }

    Pcm out;
    out.sampleRate = input.sampleRate;
    out.samples = input.samples;
    const double sr = input.sampleRate;

    // 1. 音の高さ: 再生速度を変える。高くする（間引く）ときは折り返し防止のローパスを先に掛ける
    const double ratio = pitchRatio(tone.pitchSemitones);
    if (ratio > 1.0) {
        const Biquad lp = lowPass(sr, 0.45 * sr / ratio);
        run(out.samples, lp);
        run(out.samples, lp);  // 2 段で 4 次
    }
    if (ratio != 1.0) {
        out.samples = resample(out.samples, ratio);
    }

    // 2. 3バンドEQ
    if (tone.lowDb != 0) run(out.samples, lowShelf(sr, kLowShelfHz, tone.lowDb));
    if (tone.midDb != 0) run(out.samples, peaking(sr, kMidPeakHz, kMidPeakQ, tone.midDb));
    if (tone.highDb != 0) run(out.samples, highShelf(sr, kHighShelfHz, tone.highDb));

    // 3. クリップ防止（上限を超えたときだけ全体を下げる）
    float peak = 0.0f;
    for (const float s : std::as_const(out.samples)) peak = std::max(peak, std::fabs(s));
    if (peak > kPeakLimit) {
        const float gain = kPeakLimit / peak;
        for (float& s : out.samples) s *= gain;
    }
    return out;
}

QVector<float> PieceSoundProcessor::resample(const QVector<float>& in, double ratio)
{
    if (in.size() < 2 || ratio <= 0.0) return in;

    const auto outSize = static_cast<qsizetype>(std::floor(static_cast<double>(in.size() - 1) / ratio)) + 1;
    QVector<float> out(outSize);
    for (qsizetype i = 0; i < outSize; ++i) {
        const double pos = static_cast<double>(i) * ratio;
        const auto k = static_cast<qsizetype>(std::floor(pos));
        const double frac = pos - static_cast<double>(k);
        const float a = in[std::min(k, in.size() - 1)];
        const float b = in[std::min(k + 1, in.size() - 1)];
        out[i] = static_cast<float>(a + (b - a) * frac);  // 線形補間
    }
    return out;
}

// ---------------------------------------------------------------------------
// 双二次フィルタ（RBJ Audio EQ Cookbook）
// ---------------------------------------------------------------------------

PieceSoundProcessor::Biquad PieceSoundProcessor::lowShelf(double sampleRate, double hz, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * M_PI * hz / sampleRate;
    const double c = std::cos(w0);
    const double alpha = std::sin(w0) / 2.0 * std::sqrt(2.0);  // S = 1
    const double sq = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) + (A - 1.0) * c + sq;
    return {A * ((A + 1.0) - (A - 1.0) * c + sq) / a0,
            2.0 * A * ((A - 1.0) - (A + 1.0) * c) / a0,
            A * ((A + 1.0) - (A - 1.0) * c - sq) / a0,
            -2.0 * ((A - 1.0) + (A + 1.0) * c) / a0,
            ((A + 1.0) + (A - 1.0) * c - sq) / a0};
}

PieceSoundProcessor::Biquad PieceSoundProcessor::highShelf(double sampleRate, double hz, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * M_PI * hz / sampleRate;
    const double c = std::cos(w0);
    const double alpha = std::sin(w0) / 2.0 * std::sqrt(2.0);  // S = 1
    const double sq = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
    return {A * ((A + 1.0) + (A - 1.0) * c + sq) / a0,
            -2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0,
            A * ((A + 1.0) + (A - 1.0) * c - sq) / a0,
            2.0 * ((A - 1.0) - (A + 1.0) * c) / a0,
            ((A + 1.0) - (A - 1.0) * c - sq) / a0};
}

PieceSoundProcessor::Biquad PieceSoundProcessor::peaking(double sampleRate, double hz, double q, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * M_PI * hz / sampleRate;
    const double c = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / A;
    return {(1.0 + alpha * A) / a0,
            -2.0 * c / a0,
            (1.0 - alpha * A) / a0,
            -2.0 * c / a0,
            (1.0 - alpha / A) / a0};
}

PieceSoundProcessor::Biquad PieceSoundProcessor::lowPass(double sampleRate, double hz)
{
    const double w0 = 2.0 * M_PI * hz / sampleRate;
    const double c = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * M_SQRT1_2);  // Q = 1/sqrt(2)
    const double a0 = 1.0 + alpha;
    return {(1.0 - c) / 2.0 / a0,
            (1.0 - c) / a0,
            (1.0 - c) / 2.0 / a0,
            -2.0 * c / a0,
            (1.0 - alpha) / a0};
}

void PieceSoundProcessor::run(QVector<float>& samples, const Biquad& f)
{
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    for (float& s : samples) {
        const double x0 = s;
        const double y0 = f.b0 * x0 + f.b1 * x1 + f.b2 * x2 - f.a1 * y1 - f.a2 * y2;
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
        s = static_cast<float>(y0);
    }
}
