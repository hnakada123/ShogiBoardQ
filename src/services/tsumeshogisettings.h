#ifndef TSUMESHOGISETTINGS_H
#define TSUMESHOGISETTINGS_H

/// @file tsumeshogisettings.h
/// @brief 詰将棋設定の永続化
///
/// 詰将棋局面生成ダイアログに関する設定を提供します。
/// 呼び出し元: tsumeshogigeneratordialog.cpp

#include <QString>
#include <QSize>
#include "setting.h"

namespace TsumeshogiSettings {

struct PlayPreferences {
    QSize size{760, 760};
    QString lastFile;
    int problemIndex = 0;
    int timeoutSec = 5;
    int squareSize = 42;
    bool boardRotated = false; // 攻方を下にした標準の向きから回転しているか
};
PlayPreferences playPreferences();
void setPlayPreferences(const PlayPreferences& preferences);

struct CollectionPreferences {
    QSize size{1100, 800};
    QString lastFile;
    QString enginePath;
    int pageSize = 10;
    int page = 1;
    int filter = 0;
    int timeoutSec = 5;
};
CollectionPreferences collectionPreferences();
void setCollectionPreferences(const CollectionPreferences& preferences);

/// 詰将棋対局・問題一覧の文字サイズ（それぞれ独立、デフォルト: 10）
inline const Setting<int> tsumePlayFontSize{"FontSize/tsumePlay", 10};
inline const Setting<int> tsumeCollectionFontSize{"FontSize/tsumeCollection", 10};

/// ファイル保存で最後に使用したディレクトリ
inline const Setting<QString> tsumeshogiGeneratorLastSaveDirectory{"TsumeshogiGenerator/lastSaveDirectory", QString()};

/// ダイアログのウィンドウサイズ（デフォルト: 840x800）
inline const Setting<QSize> tsumeshogiGeneratorDialogSize{"SizeRelated/tsumeshogiGeneratorDialogSize", QSize(840, 800)};

/// ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> tsumeshogiGeneratorFontSize{"FontSize/tsumeshogiGenerator", 10};


/// 目標手数（デフォルト: 3）
inline const Setting<int> tsumeshogiGeneratorTargetMoves{"TsumeshogiGenerator/targetMoves", 3};

/// 攻め駒上限（デフォルト: 4）
inline const Setting<int> tsumeshogiGeneratorMaxAttackPieces{"TsumeshogiGenerator/maxAttackPieces", 4};

/// 守り駒上限（デフォルト: 1）
inline const Setting<int> tsumeshogiGeneratorMaxDefendPieces{"TsumeshogiGenerator/maxDefendPieces", 1};

/// 配置範囲（デフォルト: 3）
inline const Setting<int> tsumeshogiGeneratorAttackRange{"TsumeshogiGenerator/attackRange", 3};

/// 探索時間・秒（デフォルト: 5）
inline const Setting<int> tsumeshogiGeneratorTimeoutSec{"TsumeshogiGenerator/timeoutSec", 5};

/// 生成上限（デフォルト: 10, 0=無制限）
inline const Setting<int> tsumeshogiGeneratorMaxPositions{"TsumeshogiGenerator/maxPositions", 10};

/// ファイル保存・コピー時に詰み手順も出力するか（デフォルト: false）
inline const Setting<bool> tsumeshogiGeneratorIncludePv{"TsumeshogiGenerator/includePv", false};

/// 生成条件の説明を展開して表示するか（デフォルト: false）
inline const Setting<bool> tsumeshogiGeneratorHelpExpanded{"TsumeshogiGenerator/helpExpanded", false};

/// 主手順の最終手に複数の詰手があっても採択するか（デフォルト: true）
inline const Setting<bool> tsumeshogiGeneratorAllowFinalMoveAlternatives{"TsumeshogiGenerator/allowFinalMoveAlternatives", true};

} // namespace TsumeshogiSettings

#endif // TSUMESHOGISETTINGS_H
