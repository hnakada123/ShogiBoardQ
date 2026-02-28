/// @file kifufilecontroller.cpp
/// @brief 棋譜ファイル操作コントローラの実装

#include "kifufilecontroller.h"
#include "kifuexportcontroller.h"
#include "kifuloadcoordinator.h"
#include "kifupastedialog.h"
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

    if (filePath.isEmpty()) return;

    // 選択したファイルのディレクトリを保存
    QFileInfo fileInfo(filePath);
    GameSettings::setLastKifuDirectory(fileInfo.absolutePath());

    if (m_deps.setReplayMode) m_deps.setReplayMode(true);
    if (m_deps.clearUiBeforeKifuLoad) m_deps.clearUiBeforeKifuLoad();
    if (m_deps.ensurePlayerInfoAndGameInfo) m_deps.ensurePlayerInfoAndGameInfo();

    // 2) KifuLoadCoordinator の作成・配線・読み込み実行
    if (m_deps.createAndWireKifuLoadCoordinator) m_deps.createAndWireKifuLoadCoordinator();

    qCDebug(lcApp) << "chooseAndLoadKifuFile: loading file=" << filePath;
    dispatchKifuLoad(filePath);

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

    if (m_deps.clearUiBeforeKifuLoad) m_deps.clearUiBeforeKifuLoad();
    if (m_deps.ensureKifuLoadCoordinatorForLive) m_deps.ensureKifuLoadCoordinatorForLive();

    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (klc) {
        const bool success = klc->loadKifuFromString(content);
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
    if (m_deps.clearUiBeforeKifuLoad) m_deps.clearUiBeforeKifuLoad();
    if (m_deps.ensureKifuLoadCoordinatorForLive) m_deps.ensureKifuLoadCoordinatorForLive();

    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (klc) {
        const bool success = klc->loadPositionFromSfen(sfen);
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

void KifuFileController::autoSaveKifuToFile(const QString& saveDir, PlayMode playMode,
                                            const QString& humanName1, const QString& humanName2,
                                            const QString& engineName1, const QString& engineName2)
{
    Q_UNUSED(playMode);
    Q_UNUSED(humanName1);
    Q_UNUSED(humanName2);
    Q_UNUSED(engineName1);
    Q_UNUSED(engineName2);
    qCDebug(lcApp) << "autoSaveKifuToFile called: dir=" << saveDir
                   << "mode=" << static_cast<int>(playMode);

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

void KifuFileController::dispatchKifuLoad(const QString& filePath)
{
    auto* klc = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    if (!klc) return;

    if (filePath.endsWith(QLatin1String(".csa"), Qt::CaseInsensitive)) {
        klc->loadCsaFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".ki2"), Qt::CaseInsensitive)
               || filePath.endsWith(QLatin1String(".ki2u"), Qt::CaseInsensitive)) {
        klc->loadKi2FromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".jkf"), Qt::CaseInsensitive)) {
        klc->loadJkfFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usen"), Qt::CaseInsensitive)) {
        klc->loadUsenFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usi"), Qt::CaseInsensitive)) {
        klc->loadUsiFromFile(filePath);
    } else {
        klc->loadKifuFromFile(filePath);
    }
}
