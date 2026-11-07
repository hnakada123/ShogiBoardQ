/// @file kifufilecontroller.cpp
/// @brief 棋譜の文書とファイルをつなぐ部品の実装

#include "kifufilecontroller.h"
#include "notificationpresenter.h"

#include "kifufilefilter.h"
#include "appactivitystate.h"
#include "gamesettings.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "kifuloader.h"
#include "kifupastedialog.h"
#include "kifusavedialogs.h"
#include "logcategories.h"
#include "sfencollectiondialog.h"
#include "uistatepolicymanager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QProgressDialog>
#include <functional>

KifuFileController::KifuFileController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.loader);
    connect(m_deps.loader, &KifuLoader::loadFinished, this, &KifuFileController::onAsyncLoadFinished);
}

bool KifuFileController::hasUnsavedChanges() const
{
    return m_deps.document->isModified();
}

bool KifuFileController::canReplaceRecord() const
{
    // 「開く」を押せる状態と同じ1か所で決める（貼り付け・局面集ビューアは開いたまま反映できるので、ここでも確かめる）
    return UiStatePolicyManager::tablePolicy(m_deps.activity->activity(), UiStatePolicyManager::UiElement::FileOpen)
        == UiStatePolicyManager::Policy::Enabled;
}

bool KifuFileController::refuseReplacingRecord()
{
    if (canReplaceRecord()) return false;
    // 開いたままの棋譜貼り付け・局面集ビューアから、対局などを始めた後に反映しようとしたとき
    emit statusMessage(tr("対局・検討・解析・局面編集の間は、棋譜を読み込めません"), 5000);
    return true;
}

bool KifuFileController::confirmDiscardUnsaved()
{
    return KifuSaveDialogs::confirmDiscardUnsaved(m_deps.parentWidget, hasUnsavedChanges(), [this]() {
        overwriteKifuFile();
        return !hasUnsavedChanges();
    });
}

QStringList KifuFileController::exportLines(KifuFormat format, KifuEncoding encoding) const
{
    return KifuExport::lines(*m_deps.document, format, encoding);
}

// ============================================================
// 読み込み
// ============================================================

void KifuFileController::chooseAndLoadKifuFile()
{
    if (refuseReplacingRecord()) return;
    const QString filePath = QFileDialog::getOpenFileName(
        m_deps.parentWidget, tr("棋譜ファイルを開く"), GameSettings::lastKifuDirectory(), KifuFileFilter::forOpening());
    if (filePath.isEmpty() || !confirmDiscardUnsaved()) return;

    // 選択したファイルのディレクトリを保存
    GameSettings::lastKifuDirectory.set(QFileInfo(filePath).absolutePath());
    startAsyncLoad(filePath, Source::File);
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
    connect(m_kifuPasteDialog, &KifuPasteDialog::importRequested,
            this, &KifuFileController::onKifuPasteImportRequested);
    m_kifuPasteDialog->show();
}

void KifuFileController::onKifuPasteImportRequested(const QString& content)
{
    qCDebug(lcApp) << "onKifuPasteImportRequested: content length =" << content.size();
    if (refuseReplacingRecord() || !confirmDiscardUnsaved()) return;
    startAsyncLoad(content, Source::Text);
}

void KifuFileController::openSfenCollection()
{
    // 既にダイアログが開いている場合はアクティブにする
    if (m_sfenCollectionDialog) {
        m_sfenCollectionDialog->raise();
        m_sfenCollectionDialog->activateWindow();
        return;
    }
    m_sfenCollectionDialog = new SfenCollectionDialog(m_deps.parentWidget);
    m_sfenCollectionDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_sfenCollectionDialog, &SfenCollectionDialog::positionSelected,
            this, &KifuFileController::onSfenCollectionPositionSelected);
    m_sfenCollectionDialog->show();
}

