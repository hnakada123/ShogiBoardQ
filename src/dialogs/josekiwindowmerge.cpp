/// @file josekiwindowmerge.cpp
/// @brief JosekiWindow の棋譜からのマージ（マージダイアログの表示と登録）

#include "josekiwindow.h"
#include "josekipresenter.h"
#include "josekirepository.h"
#include "josekimergedialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

// ============================================================
// マージ
// ============================================================

void JosekiWindow::onMergeFromCurrentKifu()
{
    if (isIoBusy()) return;
    if (!ensureFilePath()) return;
    emit requestKifuDataForMerge();
}

void JosekiWindow::setKifuDataForMerge(const QStringList &sfenList, const QStringList &moveList,
                                        const QStringList &japaneseMoveList, int currentPly)
{
    if (isIoBusy()) return;
    if (moveList.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        return;
    }
    QList<KifuMergeEntry> entries = m_presenter->buildMergeEntries(sfenList, moveList, japaneseMoveList, currentPly);
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }

    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_currentFilePath);
    dialog->setRegisteredMoves(m_repository->mergeRegisteredMoves());
    connect(dialog, &JosekiMergeDialog::registerMove, this, &JosekiWindow::onMergeRegisterMove);
    connect(dialog, &JosekiMergeDialog::registerAllMoves, this, &JosekiWindow::onMergeRegisterAllMoves);
    connect(this, &JosekiWindow::mergeRegistrationFinished, dialog, &JosekiMergeDialog::onRegistrationFinished);
    dialog->setKifuData(entries);
    dialog->show();
}

void JosekiWindow::onMergeFromKifuFile()
{
    if (isIoBusy()) return;
    if (!ensureFilePath()) return;

    QString kifFilePath = QFileDialog::getOpenFileName(
        this, tr("棋譜ファイルを選択"), QDir::homePath(),
        tr("KIF形式 (*.kif *.kifu);;すべてのファイル (*)"));
    if (kifFilePath.isEmpty()) return;

    QList<KifuMergeEntry> entries;
    QString errorMessage;
    if (!m_presenter->buildMergeEntriesFromKifFile(kifFilePath, entries, &errorMessage)) {
        if (errorMessage.isEmpty())
            QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        else
            QMessageBox::warning(this, tr("エラー"), tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }

    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_currentFilePath);
    dialog->setRegisteredMoves(m_repository->mergeRegisteredMoves());
    dialog->setWindowTitle(tr("棋譜から定跡にマージ - %1").arg(QFileInfo(kifFilePath).fileName()));
    connect(dialog, &JosekiMergeDialog::registerMove, this, &JosekiWindow::onMergeRegisterMove);
    connect(dialog, &JosekiMergeDialog::registerAllMoves, this, &JosekiWindow::onMergeRegisterAllMoves);
    connect(this, &JosekiWindow::mergeRegistrationFinished, dialog, &JosekiMergeDialog::onRegistrationFinished);
    dialog->setKifuData(entries);
    dialog->show();
}

void JosekiWindow::onMergeRegisterAllMoves(const QList<KifuMergeEntry> &entries)
{
    if (isIoBusy() || entries.isEmpty()) return;
    QStringList sfens, sfensWithPly, usiMoves;
    for (const KifuMergeEntry &entry : entries) {
        sfens << JosekiPresenter::normalizeSfen(entry.sfen);
        sfensWithPly << entry.sfen;
        usiMoves << entry.usiMove;
    }
    QString errorMessage;
    const bool success = m_presenter->registerMergeMoves(sfens, sfensWithPly, usiMoves, m_currentFilePath, &errorMessage);
    if (success) {
        for (qsizetype i = 0; i < usiMoves.size(); ++i) emit mergeRegistrationFinished(sfens.at(i), usiMoves.at(i), true);
    } else {
        emit mergeRegistrationFinished(sfens.first(), usiMoves.first(), false);
        QMessageBox::warning(this, tr("エラー"), errorMessage);
    }
    updateJosekiDisplay();
}

void JosekiWindow::onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove)
{
    if (isIoBusy()) return;
    QString errorMessage;
    const bool success = m_presenter->registerMergeMove(sfen, sfenWithPly, usiMove, m_currentFilePath, &errorMessage);
    emit mergeRegistrationFinished(sfen, usiMove, success);
    if (!success) QMessageBox::warning(this, tr("エラー"), errorMessage);
    updateJosekiDisplay();
}
