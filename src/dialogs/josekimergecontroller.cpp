/// @file josekimergecontroller.cpp
/// @brief 定跡ウィンドウの「マージ」の実装

#include "josekimergecontroller.h"

#include "josekibookmergedialog.h"
#include "josekibooksession.h"
#include "josekifilecontroller.h"
#include "josekiimportdialog.h"
#include "josekimergedialog.h"
#include "josekimergeentries.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QWidget>
#include <functional>

JosekiMergeController::JosekiMergeController(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.session && m_deps.files && m_deps.parentWidget);
    connect(m_deps.session, &JosekiBookSession::busyChanged, this, &JosekiMergeController::onBusyChanged);
    connect(m_deps.session, &JosekiBookSession::mergeFinished, this, &JosekiMergeController::onMergeFinished);
    connect(m_deps.session, &JosekiBookSession::bookMerged, this, &JosekiMergeController::onBookMerged);
    connect(m_deps.session, &JosekiBookSession::bookMergeFailed, this, &JosekiMergeController::onBookMergeFailed);
    connect(m_deps.files, &JosekiFileController::bookSwitching, this, &JosekiMergeController::closeDialogs);
}

void JosekiMergeController::mergeFromCurrentKifu()
{
    if (m_deps.session->isBusy() || !m_deps.files->ensureFilePath()) return;
    emit kifuDataRequested();
}

void JosekiMergeController::setKifuData(const QStringList &sfenList, const QStringList &moveList,
                                        const QStringList &japaneseMoveList, int currentPly)
{
    if (m_deps.session->isBusy()) return;
    if (moveList.isEmpty()) {
        QMessageBox::information(m_deps.parentWidget, tr("情報"), tr("棋譜に指し手がありません。"));
        return;
    }
    openDialog(JosekiMergeEntries::fromLine(sfenList, moveList, japaneseMoveList, currentPly), QString());
}

void JosekiMergeController::mergeFromKifuFile()
{
    if (m_deps.session->isBusy() || !m_deps.files->ensureFilePath()) return;

    // 形式はファイルから判定する（棋譜を開くときと同じ形式を読める）
    const QString kifuFilePath = QFileDialog::getOpenFileName(
        m_deps.parentWidget, tr("棋譜ファイルを選択"), QDir::homePath(),
        tr("棋譜ファイル (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.json *.usi *.sfen *.usen);;すべてのファイル (*)"));
    if (kifuFilePath.isEmpty()) return;

    QList<KifuMergeEntry> entries;
    QString errorMessage;
    if (!JosekiMergeEntries::fromKifuFile(kifuFilePath, entries, &errorMessage)) {
        if (errorMessage.isEmpty())
            QMessageBox::information(m_deps.parentWidget, tr("情報"), tr("棋譜に指し手がありません。"));
        else
            QMessageBox::warning(m_deps.parentWidget, tr("エラー"),
                                 tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    openDialog(entries, tr("棋譜から定跡にマージ - %1").arg(QFileInfo(kifuFilePath).fileName()));
}

void JosekiMergeController::mergeFromKifuFiles()
{
    importPaths({});
}

void JosekiMergeController::importPaths(const QStringList &paths)
{
    if (m_deps.session->isBusy()) return;
    JosekiImportDialog dialog(m_deps.session, paths, m_deps.parentWidget);
    dialog.exec();
    // 登録した手には登録済みの印が付くので、開いているマージのダイアログにも配る
    const auto dialogs = m_deps.parentWidget->findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *mergeDialog : dialogs) mergeDialog->setRegisteredMoves(m_deps.session->mergeRegisteredMoves());
}

void JosekiMergeController::mergeFromBookFile()
{
    if (m_deps.session->isBusy()) return;
    const QString current = m_deps.session->filePath();
    JosekiBookMergeDialog dialog(current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath(),
                                 m_deps.parentWidget);
    if (dialog.exec() != QDialog::Accepted || m_deps.session->isBusy()) return;
    const QString filePath = dialog.filePath();
    if (!current.isEmpty() && QFileInfo(filePath) == QFileInfo(current)) {
        QMessageBox::information(m_deps.parentWidget, tr("情報"), tr("開いている定跡ファイルと同じファイルは足せません。"));
        return;
    }
    // 読み込みと足し合わせはワーカースレッドで行う。終わったら onBookMerged・onBookMergeFailed
    m_deps.session->startMergeBook(filePath, dialog.options());
}

void JosekiMergeController::onBookMerged(const QString &filePath, const JosekiBook::MergeStats &stats)
{
    QMessageBox::information(m_deps.parentWidget, tr("定跡ファイルを足す"),
                             tr("「%1」から、局面を %2 つと定跡手を %3 手足し、同じ指し手 %4 手を合わせました。\n"
                                "定跡ウィンドウで保存してください。")
                                 .arg(QFileInfo(filePath).fileName())
                                 .arg(stats.addedPositions)
                                 .arg(stats.addedMoves)
                                 .arg(stats.updatedMoves));
}

void JosekiMergeController::onBookMergeFailed(const QString &errorMessage)
{
    QMessageBox::warning(m_deps.parentWidget, tr("エラー"), tr("定跡ファイルを足せませんでした。\n%1").arg(errorMessage));
}

void JosekiMergeController::openDialog(const QList<KifuMergeEntry> &entries, const QString &title)
{
    if (entries.isEmpty()) {
        QMessageBox::information(m_deps.parentWidget, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }
    auto *dialog = new JosekiMergeDialog(m_deps.parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_deps.session->filePath());
    dialog->setRegisteredMoves(m_deps.session->mergeRegisteredMoves());
    dialog->setRegistrar(std::bind(&JosekiMergeController::registerEntries, this, std::placeholders::_1));
    if (!title.isEmpty()) dialog->setWindowTitle(title);
    dialog->setKifuData(entries);
    dialog->show();
}

bool JosekiMergeController::registerEntries(const QList<KifuMergeEntry> &entries)
{
    // 保存はワーカースレッドで行う。終わったら onMergeFinished（失敗は JosekiFileController が知らせる）
    return !m_deps.session->isBusy() && m_deps.session->startRegisterMergeMoves(entries);
}

void JosekiMergeController::onMergeFinished(bool registered)
{
    const auto dialogs = m_deps.parentWidget->findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *dialog : dialogs) {
        dialog->setRegisteredMoves(m_deps.session->mergeRegisteredMoves());
        dialog->finishRegistration(registered);
    }
}

void JosekiMergeController::onBusyChanged(bool busy)
{
    const auto dialogs = m_deps.parentWidget->findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *dialog : dialogs) dialog->setEnabled(!busy);
}

void JosekiMergeController::closeDialogs()
{
    // 別のファイルに替えた後は、古いダイアログからは登録させない
    const auto dialogs = m_deps.parentWidget->findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *dialog : dialogs) {
        dialog->setRegistrar({});
        dialog->setEnabled(false);
        dialog->reject();
    }
}
