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
int tsumePlayFontSize();
void setTsumePlayFontSize(int size);
int tsumeCollectionFontSize();
void setTsumeCollectionFontSize(int size);

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

/// ファイル保存・コピー時に詰み手順も出力するか（デフォルト: false）
bool tsumeshogiGeneratorIncludePv();
void setTsumeshogiGeneratorIncludePv(bool include);

} // namespace TsumeshogiSettings

#endif // TSUMESHOGISETTINGS_H
