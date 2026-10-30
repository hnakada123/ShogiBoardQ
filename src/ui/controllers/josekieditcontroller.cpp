/// @file josekieditcontroller.cpp
/// @brief 定跡ウィンドウの定跡手の追加・編集・削除の実装

#include "josekieditcontroller.h"

#include "dialogutils.h"
#include "josekibooksession.h"
#include "josekimovedialog.h"
#include "josekitablemodel.h"

#include <QMessageBox>
#include <QWidget>

JosekiEditController::JosekiEditController(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.session && m_deps.parentWidget);
}

bool JosekiEditController::canEdit() const
{
    return m_deps.session->canEdit();
}

bool JosekiEditController::addMove(const QString &targetSfen)
{
    // 引数は表の局面（ダイアログを開いている間に変わる）を指すことがあるので写しを使う
    const QString sfen = targetSfen;
    if (!canEdit()) return false;
    if (sfen.isEmpty()) {
        QMessageBox::warning(m_deps.parentWidget, tr("定跡手追加"),
            tr("局面が設定されていません。\n将棋盤で局面を表示してから定跡手を追加してください。"));
        return false;
    }

    JosekiMoveDialog dialog(m_deps.parentWidget, false);
    dialog.setCurrentSfen(sfen);
    if (dialog.exec() != QDialog::Accepted || !canEdit()) return false;

    const JosekiMove newMove = dialog.entry();
    if (m_deps.session->hasMove(sfen, newMove.move)
        && !DialogUtils::confirmAction(m_deps.parentWidget, tr("確認"),
                tr("指し手「%1」は既に登録されています。\n上書きしますか？")
                    .arg(JosekiTableModel::moveText(newMove.move, sfen)),
                tr("上書きする"))) {
        return false;
    }
    m_deps.session->addMove(sfen, newMove);
    return true;
}

bool JosekiEditController::editMove(const QString &targetSfen, const JosekiMove &target)
{
    const QString sfen = targetSfen;
    const JosekiMove current = target;
    if (!canEdit()) return false;
    JosekiMoveDialog dialog(m_deps.parentWidget, true);
    dialog.setCurrentSfen(sfen);
    dialog.setEntry(current);
    dialog.setEditMoveDisplay(JosekiTableModel::moveText(current.move, sfen));
    if (dialog.exec() != QDialog::Accepted || !canEdit()) return false;
    return m_deps.session->updateMove(sfen, dialog.entry());
}

bool JosekiEditController::deleteMove(const QString &targetSfen, const QString &targetMove)
{
    const QString sfen = targetSfen;
    const QString usiMove = targetMove;
    if (!canEdit()) return false;
    if (!DialogUtils::confirmAction(m_deps.parentWidget, tr("削除確認"),
                                    tr("定跡手「%1」を削除しますか？")
                                        .arg(JosekiTableModel::moveText(usiMove, sfen)),
                                    tr("削除する"))
        || !canEdit()) {
        return false;
    }
    return m_deps.session->removeMove(sfen, usiMove);
}
