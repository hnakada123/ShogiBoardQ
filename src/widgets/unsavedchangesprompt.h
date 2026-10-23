#ifndef UNSAVEDCHANGESPROMPT_H
#define UNSAVEDCHANGESPROMPT_H

/// @file unsavedchangesprompt.h
/// @brief 未保存の変更を保存・破棄・キャンセルのどれにするか尋ねる確認（棋譜・定跡で共通）

#include <QCoreApplication>
#include <QString>

class QWidget;

/**
 * @brief 未保存の変更の確認（警告のアイコン、ボタンは「保存」「破棄」「キャンセル」、既定は保存）
 *
 * 棋譜（KifuSaveDialogs::confirmDiscardUnsaved）と定跡（JosekiFileController::confirmDiscardChanges）で
 * 同じ見た目にする。保存するかどうかの後の処理は呼び出し側が行う。
 */
class UnsavedChangesPrompt
{
    Q_DECLARE_TR_FUNCTIONS(UnsavedChangesPrompt)

public:
    enum class Choice { Save, Discard, Cancel };

    /**
     * @param title 題名（「未保存の棋譜」など）
     * @param text 問いの文
     * @param detail 補足（どのファイルかなど。空なら出さない）
     */
    static Choice ask(QWidget* parent, const QString& title, const QString& text, const QString& detail = QString());
};

#endif // UNSAVEDCHANGESPROMPT_H
