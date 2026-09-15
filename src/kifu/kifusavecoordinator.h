#ifndef KIFUSAVECOORDINATOR_H
#define KIFUSAVECOORDINATOR_H

/// @file kifusavecoordinator.h
/// @brief 棋譜保存コーディネータの定義


#include <QString>
#include <QStringList>
#include "playmode.h"   // 前方宣言をやめ、実体定義を取り込む

class QWidget;

namespace KifuSaveCoordinator {

/// 保存形式（保存先パスの拡張子から決定する）
enum class SaveFormat {
    Kif,   ///< .kif / .kifu（既定。未知の拡張子もこれ）
    Ki2,   ///< .ki2 / .ki2u
    Csa,   ///< .csa
    Jkf,   ///< .jkf
    Usen,  ///< .usen
    Usi,   ///< .usi
};

/// 保存先パスの拡張子から保存形式を判定する（大文字小文字は区別しない）
SaveFormat saveFormatForPath(const QString& path);

/// 保存先パスの拡張子が保存形式として認識できるか（.kif/.kifu/.ki2/.ki2u/.csa/.jkf/.usen/.usi）
bool hasKnownSaveExtension(const QString& path);

/// 保存先パスの拡張子が Shift_JIS 出力を要求するか（.kif / .ki2）
bool usesShiftJisForPath(const QString& path);

// ダイアログを出してKIF/KI2/CSA/JKF/USEN/USI形式で保存。成功時は保存パス、失敗/キャンセルは空文字。
QString saveViaDialogWithUsi(QWidget* parent,
                              const QStringList& kifLines,
                              const QStringList& ki2Lines,
                              const QStringList& csaLines,
                              const QStringList& jkfLines,
                              const QStringList& usenLines,
                              const QStringList& usiLines,
                              PlayMode mode,
                              const QString& human1,
                              const QString& human2,
                              const QString& engine1,
                              const QString& engine2,
                              bool hasBranches = false,
                              bool hasTimeInfo = false,
                              QString* outError = nullptr);

// 既存ファイルへ上書き保存。
// lines には saveFormatForPath(path) が返す形式の行を渡すこと（エンコーディングは拡張子で決まる）。
bool overwriteExisting(const QString& path,
                       const QStringList& lines,
                       QString* outError = nullptr);

} // namespace KifuSaveCoordinator

#endif // KIFUSAVECOORDINATOR_H
