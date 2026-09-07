#ifndef PIECESOUNDPROCESSOR_H
#define PIECESOUNDPROCESSOR_H

/// @file piecesoundprocessor.h
/// @brief 駒音 WAV の音質加工（音の高さ・3バンドイコライザー）

#include <QByteArray>
#include <QVector>

#include "piecesoundtone.h"

/**
 * @brief 駒音の PCM 波形に音質加工を施す純粋な処理クラス
 *
 * - 16bit PCM モノラル WAV のデコード / エンコード
 * - 音の高さ: 再生速度を変えるリサンプリング（高くすると短く、低くすると長くなる）
 * - 3バンドEQ: 低音（ローシェルフ 2.5 kHz）・中音（ピーキング 5 kHz, Q=1）・
 *   高音（ハイシェルフ 9 kHz）。RBJ Audio EQ Cookbook の双二次フィルタ
 * - クリップ防止: ピークが上限を超えた場合のみ全体を減衰
 *
 * Qt GUI / Multimedia に依存しないため単体テストできる。
 */
class PieceSoundProcessor
{
public:
    static constexpr double kLowShelfHz = 2500.0;
    static constexpr double kMidPeakHz = 5000.0;
    static constexpr double kMidPeakQ = 1.0;
    static constexpr double kHighShelfHz = 9000.0;
    static constexpr float kPeakLimit = 0.89f;

    /// デコード済みの波形
    struct Pcm {
        QVector<float> samples;  ///< -1.0〜1.0 のモノラル信号
        int sampleRate = 0;      ///< サンプリング周波数 Hz

        bool isValid() const { return sampleRate > 0 && !samples.isEmpty(); }
    };

    /// 16bit PCM モノラル WAV をデコードする（対応外の形式は無効な Pcm を返す）
    static Pcm decodeWav(const QByteArray& wav);

    /// 16bit PCM モノラル WAV にエンコードする
    static QByteArray encodeWav(const Pcm& pcm);

    /// 音質パラメータを適用した波形を返す（既定値なら入力をそのまま返す）
    static Pcm apply(const Pcm& input, const PieceSoundTone& tone);

    /// 半音数を再生速度倍率に変換する（+12 → 2.0、-12 → 0.5）
    static double pitchRatio(int semitones);

private:
    /// 双二次フィルタ係数（a0 で正規化済み）
    struct Biquad {
        double b0, b1, b2, a1, a2;
    };

    static Biquad lowShelf(double sampleRate, double hz, double gainDb);
    static Biquad highShelf(double sampleRate, double hz, double gainDb);
    static Biquad peaking(double sampleRate, double hz, double q, double gainDb);
    static Biquad lowPass(double sampleRate, double hz);
    static void run(QVector<float>& samples, const Biquad& f);
    static QVector<float> resample(const QVector<float>& in, double ratio);
};

#endif // PIECESOUNDPROCESSOR_H