void KifuFileController::onSfenCollectionPositionSelected(const QString& sfen)
{
    if (refuseReplacingRecord() || !confirmDiscardUnsaved()) return;
    finishLoad(m_deps.loader->loadPositionFromSfen(sfen), Source::Collection, QString());
}

void KifuFileController::startAsyncLoad(const QString& input, Source source)
{
    KifuLoader* coordinator = m_deps.loader;
    if (m_loadProgress) m_loadProgress->deleteLater();
    m_pendingSource = source;
    m_pendingLoadPath = source == Source::File ? input : QString();
    // 入力を待つイベントループは通常どおり動かし、読み込み中の棋譜編集を防ぐ
    m_loadProgress = new QProgressDialog(tr("棋譜を読み込み中..."), tr("キャンセル"), 0, 0, m_deps.parentWidget);
    m_loadProgress->setWindowModality(Qt::WindowModal);
    m_loadProgress->setMinimumDuration(0);
    connect(m_loadProgress, &QProgressDialog::canceled, coordinator, &KifuLoader::cancelLoad);
    m_asyncLoadPending = true;
    m_loadProgress->show();
    if (source == Source::Text) coordinator->loadTextAsync(input);
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
    finishLoad(success, m_pendingSource, m_pendingLoadPath);
}

void KifuFileController::finishLoad(bool success, Source source, const QString& path)
{
    KifuDocument* document = m_deps.document;
    switch (source) {
    case Source::File:
        if (success) document->markSaved(path);
        break;
    case Source::Text:
        if (!success) return;
        document->markUnsavedContent();
        if (m_kifuPasteDialog) m_kifuPasteDialog->accept();
        emit statusMessage(tr("棋譜を取り込みました"), 3000);
        break;
    case Source::Position:
        if (success) document->markUnsavedContent();
        break;
    case Source::Collection:
        // 局面そのものは局面集ファイルに残っているので変更なしとし、続けて別の局面を選んでも保存の確認を出さない
        // （反映後に指し手を進めた場合は通常どおり確認する）。上書き保存先は持たない
        emit statusMessage(success ? tr("局面を反映しました") : tr("局面の反映に失敗しました"), 3000);
        break;
    }
}

bool KifuFileController::loadKifuFile(const QString& filePath)
{
    // 拡張子が .json などでも、内容から形式を判定して読み込む
    const bool loaded = m_deps.loader->loadFile(filePath);
    finishLoad(loaded, Source::File, filePath);
    return loaded;
}

bool KifuFileController::loadKifuText(const QString& content)
{
    const bool loaded = m_deps.loader->loadText(content);
    finishLoad(loaded, Source::Text, QString());
    return loaded;
}

bool KifuFileController::applySfenPosition(const QString& sfen)
{
    const bool loaded = m_deps.loader->loadPositionFromSfen(sfen);
    finishLoad(loaded, Source::Position, QString());
    return loaded;
}

// ============================================================
// 保存
// ============================================================

void KifuFileController::markSaved(const QString& filePath, const QString& message)
{
    m_deps.document->markSaved(filePath);
    emit statusMessage(message, 5000);
}

QString KifuFileController::defaultFileName(const QString& extension) const
{
    // 対局情報の対局者名から作る（読み込んだ・貼り付けた棋譜も同じ。対局した棋譜は対局情報に対局者が入っている）
    const KifuDocument* document = m_deps.document;
    return KifuSaveFile::makeDefaultSaveFileName(document->headerPlayerName(Turn::Black),
                                                 document->headerPlayerName(Turn::White),
                                                 QDateTime::currentDateTime(), extension);
}

