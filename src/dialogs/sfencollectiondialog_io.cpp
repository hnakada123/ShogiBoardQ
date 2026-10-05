/// @file sfencollectiondialog_io.cpp
/// @brief SFEN局面集ビューアのファイル読み込み・行の解析・最近使ったファイル

#include "sfencollectiondialog.h"
#include "gamesettings.h"
#include "sfenvalidationservice.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QTextStream>

namespace {

/// 局面集の1行を局面の SFEN にする。局面として読めない行は空文字列。
/// SFEN（「sfen …」も可）と、USI の「position …」「startpos」を受け付ける。
/// USI の position コマンドは moves の指し手を指した後の局面にする。SFEN の後の moves は
/// 詰将棋局面生成が手順を含めて保存した解答手順なので、SFEN の局面のままにする。
QString sfenFromLine(const QString& line)
{
    QStringList tokens = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    bool usiCommand = false;
    if (!tokens.isEmpty() && tokens.first().compare(QStringLiteral("position"), Qt::CaseInsensitive) == 0) {
        tokens.removeFirst();
        usiCommand = true;
    }
    if (!tokens.isEmpty() && tokens.first().compare(QStringLiteral("sfen"), Qt::CaseInsensitive) == 0) {
        tokens.removeFirst();
    } else if (!tokens.isEmpty() && tokens.first() == QStringLiteral("startpos")) {
        usiCommand = true;
    }

    QStringList moves;
    const qsizetype movesIndex = tokens.indexOf(QStringLiteral("moves"));
    if (movesIndex >= 0) {
        if (usiCommand) {
            moves = tokens.mid(movesIndex + 1);
        }
        tokens = tokens.first(movesIndex);
    }

    const SfenValidation validation = SfenValidationService::validate(tokens.join(QLatin1Char(' ')));
    if (!validation.valid) {
        return {};
    }
    if (moves.isEmpty()) {
        return validation.normalizedSfen;
    }
    return SfenValidationService::applyUsiMoves(validation.normalizedSfen, moves);
}

} // namespace

void SfenCollectionDialog::onOpenFileClicked()
{
    QString lastDir = GameSettings::sfenCollectionLastDirectory();
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("SFEN局面集ファイルを開く"),
        lastDir,
        tr("テキストファイル (*.txt *.sfen);;すべてのファイル (*)"));

    if (!filePath.isEmpty()) {
        loadFromFile(filePath);
    }
}

bool SfenCollectionDialog::loadFromFile(const QString& filePath)
{
    QFileInfo fi(filePath);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルを開けませんでした: %1").arg(fi.fileName()));
        return false;
    }

    QTextStream in(&file);
    QString text = in.readAll();
    file.close();

    // 局面が1つもないファイルでは、表示中の局面集をそのまま残す
    const QList<QString> previousList = m_sfenList;
    const int skippedLines = parseSfenLines(text);
    if (m_sfenList.isEmpty()) {
        m_sfenList = previousList;
        QMessageBox::warning(this, tr("エラー"),
                             tr("%1 には局面がありません。\n"
                                "1行に1局面のSFENを書いたファイルを選んでください。").arg(fi.fileName()));
        return false;
    }

    m_currentIndex = 0;

    // ファイル名ラベルを更新（局面として読めなかった行があれば数を添える）
    m_fileLabel->setText(skippedLines > 0
                             ? tr("ファイル: %1（読めない %2 行を飛ばしました）").arg(fi.fileName(), QString::number(skippedLines))
                             : tr("ファイル: %1").arg(fi.fileName()));
    m_fileLabel->setToolTip(fi.absoluteFilePath());

    // 最後に開いたディレクトリを保存
    GameSettings::setSfenCollectionLastDirectory(fi.absolutePath());

    // 最近使ったファイルリストに追加
    addToRecentFiles(filePath);
    saveRecentFiles();

    updateBoardDisplay();
    updateButtonStates();
    return true;
}

int SfenCollectionDialog::parseSfenLines(const QString& text)
{
    m_sfenList.clear();
    int skippedLines = 0;

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : std::as_const(lines)) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        // コメント行（'#' 始まり。詰将棋局面生成のファイル保存が先頭に付ける）は読み飛ばす
        if (trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QString sfen = sfenFromLine(trimmed);
        if (sfen.isEmpty()) {
            ++skippedLines;
        } else {
            m_sfenList.append(sfen);
        }
    }
    return skippedLines;
}

void SfenCollectionDialog::addToRecentFiles(const QString& filePath)
{
    // 既に存在する場合は削除（先頭に移動するため）
    m_recentFiles.removeAll(filePath);

    // 先頭に追加
    m_recentFiles.prepend(filePath);

    // 最大5件に制限
    while (m_recentFiles.size() > 5) {
        m_recentFiles.removeLast();
    }

    // メニューを更新
    updateRecentFilesMenu();
}

void SfenCollectionDialog::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();

    if (m_recentFiles.isEmpty()) {
        QAction* emptyAction = m_recentFilesMenu->addAction(tr("（履歴なし）"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString& filePath : std::as_const(m_recentFiles)) {
        QFileInfo fi(filePath);
        QString displayName = fi.fileName();

        QAction* action = m_recentFilesMenu->addAction(displayName);
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &SfenCollectionDialog::onRecentFileClicked);
    }

    m_recentFilesMenu->addSeparator();

    QAction* clearAction = m_recentFilesMenu->addAction(tr("履歴をクリア"));
    connect(clearAction, &QAction::triggered, this, &SfenCollectionDialog::onClearRecentFilesClicked);
}

void SfenCollectionDialog::saveRecentFiles()
{
    GameSettings::setSfenCollectionRecentFiles(m_recentFiles);
}

void SfenCollectionDialog::onRecentFileClicked()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }

    QString filePath = action->data().toString();
    if (filePath.isEmpty()) {
        return;
    }

    // ファイルが存在するか確認
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルが見つかりません: %1").arg(filePath));
        m_recentFiles.removeAll(filePath);
        updateRecentFilesMenu();
        saveRecentFiles();
        return;
    }

    loadFromFile(filePath);
}

void SfenCollectionDialog::onClearRecentFilesClicked()
{
    m_recentFiles.clear();
    updateRecentFilesMenu();
    saveRecentFiles();
}
