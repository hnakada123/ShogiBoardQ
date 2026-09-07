#!/usr/bin/env python3
"""駒音（駒を将棋盤に打ち付ける「パチッ」）を合成して WAV ファイルを生成する。

使い方:
    python3 scripts/generate-piece-sound.py [出力パス] [--preset 名前] [--peak-db dBFS]
    python3 scripts/generate-piece-sound.py --samples 出力ディレクトリ

出力先を省略すると resources/sounds/piece_move.wav に DEFAULT_PRESET の音を書き出す。
--peak-db でプリセットの音量（ピーク dBFS）だけを上書きできる。
--samples を指定すると SAMPLE_PRESETS の全プリセットを聴き比べ用に書き出す。
numpy が必要。生成結果は決定的（乱数シード固定）。

音の設計方針:
    実録音の駒音（榧盤に黄楊駒）を 1/3 オクターブ帯域ごとに解析すると、
    - 打撃直後のエネルギーは 5〜8 kHz を頂点に 3〜16 kHz へ広がる
    - 2.5 kHz 以上は減衰時間 30〜60 ms の明るい尾を引く
    - 2 kHz 以下は 10 ms 前後で消え、低い共鳴はほとんど残らない
    という特徴を持つ。低い共鳴を長く残すと瓶の栓を抜くような「ポン」に
    なるため、帯域別の「速い成分＋遅い尾」を持つノイズの重ね合わせで
    このプロファイルを再現する。

音の構成（1 回の接触音）:
    1. 帯域別ノイズ : 各帯域に「速い成分（数 ms）」と「遅い尾（数十 ms）」の
                      2 段エンベロープを持たせた帯域制限ノイズ（BAND_PROFILE）
    2. 立ち上がり   : 0.2 ms の低域を含まないクリックで打撃の瞬間を定義

全体では主打撃の約 7 ms 後に弱い二度当たりを重ね、250 Hz ハイパスで
低域を整理する。

打撃の強さの調整:
    実録音のプロファイル（BAND_PROFILE）は強めに打った音なので、
    StrikeParams の各ノブで「穏やかに指した音」へ寄せている。
    高域の傾き・立ち上がりの鋭さ・尾の長さ・音量を独立に調整でき、
    段階別のプリセットを PRESETS にまとめている。

マスタリングEQ:
    合成後に MASTER_EQ の 3 バンド EQ を掛け、ピークを MASTER_PEAK に揃える。
    フィルタはアプリ内の PieceSoundProcessor（ローシェルフ 2.5 kHz /
    ピーキング 5 kHz Q=1 / ハイシェルフ 9 kHz、RBJ 双二次）と同一なので、
    「アプリの駒音の設定で EQ を調整して気に入った値」をそのまま標準の音に
    焼き込める。2026-09-07 に採用した値: 低音 +1 / 中音 +10 / 高音 +7 dB。
"""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import sys
import wave

import numpy as np

SAMPLE_RATE = 44100
DURATION_SEC = 0.20


@dataclasses.dataclass(frozen=True)
class StrikeParams:
    """打撃の強さを決めるノブ。値を大きくするほど「強く打った音」に近づく。"""

    peak: float                 # 正規化後のピーク振幅（0.89 ≈ -1 dBFS, 0.25 ≈ -12 dBFS）
    high_tilt_db_per_oct: float # high_tilt_start_hz 以上を 1 オクターブごとに減衰（高域のキツさ。強打: 0）
    high_tilt_start_hz: float   # 高域減衰の開始周波数
    rise_ms: float              # 立ち上がりの時定数 ms（接触時間。長いほど柔らかい。強打: 0.05）
    attack_click: float         # 立ち上がりクリックの振幅（強打: 0.8）
    fast_tau_scale: float       # 速い成分の減衰時間の倍率（大きいほど立ち上がりが丸い。強打: 1.0）
    tail_level_db: float        # 尾のレベル補正 dB（強打: 0）
    tail_tau_scale: float       # 尾の減衰時間の倍率（強打: 1.0）
    bounce: float               # 二度当たりの強さ（強打: 0.3）
    drive: float                # ソフトサチュレーション量（1.0 で無効。強打: 1.2）


