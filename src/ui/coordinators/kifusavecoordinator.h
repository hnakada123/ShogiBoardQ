#ifndef KIFUSAVECOORDINATOR_H
#define KIFUSAVECOORDINATOR_H

/// @file kifusavecoordinator.h
/// @brief 棋譜の保存ダイアログ・未保存の確認

#include <QString>
#include <QStringList>
#include <functional>

#include "kifusavefile.h"

class QWidget;

/**
 * @brief 棋譜の保存で利用者に聞くこと（保存先・情報の欠ける形式・Shift_JIS で表せない文字・未保存の変更）
 *
 * 保存形式の判定と書き出しは KifuSaveFile が行う。
 */
namespace KifuSaveCoordinator {

/// 未保存変更がある場合に保存・破棄・キャンセルを確認する。
/// 保存失敗・キャンセルは false。破棄しても呼び出し元の dirty 状態は変更しない。
bool confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save);

/// ダイアログを出してKIF/KI2/CSA/JKF/USEN/USI形式で保存。成功時は保存パス、失敗/キャンセルは空文字。
/// defaultFileName は前回保存したフォルダに置く既定のファイル名
QString saveViaDialog(QWidget* parent,
                      const KifuSaveFile::LineGenerator& generateLines,
                      const QString& defaultFileName,
                      bool hasBranches = false,
                      bool hasTimeInfo = false,
                      QString* outError = nullptr);

/// 既存ファイルへ上書き保存する。.kif/.ki2 に Shift_JIS で表せない文字があれば
/// UTF-8 で保存するか確認する（キャンセル時は outError が空で false）。parent が nullptr なら確認せずエラーにする。
bool overwriteExisting(QWidget* parent, const QString& path, const QStringList& lines, QString* outError = nullptr);

} // namespace KifuSaveCoordinator

#endif // KIFUSAVECOORDINATOR_H
