/// @file editactions.cpp
/// @brief 編集操作の QAction の組の実装

#include "editactions.h"

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QToolButton>
#include <QWidget>

EditActions::EditActions(QWidget* host)
    : QObject(host)
{
    struct Spec {
        QString text;
        const char* icon;
        QKeySequence::StandardKey key;
    };
    const Spec specs[KindCount] = {
        {tr("元に戻す"), "editUndo", QKeySequence::Undo},
        {tr("やり直す"), "editRedo", QKeySequence::Redo},
        {tr("切り取り"), "editCut", QKeySequence::Cut},
        {tr("コピー"), "editCopy", QKeySequence::Copy},
        {tr("貼り付け"), "editPaste", QKeySequence::Paste},
    };
    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const Spec& spec = specs[i];
        auto* action = new QAction(QIcon(QStringLiteral(":/images/actions/%1.svg").arg(QLatin1String(spec.icon))),
                                   spec.text, host);
        action->setShortcuts(spec.key);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        const QKeySequence shortcut = action->shortcut();
        action->setToolTip(shortcut.isEmpty()
                               ? spec.text
                               : tr("%1 (%2)").arg(spec.text, shortcut.toString(QKeySequence::NativeText)));
        action->setEnabled(false);
        host->addAction(action);
        m_actions[i] = action;
    }
}

void EditActions::setEnabled(Kind kind, bool on)
{
    action(kind)->setEnabled(on);
}

QToolButton* EditActions::createButton(Kind kind, QWidget* parent) const
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action(kind));
    button->setAccessibleName(action(kind)->text());
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}
