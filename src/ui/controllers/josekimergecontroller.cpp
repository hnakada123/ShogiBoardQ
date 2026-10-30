/// @file josekimergecontroller.cpp
/// @brief 定跡ウィンドウの「マージ」の実装

#include "josekimergecontroller.h"
#include "notificationpresenter.h"

#include "gamesettings.h"
#include "kifufilefilter.h"
#include "josekibookmergedialog.h"
#include "josekibooksession.h"
#include "josekiimportdialog.h"
#include "josekimergedialog.h"
#include "kifuloadparser.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QWidget>

JosekiMergeController::JosekiMergeController(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.session && m_deps.parentWidget);
    connect(m_deps.session, &JosekiBookSession::bookMerged, this, &JosekiMergeController::onBookMerged);
    connect(m_deps.session, &JosekiBookSession::bookMergeFailed, this, &JosekiMergeController::onBookMergeFailed);
}

void JosekiMergeController::openForLine(const RecordLine &line)
{
    if (!m_deps.session->canEdit()) return;
    openDialog(line, QString());
}

void JosekiMergeController::mergeFromKifuFile()
{
    if (!m_deps.session->canEdit()) return;

    // 形式はファイルから判定する（棋譜を開くときと同じ形式を読める）
    // 前回のフォルダは棋譜を開くときと同じ
    const QString kifuFilePath = QFileDialog::getOpenFileName(
        m_deps.parentWidget, tr("棋譜ファイルを選択"), GameSettings::lastKifuDirectory(), KifuFileFilter::forOpening());
    if (kifuFilePath.isEmpty()) return;
    GameSettings::lastKifuDirectory.set(QFileInfo(kifuFilePath).absolutePath());

    // 棋譜ファイルの本譜（棋譜を開くときと同じ読み込み）
    const KifuLoadResult loaded = KifuLoadParser::parseFile(kifuFilePath);
    if (!loaded.success) {
        NotificationPresenter::warning(m_deps.parentWidget, tr("エラー"), tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(loaded.error));
        return;
    }
    if (!m_deps.session->canEdit()) return;
    openDialog(loaded.record.mainline, tr("棋譜から定跡にマージ - %1").arg(QFileInfo(kifuFilePath).fileName()));
}

void JosekiMergeController::importPaths(const QStringList &paths)
{
    if (!m_deps.session->canEdit()) return;
    JosekiImportDialog dialog(m_deps.session, paths, m_deps.parentWidget);
    dialog.exec();
}

void JosekiMergeController::mergeFromBookFile()
{
    if (!m_deps.session->canEdit()) return;
    const QString current = m_deps.session->filePath();
    JosekiBookMergeDialog dialog(current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath(),
                                 m_deps.parentWidget);
    if (dialog.exec() != QDialog::Accepted || !m_deps.session->canEdit()) return;
    const QString filePath = dialog.filePath();
    if (!current.isEmpty() && QFileInfo(filePath) == QFileInfo(current)) {
        NotificationPresenter::information(m_deps.parentWidget, tr("情報"), tr("開いている定跡ファイルと同じファイルは足せません。"));
        return;
    }
    // 読み込みと足し合わせはワーカースレッドで行う。終わったら onBookMerged・onBookMergeFailed
    m_deps.session->startMergeBook(filePath, dialog.options());
}

void JosekiMergeController::onBookMerged(const QString &filePath, const JosekiBook::MergeStats &stats)
{
    NotificationPresenter::information(m_deps.parentWidget, tr("定跡ファイルを足す"),
                                       tr("「%1」から、局面を %2 つと定跡手を %3 手足し、同じ指し手 %4 手を合わせました。\n"
                                          "定跡ウィンドウで保存してください。")
                                           .arg(QFileInfo(filePath).fileName())
                                           .arg(stats.addedPositions)
                                           .arg(stats.addedMoves)
                                           .arg(stats.updatedMoves));
}

void JosekiMergeController::onBookMergeFailed(const QString &errorMessage)
{
    NotificationPresenter::warning(m_deps.parentWidget, tr("エラー"), tr("定跡ファイルを足せませんでした。\n%1").arg(errorMessage));
}

void JosekiMergeController::openDialog(const RecordLine &line, const QString &title)
{
    if (line.moveCount() == 0) {
        NotificationPresenter::information(m_deps.parentWidget, tr("情報"), tr("棋譜に指し手がありません。"));
        return;
    }
    auto *dialog = new JosekiMergeDialog(m_deps.session, line, m_deps.parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (!title.isEmpty()) dialog->setWindowTitle(title);
    dialog->show();
}
