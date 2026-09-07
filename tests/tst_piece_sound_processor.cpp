/// @file tst_piece_sound_processor.cpp
/// @brief PieceSoundProcessor（駒音の音質加工）の単体テスト
///
/// WAV のデコード/エンコード、音の高さ（リサンプリング）、3バンドEQ、
/// クリップ防止、パラメータの丸めを検証する。

#include <QtTest>

#include <cmath>

#include "piecesoundprocessor.h"
#include "piecesoundtone.h"

class TestPieceSoundProcessor : public QObject
{
    Q_OBJECT

private:
    static constexpr int kSampleRate = 44100;

    /// 減衰ノイズ + 5 kHz 正弦波の合成クリック（決定的）
    static PieceSoundProcessor::Pcm makeClick(int frames = 8820)
    {
        PieceSoundProcessor::Pcm pcm;
        pcm.sampleRate = kSampleRate;
        pcm.samples.resize(frames);
        quint32 state = 12345u;
        for (int i = 0; i < frames; ++i) {
            state = state * 1664525u + 1013904223u;  // LCG
            const double noise = (static_cast<double>(state >> 8) / 16777216.0) * 2.0 - 1.0;
            const double t = static_cast<double>(i) / kSampleRate;
            const double env = std::exp(-t / 0.02);
            const double tone = 0.5 * std::sin(2.0 * M_PI * 5000.0 * t);
            pcm.samples[i] = static_cast<float>(0.6 * env * (0.7 * noise + tone));
        }
        return pcm;
    }

    /// lo〜hi Hz のパワー（Goertzel を 250 Hz 刻みで合計）
    static double bandPower(const QVector<float>& x, double lo, double hi)
    {
        double total = 0.0;
        for (double f = lo; f < hi; f += 250.0) {
            const double w = 2.0 * M_PI * f / kSampleRate;
            const double coeff = 2.0 * std::cos(w);
            double s0 = 0.0, s1 = 0.0, s2 = 0.0;
            for (const float v : x) {
                s0 = v + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            total += s1 * s1 + s2 * s2 - coeff * s1 * s2;
        }
        return total;
    }

    static float peakOf(const QVector<float>& x)
    {
        float peak = 0.0f;
        for (const float v : x) peak = std::max(peak, std::fabs(v));
        return peak;
    }

private slots:
    void wavRoundtrip()
    {
        const auto src = makeClick(1000);
        const QByteArray wav = PieceSoundProcessor::encodeWav(src);
        QCOMPARE(wav.size(), 44 + 1000 * 2);
        QVERIFY(wav.startsWith("RIFF"));

        const auto decoded = PieceSoundProcessor::decodeWav(wav);
        QVERIFY(decoded.isValid());
        QCOMPARE(decoded.sampleRate, kSampleRate);
        QCOMPARE(decoded.samples.size(), src.samples.size());
        for (int i = 0; i < src.samples.size(); ++i) {
            QVERIFY(std::fabs(decoded.samples[i] - src.samples[i]) < 1.0f / 32768.0f + 1e-6f);
        }
    }

    void decodeRejectsGarbage()
    {
        QVERIFY(!PieceSoundProcessor::decodeWav(QByteArray("not a wav")).isValid());
        QVERIFY(!PieceSoundProcessor::decodeWav(QByteArray()).isValid());
    }

    void defaultToneIsIdentity()
    {
        const auto src = makeClick();
        const auto out = PieceSoundProcessor::apply(src, PieceSoundTone{});
        QCOMPARE(out.sampleRate, src.sampleRate);
        QCOMPARE(out.samples, src.samples);
    }

    void pitchRatioFollowsSemitones()
    {
        QVERIFY(std::fabs(PieceSoundProcessor::pitchRatio(0) - 1.0) < 1e-12);
        QVERIFY(std::fabs(PieceSoundProcessor::pitchRatio(12) - 2.0) < 1e-12);
        QVERIFY(std::fabs(PieceSoundProcessor::pitchRatio(-12) - 0.5) < 1e-12);
    }

    void pitchUpShortensAndBrightens()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.pitchSemitones = 12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        QVERIFY(std::abs(out.samples.size() - src.samples.size() / 2) <= 2);

        // 5 kHz の正弦成分が 10 kHz 付近へ移動する
        const double srcHigh = bandPower(src.samples, 9000, 11000) / bandPower(src.samples, 4000, 6000);
        const double outHigh = bandPower(out.samples, 9000, 11000) / bandPower(out.samples, 4000, 6000);
        QVERIFY(outHigh > srcHigh * 4.0);
    }

    void pitchDownLengthens()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.pitchSemitones = -12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        QVERIFY(std::abs(out.samples.size() - src.samples.size() * 2) <= 2);
    }

    void highBoostRaisesHighBand()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.highDb = 12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        const double srcRatio = bandPower(src.samples, 10000, 16000) / bandPower(src.samples, 1000, 3000);
        const double outRatio = bandPower(out.samples, 10000, 16000) / bandPower(out.samples, 1000, 3000);
        QVERIFY(outRatio > srcRatio * 4.0);  // +12 dB ≒ ×16 に対して十分な余裕
    }

    void lowCutLowersLowBand()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.lowDb = -12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        const double srcRatio = bandPower(src.samples, 500, 1500) / bandPower(src.samples, 6000, 8000);
        const double outRatio = bandPower(out.samples, 500, 1500) / bandPower(out.samples, 6000, 8000);
        QVERIFY(outRatio < srcRatio / 4.0);
    }

    void midBoostRaisesMidBand()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.midDb = 12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        const double srcRatio = bandPower(src.samples, 4500, 5500) / bandPower(src.samples, 12000, 16000);
        const double outRatio = bandPower(out.samples, 4500, 5500) / bandPower(out.samples, 12000, 16000);
        QVERIFY(outRatio > srcRatio * 4.0);
    }

    void peakIsLimitedAfterBoost()
    {
        const auto src = makeClick();
        PieceSoundTone tone;
        tone.lowDb = 12;
        tone.midDb = 12;
        tone.highDb = 12;
        const auto out = PieceSoundProcessor::apply(src, tone);
        QVERIFY(peakOf(out.samples) <= PieceSoundProcessor::kPeakLimit + 1e-5f);
    }

    void toneClampAndDefault()
    {
        PieceSoundTone tone;
        QVERIFY(tone.isDefault());
        tone.pitchSemitones = 40;
        tone.lowDb = -50;
        tone.midDb = 7;
        tone.highDb = 13;
        const PieceSoundTone c = tone.clamped();
        QCOMPARE(c.pitchSemitones, PieceSoundTone::kMaxSemitones);
        QCOMPARE(c.lowDb, -PieceSoundTone::kMaxGainDb);
        QCOMPARE(c.midDb, 7);
        QCOMPARE(c.highDb, PieceSoundTone::kMaxGainDb);
        QVERIFY(!c.isDefault());
        QVERIFY(c != tone);
        QVERIFY(c == c.clamped());
    }
};

QTEST_APPLESS_MAIN(TestPieceSoundProcessor)
#include "tst_piece_sound_processor.moc"