# 段階別プリセット（上から順に穏やかになる）
PRESETS: dict[str, StrikeParams] = {
    # 実録音プロファイルそのまま（強めに打った音）
    "strong": StrikeParams(0.89, 0.0, 4000.0, 0.05, 0.80, 1.0, 0.0, 1.00, 0.30, 1.2),
    # 「やや穏やか」（まだ強いとの指摘があり不採用）
    "soft1": StrikeParams(0.71, -4.0, 4000.0, 0.4, 0.25, 1.5, -2.0, 0.85, 0.20, 1.0),
    # 聴き比べで採用された穏やかさ（2026-09-07）
    "soft2": StrikeParams(0.50, -6.0, 4000.0, 0.6, 0.15, 1.8, -3.0, 0.85, 0.15, 1.0),
    "soft3": StrikeParams(0.40, -7.0, 3500.0, 0.8, 0.10, 2.0, -4.0, 0.80, 0.12, 1.0),
    "soft4": StrikeParams(0.32, -8.0, 3000.0, 1.0, 0.05, 2.2, -5.0, 0.80, 0.10, 1.0),
    "soft5": StrikeParams(0.25, -10.0, 2500.0, 1.4, 0.00, 2.5, -7.0, 0.75, 0.08, 1.0),
    "soft6": StrikeParams(0.20, -12.0, 2500.0, 2.0, 0.00, 3.0, -9.0, 0.70, 0.05, 1.0),
}

# 本番の駒音（resources/sounds/piece_move.wav）に使うプリセット
DEFAULT_PRESET = "soft2"

# マスタリングEQ（dB）。アプリ内 EQ で決めた値を焼き込む。すべて 0 なら無効
MASTER_EQ = {"low_db": 1.0, "mid_db": 10.0, "high_db": 7.0}
# マスタリングEQ 適用後のピーク上限（PieceSoundProcessor::kPeakLimit と同じ）
MASTER_PEAK = 0.89
# PieceSoundProcessor と同じフィルタ定数
EQ_LOW_SHELF_HZ = 2500.0
EQ_MID_PEAK_HZ = 5000.0
EQ_MID_PEAK_Q = 1.0
EQ_HIGH_SHELF_HZ = 9000.0

# --samples で聴き比べ用に書き出すプリセット（sample1 が最も強く、番号が進むほど穏やか）
SAMPLE_PRESETS = ["soft2", "soft3", "soft4", "soft5", "soft6"]

# 帯域別プロファイル: (下限 Hz, 上限 Hz, 初期レベル dB, 尾レベル dB, 速い成分 τ ms, 尾 τ ms)
# 初期レベルは打撃直後 5 ms の帯域パワー、尾レベルは遅い成分の t=0 外挿値。
# いずれも最も強い帯域（6.3〜8 kHz）を 0 dB とした相対値。尾が無い帯域は None。
BAND_PROFILE = [
    (250.0, 315.0, -23.0, None, 6.0, None),
    (315.0, 397.0, -23.0, None, 6.0, None),
    (397.0, 500.0, -20.0, None, 6.0, None),
    (500.0, 630.0, -19.0, None, 6.0, None),
    (630.0, 794.0, -18.0, None, 5.5, None),
    (794.0, 1000.0, -19.0, None, 5.5, None),
    (1000.0, 1260.0, -17.5, None, 5.0, None),
    (1260.0, 1587.0, -14.5, None, 5.0, None),
    (1587.0, 2000.0, -12.5, -34.0, 4.0, 7.0),
    (2000.0, 2520.0, -13.0, -18.0, 3.5, 12.0),
    (2520.0, 3175.0, -9.0, -17.0, 3.0, 30.0),
    (3175.0, 4000.0, -10.0, -15.0, 3.0, 45.0),
    (4000.0, 5040.0, -7.0, -12.0, 3.0, 50.0),
    (5040.0, 6350.0, -4.5, -10.0, 3.0, 60.0),
    (6350.0, 8000.0, 0.0, -5.5, 3.0, 60.0),
    (8000.0, 10079.0, -3.5, -11.0, 3.0, 60.0),
    (10079.0, 12699.0, -11.0, -17.0, 3.0, 60.0),
    (12699.0, 16000.0, -10.5, -14.0, 3.0, 55.0),
]


