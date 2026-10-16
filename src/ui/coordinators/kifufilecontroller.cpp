/// @file kifufilecontroller.cpp
/// @brief 棋譜ファイル操作コントローラの実装

#include "kifufilecontroller.h"
#include "appstate.h"
#include "kifuexportcontroller.h"
#include "kifuloadcoordinator.h"
#include "kifupastedialog.h"
#include "kifusavecoordinator.h"
#include "gamerecordmodel.h"
#include "gameinfopane.h"
#include "gamesettings.h"
#include "logcategories.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QStatusBar>
#include <QProgressDialog>

KifuFileController::KifuFileController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.state && m_deps.gameRecord && m_deps.exportController && m_deps.loadCoordinator);
    connect(m_deps.loadCoordinator, &KifuLoadCoordinator::loadFinished, this, &KifuFileController::onAsyncLoadFinished);
}

bool KifuFileController::confirmDiscardUnsaved()
{
    if (m_deps.gameInfo) m_deps.gameInfo->commitPendingEditor();
    return KifuSaveCoordinator::confirmDiscardUnsaved(
        m_deps.parentWidget, hasUnsavedChanges(), [this]() {
            overwriteKifuFile();
            return !hasUnsavedChanges();
        });
}

void KifuFileController::prepareForKifuLoad()
{
    if (m_deps.clearUiBeforeKifuLoad) m_deps.clearUiBeforeKifuLoad();
}

void KifuFileController::chooseAndLoadKifuFile()
{
    qCDebug(lcApp) << "chooseAndLoadKifuFile ENTER";

    // 1) ファイル選択
    const QString lastDir = GameSettings::lastKifuDirectory();

    const QString filePath = QFileDialog::getOpenFileName(
        m_deps.parentWidget, tr("棋譜ファイルを開く"), lastDir,
        tr("Kifu Files (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.json *.usi *.sfen *.usen);;"
           "KIF Files (*.kif *.kifu);;"
           "KI2 Files (*.ki2 *.ki2u);;"
           "CSA Files (*.csa);;"
           "JKF Files (*.jkf *.json);;"
           "USI/SFEN Files (*.usi *.sfen);;"
           "USEN Files (*.usen)")
        );

    if (filePath.isEmpty() || !confirmDiscardUnsaved()) return;

    // 選択したファイルのディレクトリを保存
    QFileInfo fileInfo(filePath);
    GameSettings::lastKifuDirectory.set(fileInfo.absolutePath());

    startAsyncLoad(filePath, false);
}

void KifuFileController::saveKifuToFile()
{
    auto* kec = m_deps.exportController;
    if (!kec) return;

    const QString path = kec->saveToFile();
    if (!path.isEmpty() && m_deps.state) {
        m_deps.state->saveFileName = path;
    }
}

void KifuFileController::overwriteKifuFile()
{
    if (!m_deps.state || m_deps.state->saveFileName.isEmpty()) {
        saveKifuToFile();
        return;
    }

    auto* kec = m_deps.exportController;
    if (kec) {
        (void)kec->overwriteFile(m_deps.state->saveFileName);
    }
}

void KifuFileController::pasteKifuFromClipboard()
{
    // 既にダイアログが開いている場合はアクティブにする
    if (m_kifuPasteDialog) {
        m_kifuPasteDialog->raise();
        m_kifuPasteDialog->activateWindow();
        return;
    }

    m_kifuPasteDialog = new KifuPasteDialog(m_deps.parentWidget);
    m_kifuPasteDialog->setAttribute(Qt::WA_DeleteOnClose);

    // ダイアログの「取り込む」シグナルを自身のスロットに接続
    connect(m_kifuPasteDialog, &KifuPasteDialog::importRequested,
            this, &KifuFileController::onKifuPasteImportRequested);

    m_kifuPasteDialog->show();
}

void KifuFileController::onKifuPasteImportRequested(const QString& content)
{
    qCDebug(lcApp) << "onKifuPasteImportRequested: content length =" << content.size();

    if (!confirmDiscardUnsaved()) return;
    startAsyncLoad(content, true);
}

void KifuFileController::startAsyncLoad(const QString& input, bool text)
{
    prepareForKifuLoad();
    auto* coordinator = m_deps.loadCoordinator;
    if (!coordinator) {
        qCWarning(lcApp) << "KifuLoadCoordinator is null";
        return;
    }
    if (m_loadProgress) m_loadProgress->deleteLater();
    m_loadingText = text;
    m_pendingLoadPath = text ? QString() : input;
    // 入力を待つイベントループは通常どおり動かし、読み込み中の棋譜編集を防ぐ。
    m_loadProgress = new QProgressDialog(tr("棋譜を読み込み中..."), tr("キャンセル"), 0, 0, m_deps.parentWidget);
    m_loadProgress->setWindowModality(Qt::WindowModal);
    m_loadProgress->setMinimumDuration(0);
    connect(m_loadProgress, &QProgressDialog::canceled, coordinator, &KifuLoadCoordinator::cancelLoad);
    m_asyncLoadPending = true;
    m_loadProgress->show();
    if (text) coordinator->loadTextAsync(input);
    else coordinator->loadFileAsync(input);
}

