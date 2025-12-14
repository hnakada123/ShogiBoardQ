#ifndef KIFUSAVECOORDINATOR_H
#define KIFUSAVECOORDINATOR_H

#include <QString>
#include <QStringList>
#include "playmode.h"   // ★ 前方宣言をやめ、実体定義を取り込む

class QWidget;

namespace KifuSaveCoordinator {

// ダイアログを出して保存。成功時は保存パス、失敗/キャンセルは空文字。
QString saveViaDialog(QWidget* parent,
                      const QStringList& kifuLines,
                      PlayMode mode,
                      const QString& human1,
                      const QString& human2,
                      const QString& engine1,
                      const QString& engine2,
                      QString* outError = nullptr);

// ダイアログを出してKIF/KI2形式で保存。成功時は保存パス、失敗/キャンセルは空文字。
// ki2Lines が空でなければ KI2形式も選択可能
QString saveViaDialogWithKi2(QWidget* parent,
                              const QStringList& kifLines,
                              const QStringList& ki2Lines,
                              PlayMode mode,
                              const QString& human1,
                              const QString& human2,
                              const QString& engine1,
                              const QString& engine2,
                              QString* outError = nullptr);

// 既存ファイルへ上書き保存
bool overwriteExisting(const QString& path,
                       const QStringList& kifuLines,
                       QString* outError = nullptr);

} // namespace KifuSaveCoordinator

#endif // KIFUSAVECOORDINATOR_H
