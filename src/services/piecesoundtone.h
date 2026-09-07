#ifndef PIECESOUNDTONE_H
#define PIECESOUNDTONE_H

/// @file piecesoundtone.h
/// @brief 駒音の音質パラメータ（音の高さ・3バンドイコライザー）

#include <algorithm>

/**
 * @brief 駒音の音質パラメータ
 *
 * - pitchSemitones: 音の高さ（半音、±kMaxSemitones）。正で高く短く、負で低く長くなる
 * - lowDb / midDb / highDb: 低音（2.5 kHz 以下）/ 中音（5 kHz 付近）/ 高音（9 kHz 以上）の
 *   ゲイン dB（±kMaxGainDb）
 *
 * すべて 0 が既定（加工なし）。設定の保存・復元と PieceSoundProcessor の入力に使う。
 */
struct PieceSoundTone {
    static constexpr int kMaxSemitones = 12;
    static constexpr int kMaxGainDb = 12;

    int pitchSemitones = 0;
    int lowDb = 0;
    int midDb = 0;
    int highDb = 0;

    /// 加工なし（既定値）かどうか
    bool isDefault() const
    {
        return pitchSemitones == 0 && lowDb == 0 && midDb == 0 && highDb == 0;
    }

    /// 各値を許容範囲に丸めた複製を返す
    PieceSoundTone clamped() const
    {
        PieceSoundTone t;
        t.pitchSemitones = std::clamp(pitchSemitones, -kMaxSemitones, kMaxSemitones);
        t.lowDb = std::clamp(lowDb, -kMaxGainDb, kMaxGainDb);
        t.midDb = std::clamp(midDb, -kMaxGainDb, kMaxGainDb);
        t.highDb = std::clamp(highDb, -kMaxGainDb, kMaxGainDb);
        return t;
    }

    bool operator==(const PieceSoundTone& other) const
    {
        return pitchSemitones == other.pitchSemitones && lowDb == other.lowDb
               && midDb == other.midDb && highDb == other.highDb;
    }

    bool operator!=(const PieceSoundTone& other) const { return !(*this == other); }
};

#endif // PIECESOUNDTONE_H
