#ifndef KIFUSAVEDIALOGS_H
#define KIFUSAVEDIALOGS_H

/// @file kifusavedialogs.h
/// @brief 棋譜の保存ダイアログ・未保存の確認

#include <QCoreApplication>
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
class KifuSaveDialogs
{
    Q_DECLARE_TR_FUNCTIONS(KifuSaveDialogs)

public:
    /// 未保存変更がある場合に保存・破棄・キャンセルを確認する。
    /// 保存失敗・キャンセルは false。破棄しても呼び出し元の dirty 状態は変更しない。
    static bool confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save);

    /// ダイアログを出してKIF/KI2/CSA/JKF/USEN/USI形式で保存。成功時は保存パス、失敗/キャンセルは空文字。
    /// defaultFileName は前回保存したフォルダに置く既定のファイル名
    static QString saveViaDialog(QWidget* parent,
                                 const KifuSaveFile::LineGenerator& generateLines,
                                 const QString& defaultFileName,
                                 bool hasBranches = false,
                                 bool hasTimeInfo = false,
                                 QString* outError = nullptr);

    /// 既存ファイルへ上書き保存する。.kif/.ki2 に Shift_JIS で表せない文字があれば
    /// UTF-8 で保存するか確認する（キャンセル時は outError が空で false）。parent が nullptr なら確認せずエラーにする。
    static bool overwriteExisting(QWidget* parent, const QString& path, const QStringList& lines, QString* outError = nullptr);

private:
    /// Shift_JIS で表せない文字があるとき、UTF-8 で保存するか確認する。保存するなら true
    static bool confirmUtf8Fallback(QWidget* parent, const QString& characters);
    /// 情報が失われる形式への保存を確認する。続行なら true
    static bool confirmLossySave(QWidget* parent, const QString& title, const QString& message);
};

#endif // KIFUSAVEDIALOGS_H
