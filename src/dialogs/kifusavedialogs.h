#ifndef KIFUSAVEDIALOGS_H
#define KIFUSAVEDIALOGS_H

/// @file kifusavedialogs.h
/// @brief 棋譜の保存ダイアログ・未保存の確認

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <functional>

#include "kifuexport.h"
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
    /// 保存する棋譜を format で書くと失うもの（KifuExport::lossIn。空の関数なら何も失わない）
    using LossOf = std::function<KifuExport::Loss(KifuFormat)>;

    /// 未保存変更がある場合に保存・破棄・キャンセルを確認する。
    /// 保存失敗・キャンセルは false。破棄しても呼び出し元の dirty 状態は変更しない。
    static bool confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save);

    /// ダイアログを出して保存する（形式と文字コードは選んだ拡張子。KifuFormats::saveTargets）。
    /// 選んだ形式で失うもの（lossOf）があれば、保存するかを確認する。
    /// 成功時は保存パス、失敗/キャンセルは空文字。defaultFileName は前回保存したフォルダに置く既定のファイル名
    static QString saveViaDialog(QWidget* parent,
                                 const KifuSaveFile::LineGenerator& generateLines,
                                 const QString& defaultFileName,
                                 const LossOf& lossOf,
                                 QString* outError = nullptr);

    /// 情報が失われる形式への保存を確認する。続行なら true（「名前を付けて保存」と上書き保存で共通）
    static bool confirmLossySave(QWidget* parent, KifuFormat format, const KifuExport::Loss& loss);
    /// 失うものの一覧の文（「分岐、消費時間、コメント」。保存の確認とコピーの知らせで共通）
    static QString lostItemsText(const KifuExport::Loss& loss);
    /// 知らせの文に書く形式の名前（"KI2"・"USEN" など）
    static QString formatName(KifuFormat format);

    /// 既存ファイルへ上書き保存する（行は拡張子の形式と文字コードで generateLines に作らせる）。.kif/.ki2 に
    /// Shift_JIS で表せない文字があれば UTF-8 で保存するか確認する（キャンセル時は outError が空で false）。
    /// parent が nullptr なら確認せずエラーにする。
    static bool overwriteExisting(QWidget* parent, const QString& path, const KifuSaveFile::LineGenerator& generateLines,
                                  QString* outError = nullptr);

private:
    /// Shift_JIS で表せない文字があるとき、UTF-8 で保存するか確認する。保存するなら true
    static bool confirmUtf8Fallback(QWidget* parent, const QString& characters);
};

#endif // KIFUSAVEDIALOGS_H
