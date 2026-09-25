/// @file kifufilecontroller.cpp
/// @brief 棋譜ファイル操作コントローラの実装

#include "kifufilecontroller.h"
#include "kifuexportcontroller.h"
#include "kifuloadcoordinator.h"
#include "kifupastedialog.h"
#include "kifusavecoordinator.h"
#include "gamerecordmodel.h"
#include "gamesettings.h"
#include "logcategories.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QStatusBar>

KifuFileController::KifuFileController(QObject* parent)
    : QObject(parent)
{
}

void KifuFileController::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

bool KifuFileController::confirmDiscardUnsaved()
{
    auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr;
    return KifuSaveCoordinator::confirmDiscardUnsaved(
        m_deps.parentWidget, record && record->isDirty(), [this, record]() {
            overwriteKifuFile();
            return record && !record->isDirty();
        });
}

void KifuFileController::prepareForKifuLoad()
{
    if (m_deps.setReplayMode) m_deps.setReplayMode(true);
    if (m_deps.clearUiBeforeKifuLoad) m_deps.clearUiBeforeKifuLoad();
    if (m_deps.ensurePlayerInfoAndGameInfo) m_deps.ensurePlayerInfoAndGameInfo();
}

void KifuFileController::chooseAndLoadKifuFile()
{
    qCDebug(lcApp) << "chooseAndLoadKifuFile ENTER";

    // 1) ファイル選択
    const QString lastDir = GameSettings::lastKifuDirectory();

    const QString filePath = QFileDialog::getOpenFileName(
        m_deps.parentWidget, tr("棋譜ファイルを開く"), lastDir,
        tr("Kifu Files (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.usi *.sfen *.usen);;"
           "KIF Files (*.kif *.kifu *.ki2 *.ki2u);;"
           "CSA Files (*.csa);;"
           "JKF Files (*.jkf);;"
           "USI Files (*.usi *.sfen);;"
           "USEN Files (*.usen)")
        );

    if (filePath.isEmpty() || !confirmDiscardUnsaved()) return;

    // 選択したファイルのディレクトリを保存
    QFileInfo fileInfo(filePath);
    GameSettings::setLastKifuDirectory(fileInfo.absolutePath());

    prepareForKifuLoad();

    // 2) KifuLoadCoordinator の作成・配線・読み込み実行
    if (m_deps.createAndWireKifuLoadCoordinator) m_deps.createAndWireKifuLoadCoordinator();

    qCDebug(lcApp) << "chooseAndLoadKifuFile: loading file=" << filePath;
    const bool loaded = dispatchKifuLoad(filePath);

    // 読み込んだファイルを「上書き保存」の対象にする。
    // 失敗時は表示中の棋譜が変わらないので、保存先も変更しない。
    if (loaded) {
        setOverwriteTarget(filePath);
    }

    qCDebug(lcApp) << "chooseAndLoadKifuFile LEAVE";
}

void KifuFileController::saveKifuToFile()
{
    if (m_deps.ensureGameRecordModel) m_deps.ensureGameRecordModel();
    if (m_deps.ensureKifuExportController) m_deps.ensureKifuExportController();
    if (m_deps.updateKifuExportDependencies) m_deps.updateKifuExportDependencies();

    auto* kec = m_deps.getKifuExportController ? m_deps.getKifuExportController() : nullptr;
    if (!kec) return;

    const QString path = kec->saveToFile();
    if (!path.isEmpty() && m_deps.saveFileName) {
        *m_deps.saveFileName = path;
    }
}