def _envelope(t: np.ndarray, onset: float, tau: float) -> np.ndarray:
    """onset 秒から始まる指数減衰エンベロープ。"""
    shifted = t - onset
    env = np.exp(-np.clip(shifted, 0.0, None) / tau)
    env[shifted < 0.0] = 0.0
    return env


def _click(t: np.ndarray, onset: float, width: float) -> np.ndarray:
    """onset 秒を中心とする幅 width 秒のクリック（ハン窓パルスの微分、低域を含まない）。"""
    rel = (t - onset) / width
    click = np.zeros_like(t)
    inside = np.abs(rel) < 0.5
    click[inside] = -np.sin(2.0 * np.pi * rel[inside])
    return click


def _biquad(x: np.ndarray, b0: float, b1: float, b2: float, a1: float, a2: float) -> np.ndarray:
    """双二次フィルタ（直接型 I、係数は a0 で正規化済み）。"""
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        y[i] = out
        x2, x1, y2, y1 = x1, v, y1, out
    return y


def _highpass(x: np.ndarray, cutoff_hz: float) -> np.ndarray:
    """2 次バターワース・ハイパス。"""
    w0 = 2.0 * np.pi * cutoff_hz / SAMPLE_RATE
    alpha = np.sin(w0) / (2.0 * np.sqrt(0.5))
    c = np.cos(w0)
    a0 = 1.0 + alpha
    return _biquad(x, (1.0 + c) / 2.0 / a0, -(1.0 + c) / a0, (1.0 + c) / 2.0 / a0,
                   -2.0 * c / a0, (1.0 - alpha) / a0)


# --- RBJ Audio EQ Cookbook（PieceSoundProcessor と同じ式） ---

def _low_shelf(x: np.ndarray, hz: float, gain_db: float) -> np.ndarray:
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * hz / SAMPLE_RATE
    c = np.cos(w0)
    alpha = np.sin(w0) / 2.0 * np.sqrt(2.0)  # S = 1
    sq = 2.0 * np.sqrt(A) * alpha
    a0 = (A + 1.0) + (A - 1.0) * c + sq
    return _biquad(x,
                   A * ((A + 1.0) - (A - 1.0) * c + sq) / a0,
                   2.0 * A * ((A - 1.0) - (A + 1.0) * c) / a0,
                   A * ((A + 1.0) - (A - 1.0) * c - sq) / a0,
                   -2.0 * ((A - 1.0) + (A + 1.0) * c) / a0,
                   ((A + 1.0) + (A - 1.0) * c - sq) / a0)


def _high_shelf(x: np.ndarray, hz: float, gain_db: float) -> np.ndarray:
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * hz / SAMPLE_RATE
    c = np.cos(w0)
    alpha = np.sin(w0) / 2.0 * np.sqrt(2.0)  # S = 1
    sq = 2.0 * np.sqrt(A) * alpha
    a0 = (A + 1.0) - (A - 1.0) * c + sq
    return _biquad(x,
                   A * ((A + 1.0) + (A - 1.0) * c + sq) / a0,
                   -2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0,
                   A * ((A + 1.0) + (A - 1.0) * c - sq) / a0,
                   2.0 * ((A - 1.0) - (A + 1.0) * c) / a0,
                   ((A + 1.0) - (A - 1.0) * c - sq) / a0)


