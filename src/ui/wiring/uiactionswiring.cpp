/// @file uiactionswiring.cpp
/// @brief メニュー/ツールバーアクションのシグナル/スロット接続ファサードの実装

#include "uiactionswiring.h"
#include "fileactionswiring.h"
#include "gameactionswiring.h"
#include "editactionswiring.h"
#include "viewactionswiring.h"
#include "mainwindow.h"
#include "logcategories.h"

void UiActionsWiring::wire()
{
    auto* mw = qobject_cast<MainWindow*>(m_d.ctx);
    if (Q_UNLIKELY(!m_d.ui) || Q_UNLIKELY(!mw)) {
        qCWarning(lcUi, "UiActionsWiring::wire(): required dependency is null (ui=%p, mw=%p)",
                  static_cast<void*>(m_d.ui), static_cast<void*>(mw));
        return;
    }

    if (!m_fileWiring) {
        m_fileWiring = new FileActionsWiring({m_d.ui, mw, m_d.dlw, m_d.kfc}, this);
        m_gameWiring = new GameActionsWiring({m_d.ui, mw, m_d.dlw, m_d.dcw}, this);
        m_editWiring = new EditActionsWiring({m_d.ui, mw, mw->kifuExportController(), m_d.dlw, m_d.kfc}, this);
        m_viewWiring = new ViewActionsWiring({m_d.ui, mw, m_d.dlw}, this);
    }

    m_fileWiring->wire();
    m_gameWiring->wire();
    m_editWiring->wire();
    m_viewWiring->wire();
}
