/// @file josekifilecontroller.cpp
/// @brief 定跡ウィンドウのファイル操作の実装

#include "josekifilecontroller.h"

#include "josekibooksession.h"

#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace {
QString bookFileFilter()
{
    return JosekiFileController::tr("定跡ファイル (*.db);;すべてのファイル (*)");
}

QString withBookSuffix(QString filePath)
{
    if (!filePath.isEmpty() && !filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".db");
    return filePath;
}
} // namespace

JosekiFileController::JosekiFileController(const Deps &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.session && m_deps.parentWidget);
    connect(m_deps.session, &JosekiBookSession::loadFailed, this, &JosekiFileController::onLoadFailed);
    connect(m_deps.session, &JosekiBookSession::saveFailed, this, &JosekiFileController::onSaveFailed);
    connect(m_deps.session, &JosekiBookSession::busyChanged, this, &JosekiFileController::onBusyChanged);
}

JosekiFileController::~JosekiFileController() = default;

bool JosekiFileController::isBusy() const
{
    return m_deps.session->isBusy();
}

// ============================================================
// 利用者操作
// ============================================================

void JosekiFileController::open()
{
    if (isBusy() || !confirmDiscardChanges()) return;
    const QString current = m_deps.session->filePath();
    const QString startDir = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    const QString filePath =
        QFileDialog::getOpenFileName(m_deps.parentWidget, tr("定跡ファイルを開く"), startDir, bookFileFilter());
    if (!filePath.isEmpty()) openFile(filePath);
}

void JosekiFileController::newBook()
{
    if (!confirmDiscardChanges()) return;
    emit bookSwitching();
    m_deps.session->newBook();
}

void JosekiFileController::save()
{
    if (isBusy()) return;
    if (m_deps.session->filePath().isEmpty()) {
        saveAs();
        return;
    }
    saveTo(m_deps.session->filePath());
}

void JosekiFileController::saveAs()
{
    if (isBusy()) return;
    const QString filePath = selectSaveFilePath();
    if (!filePath.isEmpty()) saveTo(filePath);
}

void JosekiFileController::openRecent(const QString &filePath)
{
    if (isBusy()) return;
    if (filePath.isEmpty() || !confirmDiscardChanges()) return;
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(m_deps.parentWidget, tr("エラー"), tr("ファイルが見つかりません: %1").arg(filePath));
        m_deps.session->removeRecentFile(filePath);
        return;
    }
    openFile(filePath);
}

void JosekiFileController::clearRecentFiles()
{
    if (isBusy()) return;
    // 最近使ったファイルだけを消す。編集中のデータ（未保存の変更を含む）は残す
    m_deps.session->clearRecentFiles();
    emit statusMessage(tr("最近使ったファイル履歴をクリアしました"));
}

// ============================================================
// 読み込み・保存
// ============================================================

void JosekiFileController::openFile(const QString &filePath)
{
    if (isBusy()) return;
    emit bookSwitching();
    m_deps.session->startLoad(filePath);
}

void JosekiFileController::saveTo(const QString &filePath)
{
    if (isBusy()) return;
    if (filePath != m_deps.session->filePath()) emit bookSwitching();
    m_deps.session->startSave(filePath);
}

bool JosekiFileController::saveNow(const QString &filePath)
{
    if (filePath != m_deps.session->filePath()) emit bookSwitching();
    QString errorMessage;
    if (m_deps.session->save(filePath, &errorMessage)) return true;
    QMessageBox::warning(m_deps.parentWidget, tr("エラー"), errorMessage);
    return false;
}

void JosekiFileController::onLoadFailed(const QString &errorMessage)
{
    if (!errorMessage.isEmpty()) QMessageBox::warning(m_deps.parentWidget, tr("エラー"), errorMessage);
}

void JosekiFileController::onSaveFailed(const QString &errorMessage)
{
    QMessageBox::warning(m_deps.parentWidget, tr("エラー"), errorMessage);
}

void JosekiFileController::onBusyChanged(bool busy)
{
    // 読み書きはワーカースレッドで行い、ほかの操作はできるので、待ちのカーソルは定跡ウィンドウの中だけに出す
    if (busy) m_deps.parentWidget->setCursor(Qt::BusyCursor);
    else m_deps.parentWidget->unsetCursor();
}

// ============================================================
// 確認と保存先
// ============================================================

bool JosekiFileController::confirmDiscardChanges()
{
    if (isBusy()) {
        QMessageBox::information(m_deps.parentWidget, tr("処理中"),
                                 tr("ファイルの読み込み/保存が完了するまでお待ちください。"));
        return false;
    }
    if (!m_deps.session->isModified()) return true;
    // 棋譜の未保存の警告（KifuSaveCoordinator::confirmDiscardUnsaved）と同じ見た目にし、どの定跡かを添える
    const QString current = m_deps.session->filePath();
    QMessageBox msgBox(QMessageBox::Warning, tr("未保存の定跡"), tr("定跡に未保存の変更があります。保存しますか？"),
                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, m_deps.parentWidget);
    msgBox.setInformativeText(current.isEmpty() ? tr("新しい定跡（まだファイルに保存していません）")
                                                 : tr("定跡ファイル: %1").arg(QFileInfo(current).fileName()));
    msgBox.button(QMessageBox::Save)->setText(tr("保存"));
    msgBox.button(QMessageBox::Discard)->setText(tr("破棄"));
    msgBox.button(QMessageBox::Cancel)->setText(tr("キャンセル"));
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);
    const int choice = msgBox.exec();
    if (choice == QMessageBox::Save) {
        const QString filePath = current.isEmpty() ? selectSaveFilePath() : current;
        // 確認の応答の流れの中なので、保存が終わるまで待つ
        return !filePath.isEmpty() && saveNow(filePath);
    }
    return choice == QMessageBox::Discard;
}

bool JosekiFileController::ensureFilePath()
{
    if (!m_deps.session->filePath().isEmpty()) return true;
    const QMessageBox::StandardButton result = QMessageBox::question(
        m_deps.parentWidget, tr("保存先の指定"),
        tr("定跡ファイルの保存先が設定されていません。\nOKを選択すると保存先が指定できます。"),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
    if (result != QMessageBox::Ok) return false;
    const QString filePath = withBookSuffix(QFileDialog::getSaveFileName(
        m_deps.parentWidget, tr("定跡ファイルの保存先を指定"), QDir::homePath(), bookFileFilter()));
    return !filePath.isEmpty() && saveNow(filePath);
}

QString JosekiFileController::selectSaveFilePath()
{
    const QString current = m_deps.session->filePath();
    const QString startDir = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    return withBookSuffix(
        QFileDialog::getSaveFileName(m_deps.parentWidget, tr("定跡ファイルを保存"), startDir, bookFileFilter()));
}