void KifuFileController::saveKifuToFile()
{
    // 入力中のコメントも、選んだ形式で失うものに数える
    m_deps.document->commitPendingEdits();
    const KifuBranchTree* tree = m_deps.document->tree();

    QString error;
    const QString path = KifuSaveDialogs::saveViaDialog(
        m_deps.parentWidget,
        std::bind(&KifuFileController::exportLines, this, std::placeholders::_1, std::placeholders::_2),
        defaultFileName(QStringLiteral("kifu")), std::bind(&KifuExport::lossIn, std::cref(*tree), std::placeholders::_1),
        &error);
    if (!error.isEmpty()) NotificationPresenter::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
    if (!path.isEmpty()) markSaved(path, tr("棋譜を保存しました: %1").arg(path));
}

void KifuFileController::overwriteKifuFile()
{
    const QString filePath = m_deps.document->filePath();
    if (filePath.isEmpty()) {
        saveKifuToFile();
        return;
    }
    QString error;
    // 確認ダイアログでキャンセルした場合は error が空
    if (!writeToPath(filePath, m_deps.parentWidget, &error) && !error.isEmpty())
        NotificationPresenter::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
}

bool KifuFileController::saveKifuToPath(const QString& filePath, QString* errorText)
{
    return writeToPath(filePath, nullptr, errorText);
}

bool KifuFileController::writeToPath(const QString& filePath, QWidget* dialogParent, QString* errorText)
{
    if (errorText) errorText->clear();
    if (filePath.isEmpty()) return false;
    // 利用者の保存は「名前を付けて保存」と同じく、形式に書けないものがあれば確かめる
    // （書けないものが消えたまま未保存の印まで消えないように。自動化 API は確認を出さない）
    if (dialogParent) {
        m_deps.document->commitPendingEdits();
        const KifuFormat format = KifuSaveFile::saveFormatForPath(filePath);
        const KifuExport::Loss loss = KifuExport::lossIn(*m_deps.document->tree(), format);
        if (!loss.isEmpty() && !KifuSaveDialogs::confirmLossySave(dialogParent, format, loss)) return false;
    }
    // 名前を付けて保存と同じ基準（拡張子）で保存形式と文字コードを決める。
    // KIF 固定で書くと .csa などに保存したファイルが KIF 内容で上書きされてしまう
    const bool ok = KifuSaveDialogs::overwriteExisting(
        dialogParent, filePath,
        std::bind(&KifuFileController::exportLines, this, std::placeholders::_1, std::placeholders::_2), errorText);
    if (ok) markSaved(filePath, tr("棋譜を上書き保存しました: %1").arg(filePath));
    return ok;
}

void KifuFileController::autoSaveKifu(const QString& saveDir, const QString& extension)
{
    qCDebug(lcApp) << "autoSaveKifu: dir=" << saveDir << "extension=" << extension;
    if (saveDir.isEmpty() || !QDir().mkpath(saveDir)) {
        emit statusMessage(tr("棋譜の自動保存先を作成できません: %1").arg(saveDir), 5000);
        return;
    }
    // 名前は対局者と日時から作る。同じ秒に終わった対局があっても上書きしない
    const QDir dir(saveDir);
    const QString fileName = defaultFileName(extension);
    QString filePath = dir.filePath(fileName);
    const QFileInfo info(fileName);
    for (int serial = 2; QFileInfo::exists(filePath); ++serial)
        filePath = dir.filePath(QStringLiteral("%1_%2.%3").arg(info.completeBaseName()).arg(serial).arg(info.suffix()));

    if (exportLines(KifuSaveFile::saveFormatForPath(filePath)).isEmpty()) {
        emit statusMessage(tr("棋譜データが空のため自動保存をスキップしました"), 3000);
        return;
    }
    QString errorText;
    if (writeToPath(filePath, nullptr, &errorText)) {
        emit statusMessage(tr("棋譜を自動保存しました: %1").arg(filePath), 5000);
        return;
    }
    qCWarning(lcKifu).noquote() << "autoSaveKifu failed:" << errorText;
    NotificationPresenter::critical(m_deps.parentWidget, tr("棋譜の自動保存"),
                                    tr("棋譜の自動保存に失敗しました: %1").arg(errorText));
}