void KifuFileController::overwriteKifuFile()
{
    if (!m_deps.saveFileName || m_deps.saveFileName->isEmpty()) {
        saveKifuToFile();
        return;
    }

    if (m_deps.ensureGameRecordModel) m_deps.ensureGameRecordModel();
    if (m_deps.ensureKifuExportController) m_deps.ensureKifuExportController();
    if (m_deps.updateKifuExportDependencies) m_deps.updateKifuExportDependencies();

    auto* kec = m_deps.getKifuExportController ? m_deps.getKifuExportController() : nullptr;
    if (kec) {
        (void)kec->overwriteFile(*m_deps.saveFileName);
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
    prepareForKifuLoad();
    if (m_deps.prepareKifuLoadCoordinatorForLive) m_deps.prepareKifuLoadCoordinatorForLive();

    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (klc) {
        const bool success = klc->loadKifuFromString(content);
        // 貼り付けた棋譜はファイル由来ではないので、以前のファイルへ上書きさせない
        if (success) {
            clearOverwriteTarget();
            if (auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr) {
                record->markDirty();
            }
        }
        if (m_deps.statusBar) {
            if (success) {
                m_deps.statusBar->showMessage(tr("棋譜を取り込みました"), 3000);
            } else {
                m_deps.statusBar->showMessage(tr("棋譜の取り込みに失敗しました"), 3000);
            }
        }
    } else {
        qCWarning(lcApp) << "onKifuPasteImportRequested: KifuLoadCoordinator is null";
        if (m_deps.statusBar) {
            m_deps.statusBar->showMessage(tr("棋譜の取り込みに失敗しました（内部エラー）"), 3000);
        }
    }
}

void KifuFileController::onSfenCollectionPositionSelected(const QString& sfen)
{
    if (!confirmDiscardUnsaved()) return;
    prepareForKifuLoad();
    if (m_deps.prepareKifuLoadCoordinatorForLive) m_deps.prepareKifuLoadCoordinatorForLive();

    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (klc) {
        const bool success = klc->loadPositionFromSfen(sfen);
        // 局面集から反映した局面はファイル由来ではないので、以前のファイルへ上書きさせない
        if (success) {
            clearOverwriteTarget();
            if (auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr) {
                record->markDirty();
            }
        }
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

void KifuFileController::autoSaveKifuToFile(const QString& saveDir)
{
    qCDebug(lcApp) << "autoSaveKifuToFile called: dir=" << saveDir;

    if (m_deps.ensureGameRecordModel) m_deps.ensureGameRecordModel();
    if (m_deps.ensureKifuExportController) m_deps.ensureKifuExportController();
    if (m_deps.updateKifuExportDependencies) m_deps.updateKifuExportDependencies();

    auto* kec = m_deps.getKifuExportController ? m_deps.getKifuExportController() : nullptr;
    if (!kec) {
        qCWarning(lcApp) << "autoSaveKifuToFile: KifuExportController is null";
        return;
    }

    if (auto savedPath = kec->autoSaveToDir(saveDir)) {
        if (m_deps.saveFileName) {
            *m_deps.saveFileName = *savedPath;
        }
    }
}

// ============================================================
// 自動化 API 用の非対話 API
// ============================================================

bool KifuFileController::hasUnsavedChanges() const
{
    auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr;
    return record && record->isDirty();
}

bool KifuFileController::loadKifuFile(const QString& filePath)
{
    prepareForKifuLoad();
    if (m_deps.createAndWireKifuLoadCoordinator) m_deps.createAndWireKifuLoadCoordinator();
    const bool loaded = dispatchKifuLoad(filePath);
    if (loaded) setOverwriteTarget(filePath);
    return loaded;
}

bool KifuFileController::loadKifuText(const QString& content)
{
    prepareForKifuLoad();
    if (m_deps.createAndWireKifuLoadCoordinator) m_deps.createAndWireKifuLoadCoordinator();
    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (!klc) return false;
    const bool success = klc->loadKifuFromString(content);
    if (success) {
        clearOverwriteTarget();
        if (auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr) {
            record->markDirty();
        }
    }
    return success;
}

bool KifuFileController::applySfenPosition(const QString& sfen)
{
    prepareForKifuLoad();
    if (m_deps.prepareKifuLoadCoordinatorForLive) m_deps.prepareKifuLoadCoordinatorForLive();
    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (!klc) return false;
    const bool success = klc->loadPositionFromSfen(sfen);
    if (success) {
        clearOverwriteTarget();
        if (auto* record = m_deps.getGameRecordModel ? m_deps.getGameRecordModel() : nullptr) {
            record->markDirty();
        }
    }
    return success;
}

bool KifuFileController::saveKifuToPath(const QString& filePath)
{
    if (m_deps.ensureGameRecordModel) m_deps.ensureGameRecordModel();
    if (m_deps.ensureKifuExportController) m_deps.ensureKifuExportController();
    if (m_deps.updateKifuExportDependencies) m_deps.updateKifuExportDependencies();

    auto* kec = m_deps.getKifuExportController ? m_deps.getKifuExportController() : nullptr;
    if (!kec) return false;
    const bool ok = kec->overwriteFile(filePath);
    if (ok) setOverwriteTarget(filePath);
    return ok;
}

bool KifuFileController::dispatchKifuLoad(const QString& filePath)
{
    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (!klc) return false;

    if (filePath.endsWith(QLatin1String(".csa"), Qt::CaseInsensitive)) {
        return klc->loadCsaFromFile(filePath);
    }
    if (filePath.endsWith(QLatin1String(".ki2"), Qt::CaseInsensitive)
        || filePath.endsWith(QLatin1String(".ki2u"), Qt::CaseInsensitive)) {
        return klc->loadKi2FromFile(filePath);
    }
    if (filePath.endsWith(QLatin1String(".jkf"), Qt::CaseInsensitive)) {
        return klc->loadJkfFromFile(filePath);
    }
    if (filePath.endsWith(QLatin1String(".usen"), Qt::CaseInsensitive)) {
        return klc->loadUsenFromFile(filePath);
    }
    if (filePath.endsWith(QLatin1String(".usi"), Qt::CaseInsensitive)
        || filePath.endsWith(QLatin1String(".sfen"), Qt::CaseInsensitive)) {
        return klc->loadUsiFromFile(filePath);
    }
    return klc->loadKifuFromFile(filePath);
}

void KifuFileController::setOverwriteTarget(const QString& filePath)
{
    if (!m_deps.saveFileName) return;

    // 保存形式を拡張子で決められるファイルだけを上書き対象にする。
    // それ以外（.sfen や不明な拡張子）は「上書き保存」で名前を付けて保存へ誘導する。
    if (KifuSaveCoordinator::hasKnownSaveExtension(filePath)) {
        *m_deps.saveFileName = filePath;
    } else {
        m_deps.saveFileName->clear();
    }
}

void KifuFileController::clearOverwriteTarget()
{
    if (m_deps.saveFileName) {
        m_deps.saveFileName->clear();
    }
}
