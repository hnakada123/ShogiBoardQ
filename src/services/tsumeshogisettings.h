#ifndef TSUMESHOGISETTINGS_H
#define TSUMESHOGISETTINGS_H

/// @file tsumeshogisettings.h
/// @brief 詰将棋設定の永続化
///
/// 詰将棋局面生成ダイアログに関する設定を提供します。
/// 呼び出し元: tsumeshogigeneratordialog.cpp

#include <QString>
#include <QSize>

namespace TsumeshogiSettings {

/// ファイル保存で最後に使用したディレクトリ
QString tsumeshogiGeneratorLastSaveDirectory();
void setTsumeshogiGeneratorLastSaveDirectory(const QString& dir);

/// ダイアログのウィンドウサイズ（デフォルト: 600x550）
QSize tsumeshogiGeneratorDialogSize();
void setTsumeshogiGeneratorDialogSize(const QSize& size);

/// ダイアログのフォントサイズ（デフォルト: 10）
int tsumeshogiGeneratorFontSize();
void setTsumeshogiGeneratorFontSize(int size);

/// 最後に選択したエンジン番号（デフォルト: 0）
int tsumeshogiGeneratorEngineIndex();
void setTsumeshogiGeneratorEngineIndex(int index);

/// 目標手数（デフォルト: 3）
int tsumeshogiGeneratorTargetMoves();
void setTsumeshogiGeneratorTargetMoves(int moves);

/// 攻め駒上限（デフォルト: 4）
int tsumeshogiGeneratorMaxAttackPieces();
void setTsumeshogiGeneratorMaxAttackPieces(int count);

/// 守り駒上限（デフォルト: 1）
int tsumeshogiGeneratorMaxDefendPieces();
void setTsumeshogiGeneratorMaxDefendPieces(int count);

/// 配置範囲（デフォルト: 3）
int tsumeshogiGeneratorAttackRange();
void setTsumeshogiGeneratorAttackRange(int range);

/// 探索時間・秒（デフォルト: 5）
int tsumeshogiGeneratorTimeoutSec();
void setTsumeshogiGeneratorTimeoutSec(int sec);

/// 生成上限（デフォルト: 10, 0=無制限）
int tsumeshogiGeneratorMaxPositions();
void setTsumeshogiGeneratorMaxPositions(int count);

} // namespace TsumeshogiSettings

#endif // TSUMESHOGISETTINGS_H