def _peaking(x: np.ndarray, hz: float, q: float, gain_db: float) -> np.ndarray:
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * hz / SAMPLE_RATE
    c = np.cos(w0)
    alpha = np.sin(w0) / (2.0 * q)
    a0 = 1.0 + alpha / A
    return _biquad(x, (1.0 + alpha * A) / a0, -2.0 * c / a0, (1.0 - alpha * A) / a0,
                   -2.0 * c / a0, (1.0 - alpha / A) / a0)


def _master_eq(x: np.ndarray) -> np.ndarray:
    """MASTER_EQ を PieceSoundProcessor::apply と同じ順序・同じクリップ防止で適用する。"""
    low, mid, high = MASTER_EQ["low_db"], MASTER_EQ["mid_db"], MASTER_EQ["high_db"]
    if low == 0.0 and mid == 0.0 and high == 0.0:
        return x
    y = x.copy()
    if low != 0.0:
        y = _low_shelf(y, EQ_LOW_SHELF_HZ, low)
    if mid != 0.0:
        y = _peaking(y, EQ_MID_PEAK_HZ, EQ_MID_PEAK_Q, mid)
    if high != 0.0:
        y = _high_shelf(y, EQ_HIGH_SHELF_HZ, high)
    peak = np.max(np.abs(y))
    if peak > MASTER_PEAK:
        y *= MASTER_PEAK / peak
    return y


def _unit_band_noise(n: int, lo_hz: float, hi_hz: float,
                     rng: np.random.Generator) -> np.ndarray:
    """帯域制限した白色ノイズを RMS=1 に正規化して返す。"""
    noise = rng.standard_normal(n)
    spectrum = np.fft.rfft(noise)
    freqs = np.fft.rfftfreq(n, d=1.0 / SAMPLE_RATE)
    spectrum[(freqs < lo_hz) | (freqs >= hi_hz)] = 0.0
    band = np.fft.irfft(spectrum, n=n)
    return band / (np.sqrt(np.mean(band ** 2)) + 1e-12)


def _strike(t: np.ndarray, onset: float, strength: float, prm: StrikeParams,
            rng: np.random.Generator) -> np.ndarray:
    """1 回の接触音。BAND_PROFILE に従って帯域別ノイズを重ねる。"""
    out = np.zeros_like(t)
    window_sec = 0.005  # 初期レベルを測った窓幅
    # 立ち上がりの丸み: 接触時間に相当する短いランプ（高域の「割れ」を自然に抑える）
    rise = 1.0 - np.exp(-np.clip(t - onset, 0.0, None) / (prm.rise_ms / 1000.0))
    rise[t < onset] = 0.0
    for lo_hz, hi_hz, e0_db, tail_db, tau_fast_ms, tau_tail_ms in BAND_PROFILE:
        noise = _unit_band_noise(t.size, lo_hz, hi_hz, rng)

        # 穏やかさの調整: 高域の傾き
        center_hz = np.sqrt(lo_hz * hi_hz)
        tilt_db = 0.0
        if center_hz > prm.high_tilt_start_hz:
            tilt_db = prm.high_tilt_db_per_oct * np.log2(center_hz / prm.high_tilt_start_hz)
        e0 = 10.0 ** ((e0_db + tilt_db) / 10.0)

        env = np.zeros_like(t)
        tail_power = 0.0
        if tail_db is not None and tau_tail_ms is not None:
            tail_amp = 10.0 ** ((tail_db + tilt_db + prm.tail_level_db) / 20.0)
            tail_power = tail_amp ** 2
            env += tail_amp * _envelope(t, onset, tau_tail_ms * prm.tail_tau_scale / 1000.0)

        # 速い成分: 初期 5 ms 窓の平均パワーが (e0 - 尾) になるよう振幅を補正する
        tau_fast = tau_fast_ms * prm.fast_tau_scale / 1000.0
        fast_power = max(e0 - tail_power, 0.0)
        window_gain = (tau_fast / (2.0 * window_sec)) * (1.0 - np.exp(-2.0 * window_sec / tau_fast))
        fast_amp = np.sqrt(fast_power / window_gain)
        env += fast_amp * _envelope(t, onset, tau_fast)

        out += noise * env * rise

    # 立ち上がりのクリック（帯域ノイズだけでは鈍る打撃の瞬間を補う）
    out += prm.attack_click * _click(t, onset + 0.0003, width=0.0004)
    return strength * out


