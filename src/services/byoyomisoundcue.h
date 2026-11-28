#ifndef BYOYOMISOUNDCUE_H
#define BYOYOMISOUNDCUE_H

/// @file byoyomisoundcue.h
/// @brief 秒読み音声の音の種類

/// 秒読み音声で鳴らす音の種類。鳴らす側（ui の ByoyomiSoundPlayer）と、試聴を頼む設定ダイアログ（dialogs）で共通
enum class ByoyomiSoundCue {
    None,     ///< 鳴らさない
    Notice,   ///< 予告音（残り 30・20・10 秒、または秒読み突入時）
    Tick,     ///< カウント音（残り 9〜6 秒）
    Urgent,   ///< 切迫カウント音（残り 5〜1 秒）
    Timeout,  ///< 時間切れ音
};

#endif // BYOYOMISOUNDCUE_H
