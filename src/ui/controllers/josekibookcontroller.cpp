/// @file josekibookcontroller.cpp
/// @brief 定跡ウィンドウからの定跡の操作の実装

#include "josekibookcontroller.h"

#include "josekibooksession.h"
#include "josekieditcontroller.h"
#include "josekifilecontroller.h"
#include "josekimergecontroller.h"
#include "kifunavigationstate.h"

#include <QDockWidget>
#include <QEvent>

JosekiBookController::JosekiBookController(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.session && m_deps.window && m_deps.navState);
    JosekiWindow *window = m_deps.window;
    m_files = new JosekiFileController({m_deps.session, window}, this);
    m_merge = new JosekiMergeController({m_deps.session, window}, this);
    m_edit = new JosekiEditController({m_deps.session, window}, this);

    connect(window, &JosekiWindow::newBookRequested, m_files, &JosekiFileController::newBook);
    connect(window, &JosekiWindow::openBookRequested, m_files, &JosekiFileController::open);
    connect(window, &JosekiWindow::saveBookRequested, m_files, &JosekiFileController::save);
    connect(window, &JosekiWindow::saveBookAsRequested, m_files, &JosekiFileController::saveAs);
    connect(window, &JosekiWindow::recentFileRequested, m_files, &JosekiFileController::openRecent);
    connect(window, &JosekiWindow::clearRecentFilesRequested, m_files, &JosekiFileController::clearRecentFiles);
    connect(m_files, &JosekiFileController::statusMessage, window, &JosekiWindow::showTransientStatus);

    connect(window, &JosekiWindow::addMoveRequested, m_edit, &JosekiEditController::addMove);
    connect(window, &JosekiWindow::editMoveRequested, m_edit, &JosekiEditController::editMove);
    connect(window, &JosekiWindow::deleteMoveRequested, m_edit, &JosekiEditController::deleteMove);

    connect(window, &JosekiWindow::mergeRequested, this, &JosekiBookController::onMergeRequested);
    // ドロップの処理の中ではダイアログを開かない（ドラッグ元のアプリを待たせない）
    connect(window, &JosekiWindow::pathsDropped, this, &JosekiBookController::openDroppedPaths, Qt::QueuedConnection);
    connect(m_deps.session, &JosekiBookSession::modifiedChanged, this, &JosekiBookController::onModifiedChanged);
}

JosekiBookController::~JosekiBookController() = default;

void JosekiBookController::onMergeRequested(JosekiWindow::MergeSource source)
{
    using Source = JosekiWindow::MergeSource;
    switch (source) {
    case Source::CurrentKifu:
        // 表示中の手順（分岐を選んでいればその分岐）
        m_merge->openForLine(m_deps.navState->currentRecordLine());
        break;
    case Source::KifuFile: m_merge->mergeFromKifuFile(); break;
    case Source::KifuFiles: m_merge->importPaths({}); break;
    case Source::BookFile: m_merge->mergeFromBookFile(); break;
    }
}

void JosekiBookController::openDroppedPaths(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    // 定跡ファイル（.db）1つなら開き、それ以外は複数の棋譜から登録するダイアログに並べる
    if (paths.size() == 1 && JosekiBookFile::hasBookSuffix(paths.constFirst())) {
        m_files->openRecent(paths.constFirst());
        return;
    }
    m_merge->importPaths(paths);
}

bool JosekiBookController::confirmClose()
{
    return m_files->confirmDiscardChanges();
}

void JosekiBookController::attachDock(QDockWidget *dock)
{
    m_dock = dock;
    if (!dock) return;
    m_dockTitle = dock->windowTitle();
    dock->installEventFilter(this);
    onModifiedChanged(m_deps.session->isModified());
}

bool JosekiBookController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_dock && event->type() == QEvent::Close) {
        if (!confirmClose()) {
            event->ignore();
            return true;
        }
        // 閉じても定跡は盤の矢印と対局で使い続けるので、「破棄」を選んだ変更は保存した内容に戻す
        m_files->revertChanges();
    }
    return QObject::eventFilter(watched, event);
}

void JosekiBookController::onModifiedChanged(bool modified)
{
    if (m_dock) m_dock->setWindowTitle(modified ? m_dockTitle + QStringLiteral(" *") : m_dockTitle);
}
