#ifndef PIECESOUNDPLAYER_H
#define PIECESOUNDPLAYER_H

/// @file piecesoundplayer.h
/// @brief 駒音（駒を指したときの効果音）再生サービスの定義

#include <QObject>
#include <QString>

#include "piecesoundtone.h"

class QSoundEffect;

/**
 * @brief 駒音を再生するサービス
 *
 * 責務:
 * - 駒音リソース（qrc:/sounds/piece_move.wav）の読み込みと再生
 * - 駒音の有効/無効・音量・音質（音の高さ・3バンドEQ）の保持と AppSettings への永続化
 * - 音質が既定値以外のときは PieceSoundProcessor で加工した WAV をキャッシュ
 *   ディレクトリに書き出し、それを再生する
 *
 * 着手が盤面に反映されたタイミングで playMoveSound() を呼ぶ。
 * 接続元:
 * - ShogiGameController::moveCommitted（人間・ローカルエンジンの着手）
 * - CsaGameWiring::moveAppliedToBoard（通信対局の相手・自エンジンの着手）
 *
 * 無効のあいだは QSoundEffect を生成しないため、オーディオデバイスを開かない。
 * 音量は 0〜100 のパーセント値で扱い、対数スケールで QSoundEffect の振幅に変換する。
 */
class PieceSoundPlayer : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMinVolume = 0;
    static constexpr int kMaxVolume = 100;
    /// 既定の音量（%）。標準の駒音（piece_move.wav）はこの音量で聴くことを前提に調整されている
    static constexpr int kDefaultVolume = 30;

    explicit PieceSoundPlayer(QObject* parent = nullptr);

    /// 駒音が有効かどうか
    bool isEnabled() const { return m_enabled; }

    /// 音量（0〜100 %）
    int volume() const { return m_volume; }

    /// 音質パラメータ
    PieceSoundTone tone() const { return m_tone; }

public slots:
    /// 駒音の有効/無効を切り替え、設定に保存する
    void setEnabled(bool enabled);

    /// 音量（0〜100 %）を設定し、設定に保存する
    void setVolume(int percent);

    /// 音質パラメータを設定し、設定に保存して再生波形を作り直す
    void setTone(const PieceSoundTone& tone);

    /// 駒音を再生する（無効時は何もしない）
    void playMoveSound();

    /// 有効/無効にかかわらず駒音を再生する（設定ダイアログの試聴用）
    void preview();

signals:
    /// 有効/無効が切り替わった
    void enabledChanged(bool enabled);

    /// 音量が変わった
    void volumeChanged(int percent);

    /// 音質が変わった
    void toneChanged(const PieceSoundTone& tone);

private:
    /// 効果音オブジェクトを遅延生成し、リソースの読み込みを開始する
    void ensureEffect();

    /// 現在の音量を効果音オブジェクトに反映する
    void applyVolume();

    /// 現在の音質に応じた再生元（元リソース or 加工済みファイル）を効果音に設定する
    void updateSource();

    /// 現在の音質で加工した WAV をキャッシュに書き出し、そのパスを返す（失敗時は空）
    QString renderProcessedFile() const;

    /// 効果音を再生する（有効/無効は見ない）
    void playNow();

    QSoundEffect* m_effect = nullptr;  ///< 効果音（親=this、遅延生成）
    bool m_enabled;                    ///< 有効フラグ（設定から初期化）
    int m_volume;                      ///< 音量 0〜100 %（設定から初期化）
    PieceSoundTone m_tone;             ///< 音質（設定から初期化）
};

#endif // PIECESOUNDPLAYER_H
