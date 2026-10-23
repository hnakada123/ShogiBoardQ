/// @file unsavedchangesprompt.cpp
/// @brief 未保存の変更の確認の実装

#include "unsavedchangesprompt.h"

#include <QAbstractButton>
#include <QMessageBox>

UnsavedChangesPrompt::Choice UnsavedChangesPrompt::ask(QWidget* parent, const QString& title, const QString& text,
                                                       const QString& detail)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                    parent);
    if (!detail.isEmpty()) box.setInformativeText(detail);
    box.button(QMessageBox::Save)->setText(tr("保存"));
    box.button(QMessageBox::Discard)->setText(tr("破棄"));
    box.button(QMessageBox::Cancel)->setText(tr("キャンセル"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);
    switch (box.exec()) {
    case QMessageBox::Save:
        return Choice::Save;
    case QMessageBox::Discard:
        return Choice::Discard;
    default:
        return Choice::Cancel;
    }
}