def synthesize(prm: StrikeParams) -> np.ndarray:
    rng = np.random.default_rng(20260907)
    n = int(SAMPLE_RATE * DURATION_SEC)
    t = np.arange(n, dtype=np.float64) / SAMPLE_RATE

    signal = _strike(t, onset=0.0, strength=1.0, prm=prm, rng=rng)
    signal += _strike(t, onset=0.007, strength=prm.bounce, prm=prm, rng=rng)  # 二度当たり

    # 低域の整理
    signal = _highpass(signal, 250.0)

    # ピークを 1 に揃えてから（必要なら）軽いソフトサチュレーション
    # （正規化前に通すと立ち上がりだけが強く潰れ、尾が相対的に大きくなる）
    signal /= np.max(np.abs(signal)) + 1e-12
    if prm.drive > 1.0:
        signal = np.tanh(prm.drive * signal) / np.tanh(prm.drive)

    # 末尾 30 ms をフェードアウト
    fade_len = int(SAMPLE_RATE * 0.030)
    fade = np.linspace(1.0, 0.0, fade_len)
    signal[-fade_len:] *= fade

    # DC 除去と正規化
    signal -= np.mean(signal)
    signal *= prm.peak / (np.max(np.abs(signal)) + 1e-12)

    # マスタリングEQ（アプリ内 EQ で決めた値を焼き込む）
    signal = _master_eq(signal)
    return signal


def write_wav(path: pathlib.Path, samples: np.ndarray) -> None:
    pcm = np.clip(np.round(samples * 32767.0), -32768, 32767).astype("<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm.tobytes())


def _describe(samples: np.ndarray) -> str:
    peak_dbfs = 20.0 * np.log10(np.max(np.abs(samples)) + 1e-12)
    rms_dbfs = 20.0 * np.log10(np.sqrt(np.mean(samples ** 2)) + 1e-12)
    return f"{samples.size} samples, {DURATION_SEC * 1000:.0f} ms, peak {peak_dbfs:.1f} dBFS, rms {rms_dbfs:.1f} dBFS"


def main(argv: list[str]) -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    default_out = repo_root / "resources" / "sounds" / "piece_move.wav"

    parser = argparse.ArgumentParser(description="駒音 WAV を合成する")
    parser.add_argument("output", nargs="?", default=str(default_out), help="出力 WAV パス")
    parser.add_argument("--preset", default=DEFAULT_PRESET, choices=sorted(PRESETS),
                        help=f"使用するプリセット（既定: {DEFAULT_PRESET}）")
    parser.add_argument("--peak-db", type=float, metavar="DBFS",
                        help="プリセットの音量（ピーク dBFS、例: -12）だけを上書きする")
    parser.add_argument("--samples", metavar="DIR",
                        help="聴き比べ用に SAMPLE_PRESETS の全プリセットを DIR に書き出す")
    args = parser.parse_args(argv[1:])

    if args.samples:
        out_dir = pathlib.Path(args.samples)
        for index, name in enumerate(SAMPLE_PRESETS, start=1):
            samples = synthesize(PRESETS[name])
            out = out_dir / f"piece_move_sample{index}_{name}.wav"
            write_wav(out, samples)
            print(f"wrote {out} ({_describe(samples)})")
        return 0

    prm = PRESETS[args.preset]
    if args.peak_db is not None:
        prm = dataclasses.replace(prm, peak=10.0 ** (args.peak_db / 20.0))
    samples = synthesize(prm)
    out = pathlib.Path(args.output)
    write_wav(out, samples)
    print(f"wrote {out} [{args.preset}] ({_describe(samples)})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