void KifuFileController::onAsyncLoadFinished(bool success)
{
    // 自動化 API などほかから始めた読み込みの終わりは扱わない
    if (!m_asyncLoadPending) return;
    m_asyncLoadPending = false;
    if (m_loadProgress) {
        m_loadProgress->close();
        m_loadProgress->deleteLater();
        m_loadProgress = nullptr;
    }
    if (!success) return;
    if (m_loadingText) {
        clearOverwriteTarget();
        if (auto* record = m_deps.gameRecord) record->markDirty();
        if (m_kifuPasteDialog) m_kifuPasteDialog->accept();
        if (m_deps.statusBar) m_deps.statusBar->showMessage(tr("棋譜を取り込みました"), 3000);
    } else {
        attachToFile(m_pendingLoadPath);
    }
}

void KifuFileController::onSfenCollectionPositionSelected(const QString& sfen)
{
    if (!confirmDiscardUnsaved()) return;
    prepareForKifuLoad();
    auto* klc = m_deps.loadCoordinator;
    if (klc) {
        const bool success = klc->loadPositionFromSfen(sfen);
        // 局面集から反映した局面は棋譜ファイル由来ではないので、以前のファイルへ上書きさせない。
        // 局面そのものは局面集ファイルに残っているため未保存扱いにはせず、続けて別の局面を
        // 選んでも保存の確認を出さない（反映後に指し手を進めた場合は通常どおり確認する）
        if (success) attachToFile(QString());
        if (m_deps.statusBar) {
            if (success) {
                m_deps.statusBar->showMessage(tr("局面を反映しました"), 3000);
            } else {
                m_deps.statusBar->showMessage(tr("局面の反映に失敗しました"), 3000);
            }
        }
    } else {
        if (m_deps.statusBar) {
            m_deps.statusBar->showMessage(tr("局面の反映に失敗しました（内部エラー）"), 3000);
        }
    }
}

void KifuFileController::autoSaveKifu(const QString& saveDir, const QString& extension)
{
    qCDebug(lcApp) << "autoSaveKifu: dir=" << saveDir << "extension=" << extension;

    auto* kec = m_deps.exportController;
    if (!kec) {
        qCWarning(lcApp) << "autoSaveKifuToFile: KifuExportController is null";
        return;
    }

    if (auto savedPath = kec->autoSaveToDir(saveDir, extension)) {
        if (m_deps.state) {
            m_deps.state->saveFileName = *savedPath;
        }
    }
}

// ============================================================
// 自動化 API 用の非対話 API
// ============================================================

bool KifuFileController::hasUnsavedChanges() const
{
    auto* record = m_deps.gameRecord;
    return (record && record->isDirty()) || (m_deps.gameInfo && m_deps.gameInfo->isDirty());
}

bool KifuFileController::loadKifuFile(const QString& filePath)
{
    prepareForKifuLoad();
    // 拡張子が .json などでも、内容から形式を判定して読み込む
    const bool loaded = m_deps.loadCoordinator && m_deps.loadCoordinator->loadFile(filePath);
    if (loaded) attachToFile(filePath);
    return loaded;
}

bool KifuFileController::loadKifuText(const QString& content)
{
    prepareForKifuLoad();
    auto* klc = m_deps.loadCoordinator;
    if (!klc) return false;
    const bool success = klc->loadText(content);
    if (success) {
        clearOverwriteTarget();
        if (auto* record = m_deps.gameRecord) {
            record->markDirty();
        }
    }
    return success;
}

bool KifuFileController::applySfenPosition(const QString& sfen)
{
    prepareForKifuLoad();
    auto* klc = m_deps.loadCoordinator;
    if (!klc) return false;
    const bool success = klc->loadPositionFromSfen(sfen);
    if (success) {
        clearOverwriteTarget();
        if (auto* record = m_deps.gameRecord) {
            record->markDirty();
        }
    }
    return success;
}

bool KifuFileController::saveKifuToPath(const QString& filePath, QString* errorText)
{
    auto* kec = m_deps.exportController;
    if (!kec) return false;
    const bool ok = kec->saveToPathWithoutDialogs(filePath, errorText);
    if (ok) attachToFile(filePath);
    return ok;
}

void KifuFileController::attachToFile(const QString& filePath)
{
    if (auto* record = m_deps.gameRecord) record->clearDirty();
    if (!m_deps.state) return;

    // 保存形式を拡張子で決められるファイルだけを上書き対象にする。
    // それ以外（.sfen や不明な拡張子）は「上書き保存」で名前を付けて保存へ誘導する。
    if (KifuSaveFile::hasKnownSaveExtension(filePath)) {
        m_deps.state->saveFileName = filePath;
    } else {
        m_deps.state->saveFileName.clear();
    }
}

void KifuFileController::clearOverwriteTarget()
{
    if (m_deps.state) {
        m_deps.state->saveFileName.clear();
    }
}
