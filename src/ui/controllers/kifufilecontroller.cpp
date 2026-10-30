/// @file kifufilecontroller.cpp
/// @brief 棋譜の文書とファイルをつなぐ部品の実装

#include "kifufilecontroller.h"

#include "kifufilefilter.h"
#include "appactivitystate.h"
#include "bodtextgenerator.h"
#include "errorbus.h"
#include "gamesettings.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "kifuloader.h"
#include "kifunavigationstate.h"
#include "kifupastedialog.h"
#include "kifusavedialogs.h"
#include "logcategories.h"
#include "shogiboard.h"
#include "usiexporter.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <functional>

KifuFileController::KifuFileController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.loader && m_deps.board && m_deps.navState);
    connect(m_deps.loader, &KifuLoader::loadFinished, this, &KifuFileController::onAsyncLoadFinished);
}

bool KifuFileController::hasUnsavedChanges() const
{
    return m_deps.document->isModified();
}

bool KifuFileController::canReplaceRecord() const
{
    return m_deps.activity->is(AppActivityState::Activity::Idle);
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
    if (loaded) m_deps.document->markUnsavedContent();
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
    const KifuBranchTree* tree = m_deps.document->tree();
    const bool hasBranches = tree->lineCount() > 1;
    bool hasTimeInfo = false;
    for (const auto* node : tree->mainLine()) {
        if (!node->timeText().isEmpty()) {
            hasTimeInfo = true;
            break;
        }
    }

    QString error;
    const QString path = KifuSaveDialogs::saveViaDialog(
        m_deps.parentWidget,
        std::bind(&KifuFileController::exportLines, this, std::placeholders::_1, std::placeholders::_2),
        defaultFileName(QStringLiteral("kifu")),
        hasBranches, hasTimeInfo, &error);
    if (!error.isEmpty()) QMessageBox::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
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
        QMessageBox::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
}

bool KifuFileController::saveKifuToPath(const QString& filePath, QString* errorText)
{
    return writeToPath(filePath, nullptr, errorText);
}

bool KifuFileController::writeToPath(const QString& filePath, QWidget* dialogParent, QString* errorText)
{
    if (errorText) errorText->clear();
    if (filePath.isEmpty()) return false;
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
    ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, tr("棋譜の自動保存に失敗しました: %1").arg(errorText));
}

// ============================================================
// クリップボードへのコピー
// ============================================================

bool KifuFileController::setClipboardText(const QString& text, const QString& successMsg)
{
    if (QClipboard* clipboard = QApplication::clipboard()) {
        clipboard->setText(text);
        emit statusMessage(successMsg, 3000);
        return true;
    }
    emit statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuFileController::copyRecord(KifuFormat format)
{
    const bool mainLineOnly = m_deps.document->tree()->lineCount() > 1;
    struct Messages {
        QString empty;
        QString copied;
    };
    Messages m;
    switch (format) {
    case KifuFormat::Kif:
        m = {tr("KIF形式の棋譜データがありません"), tr("KIF形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        // 局面だけの形式は棋譜としてはコピーしない（局面のコピーは別の操作）
        return false;
    case KifuFormat::Ki2:
        m = {tr("KI2形式の棋譜データがありません"), tr("KI2形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Csa:
        // CSA・USI 形式は分岐を書けない
        m = {tr("CSA形式の棋譜データがありません"),
             mainLineOnly ? tr("CSA形式の棋譜をクリップボードにコピーしました（CSA形式は分岐に対応していないため、本譜のみです）")
                          : tr("CSA形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Usi:
        m = {tr("USI形式の棋譜データがありません"),
             mainLineOnly ? tr("USI形式の棋譜をクリップボードにコピーしました（USI形式は分岐に対応していないため、本譜のみです）")
                          : tr("USI形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Jkf:
        m = {tr("JKF形式の棋譜データがありません"), tr("JKF形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Usen:
        m = {tr("USEN形式の棋譜データがありません"), tr("USEN形式の棋譜をクリップボードにコピーしました")};
        break;
    }
    const QStringList lines = exportLines(format);
    if (lines.isEmpty()) {
        emit statusMessage(m.empty, 3000);
        return false;
    }
    return setClipboardText(lines.join(QStringLiteral("\n")), m.copied);
}

bool KifuFileController::copyUsiCurrentToClipboard()
{
    // 表示中の手順の、表示中の手までの指し手（終局の行まで来ていればその終局）
    QList<KifuBranchNode*> nodes;
    TerminalType terminal = TerminalType::None;
    const KifuNavigationState* nav = m_deps.navState;
    const int limit = nav->currentPly();
    for (KifuBranchNode* node : nav->selectedPath()) {
        if (node->ply() > limit) break;
        if (node->isTerminal()) {
            terminal = node->terminalType();
            break;
        }
        nodes.append(node);
    }
    const QStringList moves = KifuBranchTree::toRecordLine(nodes).usiMoves();
    const QStringList lines = UsiExporter::exportPosition(m_deps.document->startSfen(), moves, terminal);
    if (lines.isEmpty()) {
        emit statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(lines.join(QStringLiteral("\n")),
                            tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"));
}

QString KifuFileController::boardSfen() const
{
    // 局面編集による変更も含め、盤に出している局面・手番・持ち駒をコピーする
    const ShogiBoard* board = m_deps.board;
    // 手数は表示中の手の局面の手数（無ければ表示中の手数の次）
    bool ok = false;
    int moveNumber = m_deps.navState->currentSfen().section(QLatin1Char(' '), 3, 3).toInt(&ok);
    if (!ok) moveNumber = m_deps.navState->currentPly() + 1;
    return board->position().toSfen(board->currentPlayer(), moveNumber);
}

bool KifuFileController::copySfenToClipboard()
{
    const QString sfen = boardSfen();
    if (sfen.isEmpty()) {
        emit statusMessage(tr("SFEN形式の局面データがありません"), 3000);
        return false;
    }
    return setClipboardText(sfen, tr("SFEN形式の局面をクリップボードにコピーしました"));
}

bool KifuFileController::copyBodToClipboard()
{
    const QString sfen = boardSfen();
    if (sfen.isEmpty()) {
        emit statusMessage(tr("BOD形式の局面データがありません"), 3000);
        return false;
    }
    const std::optional<Position> position = Position::fromSfen(sfen);
    if (!position) {
        emit statusMessage(tr("SFEN形式の解析に失敗しました"), 3000);
        return false;
    }
    // 最終手は表示中の手（局面図の「手数＝N ▲７六歩 まで」）
    const KifuBranchNode* node = m_deps.navState->currentNode();
    const int ply = node ? node->ply() : 0;
    const QString lastMove = node && node->isActualMove() ? node->displayText() : QString();
    // 駒落ちは KIF の局面図と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
    const QString bodText = BodTextGenerator::generate(*position, ply, lastMove, m_deps.document->usesHandicapNames());
    return setClipboardText(bodText, tr("BOD形式の局面をクリップボードにコピーしました"));
}
