#ifndef TSUMESHOGISETTINGS_H
#define TSUMESHOGISETTINGS_H

/// @file tsumeshogisettings.h
/// @brief 詰将棋の設定の宣言
///
/// 詰将棋を解く画面・詰将棋問題一覧・詰将棋局面生成ダイアログの設定。

#include <QString>
#include <QSize>
#include <QtGlobal>
#include "boardsquaresize.h"
#include "setting.h"

namespace TsumeshogiSettings {

/// 詰将棋を解くダイアログの大きさ（閉じたときに DialogFontScale が保存する）
inline const Setting<QSize> playDialogSize{"TsumePlay/size", QSize(760, 760)};
/// 解く画面の判定時間は問題一覧の値（CollectionPreferences::timeoutSec）だけで、ここには持たない
struct PlayPreferences {
    int squareSize = 42;       // 盤のマスの幅（範囲は BoardSquareSize）
    bool boardRotated = false; // 攻方を下にした標準の向きから回転しているか

    /// 範囲に収めた値
    static PlayPreferences normalizedOf(PlayPreferences preferences)
    {
        preferences.squareSize = qBound(BoardSquareSize::kMin, preferences.squareSize, BoardSquareSize::kMax);
        return preferences;
    }
};
/// 詰将棋を解くダイアログの設定（既定値は PlayPreferences の既定値。マスの幅は範囲に収める）
inline const StructSetting<PlayPreferences> playPreferences{
    {SettingField<PlayPreferences>::of("TsumePlay/squareSize", &PlayPreferences::squareSize),
     SettingField<PlayPreferences>::of("TsumePlay/boardRotated", &PlayPreferences::boardRotated)},
    &PlayPreferences::normalizedOf};

/// 詰将棋問題一覧の大きさ（閉じたときに DialogFontScale が保存する）
inline const Setting<QSize> collectionDialogSize{"TsumeCollection/size", QSize(1100, 800)};
struct CollectionPreferences {
    /// 判定時間（秒）の範囲。問題一覧と、そこから開く解く画面の欄で共通
    static constexpr int kMinTimeoutSec = 1;
    static constexpr int kMaxTimeoutSec = 600;

    QString lastFile;
    int pageSize = 10;
    int page = 1;
    int filter = 0;
    int timeoutSec = 5;  ///< 判定時間（秒）。解く画面で変えた値も一覧に戻って保存する

    /// 範囲に収めた値
    static CollectionPreferences normalizedOf(CollectionPreferences preferences)
    {
        preferences.timeoutSec = qBound(kMinTimeoutSec, preferences.timeoutSec, kMaxTimeoutSec);
        return preferences;
    }
};
/// 詰将棋問題一覧の設定（既定値は CollectionPreferences の既定値。判定時間は範囲に収める）
inline const StructSetting<CollectionPreferences> collectionPreferences{
    {SettingField<CollectionPreferences>::of("TsumeCollection/file", &CollectionPreferences::lastFile),
     SettingField<CollectionPreferences>::of("TsumeCollection/pageSize", &CollectionPreferences::pageSize),
     SettingField<CollectionPreferences>::of("TsumeCollection/page", &CollectionPreferences::page),
     SettingField<CollectionPreferences>::of("TsumeCollection/filter", &CollectionPreferences::filter),
     SettingField<CollectionPreferences>::of("TsumeCollection/timeout", &CollectionPreferences::timeoutSec)},
    &CollectionPreferences::normalizedOf};

/// 問題一覧が最後に開いたファイル（CollectionPreferences::lastFile）。保存していなければ、以前の版が
/// 解く画面の設定（TsumePlay/file）に書いたファイル（読むだけ）
QString lastCollectionFile();

/// 詰将棋対局・問題一覧の文字サイズ（それぞれ独立、デフォルト: 10）
inline const Setting<int> tsumePlayFontSize{"FontSize/tsumePlay", 10};
inline const Setting<int> tsumeCollectionFontSize{"FontSize/tsumeCollection", 10};

/// ファイル保存で最後に使用したディレクトリ
inline const Setting<QString> tsumeshogiGeneratorLastSaveDirectory{"TsumeshogiGenerator/lastSaveDirectory", QString()};

/// ダイアログのウィンドウサイズ（デフォルト: 840x800）
inline const Setting<QSize> tsumeshogiGeneratorDialogSize{"SizeRelated/tsumeshogiGeneratorDialogSize", QSize(840, 800)};

/// ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> tsumeshogiGeneratorFontSize{"FontSize/tsumeshogiGenerator", 10};


/**
 * @brief 詰将棋生成の条件の範囲（ここだけで決める）
 *
 * 生成ダイアログの欄・設定の宣言・CLI の generate-tsume の引数（MCP の generate_tsume）・
 * TsumeshogiGenerator::Settings::normalized が使う。既定値は下の設定の宣言の既定値。
 */
struct GeneratorLimits {
    static constexpr int kMinTargetMoves = 1;
    static constexpr int kMaxTargetMoves = 99;
    static constexpr int kMinAttackPieces = 1;
    static constexpr int kMaxAttackPieces = 10;
    static constexpr int kMaxDefendPieces = 10;   ///< 下限は 0
    static constexpr int kMinAttackRange = 1;
    static constexpr int kMaxAttackRange = 8;
    static constexpr int kMinTimeoutSec = 1;
    static constexpr int kMaxTimeoutSec = 300;
    static constexpr int kMaxPositions = 10000;   ///< 下限の 0 は無制限（GUI だけ。CLI は終わるために 1 から）

    /// 目標手数として使えるか（範囲内の奇数。詰将棋は攻方の手で終わる）
    static constexpr bool isTargetMoves(int moves)
    {
        return moves >= kMinTargetMoves && moves <= kMaxTargetMoves && moves % 2 != 0;
    }
    /// 範囲に収めた奇数の目標手数（偶数は1つ小さい奇数。以前の版が偶数を保存した設定など）
    static constexpr int oddTargetMoves(int moves)
    {
        const int bounded = qBound(kMinTargetMoves, moves, kMaxTargetMoves);
        return bounded % 2 != 0 ? bounded : qMax(kMinTargetMoves, bounded - 1);
    }
};

/// 目標手数（デフォルト: 3。奇数）
inline const BoundedSetting<int> tsumeshogiGeneratorTargetMoves{
    "TsumeshogiGenerator/targetMoves", 3, GeneratorLimits::kMinTargetMoves, GeneratorLimits::kMaxTargetMoves};

/// 以前の版が保存した偶数の目標手数を、奇数に直して保存し直す（生成ダイアログが欄に読み込む前に呼ぶ。
/// CLI は引数の表の既定値を読むだけなので呼ばない）
void upgradeLegacyGeneratorTargetMoves();

/// 攻め駒上限（デフォルト: 4）
inline const BoundedSetting<int> tsumeshogiGeneratorMaxAttackPieces{
    "TsumeshogiGenerator/maxAttackPieces", 4, GeneratorLimits::kMinAttackPieces, GeneratorLimits::kMaxAttackPieces};

/// 守り駒上限（デフォルト: 1）
inline const BoundedSetting<int> tsumeshogiGeneratorMaxDefendPieces{
    "TsumeshogiGenerator/maxDefendPieces", 1, 0, GeneratorLimits::kMaxDefendPieces};

/// 配置範囲（デフォルト: 3）
inline const BoundedSetting<int> tsumeshogiGeneratorAttackRange{
    "TsumeshogiGenerator/attackRange", 3, GeneratorLimits::kMinAttackRange, GeneratorLimits::kMaxAttackRange};

/// 探索時間・秒（デフォルト: 5）
inline const BoundedSetting<int> tsumeshogiGeneratorTimeoutSec{
    "TsumeshogiGenerator/timeoutSec", 5, GeneratorLimits::kMinTimeoutSec, GeneratorLimits::kMaxTimeoutSec};

/// 生成上限（デフォルト: 10, 0=無制限）
inline const BoundedSetting<int> tsumeshogiGeneratorMaxPositions{
    "TsumeshogiGenerator/maxPositions", 10, 0, GeneratorLimits::kMaxPositions};

/// ファイル保存・コピー時に詰み手順も出力するか（デフォルト: false）
inline const Setting<bool> tsumeshogiGeneratorIncludePv{"TsumeshogiGenerator/includePv", false};

/// 生成条件の説明を展開して表示するか（デフォルト: false）
inline const Setting<bool> tsumeshogiGeneratorHelpExpanded{"TsumeshogiGenerator/helpExpanded", false};

/// 主手順の最終手に複数の詰手があっても採択するか（デフォルト: true）
inline const Setting<bool> tsumeshogiGeneratorAllowFinalMoveAlternatives{"TsumeshogiGenerator/allowFinalMoveAlternatives", true};

} // namespace TsumeshogiSettings

#endif // TSUMESHOGISETTINGS_H
