/// @file sfencollectiondialog.cpp
/// @brief SFEN局面集ビューアダイアログクラスの実装

#include "sfencollectiondialog.h"
#include "buttonstyles.h"
#include "boardviewerwidget.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "gamesettings.h"
#include "dialogutils.h"
#include "appsettings.h"
#include "dialogfontscale.h"
#include "sfenutils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>
#include "sfenvalidationservice.h"
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>

namespace {
constexpr QSize kMinimumSize{400, 500};
} // namespace

SfenCollectionDialog::SfenCollectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("局面集ビューア"));
    setMinimumSize(kMinimumSize);

    // 前回保存されたウィンドウサイズを読み込む
    DialogUtils::restoreDialogSize(this, GameSettings::sfenCollectionDialogSize());

    buildUi();
    DialogFontScale::install(this, AppSettings::sfenCollectionDialogFontSize);

    // 最近使ったファイルリストを読み込み
    m_recentFiles = GameSettings::sfenCollectionRecentFiles();
    updateRecentFilesMenu();

    // 前回保存された将棋盤マスサイズを復元
    const int savedSquareSize = GameSettings::sfenCollectionSquareSize();
    if (savedSquareSize >= 20 && savedSquareSize <= 150) {
        m_viewer->view()->setSquareSize(savedSquareSize);
    }

    // 初期盤面（平手初期局面）を表示する。局面集はまだ無い
    m_viewer->board()->setSfen(SfenUtils::hirateSfen());
    m_viewer->fitToBoard();

    // レイアウト確定後に自動調整
    QTimer::singleShot(0, m_viewer, &BoardViewerWidget::fitToBoard);
}

void SfenCollectionDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ファイルを開くボタン + ファイル名ラベル
    auto* fileLayout = new QHBoxLayout();
    fileLayout->setSpacing(8);

    m_btnOpenFile = new QPushButton(tr("ファイルを開く"), this);
    m_btnOpenFile->setToolTip(tr("SFEN局面集ファイルを開く"));
    m_btnOpenFile->setStyleSheet(ButtonStyles::fileOperation());
    connect(m_btnOpenFile, &QPushButton::clicked,
            this, &SfenCollectionDialog::onOpenFileClicked);
    fileLayout->addWidget(m_btnOpenFile);

    m_btnRecentFiles = new QPushButton(tr("履歴"), this);
    m_btnRecentFiles->setToolTip(tr("最近使ったファイルを開く"));
    m_btnRecentFiles->setStyleSheet(ButtonStyles::fileOperation());
    m_recentFilesMenu = new QMenu(this);
    m_recentFilesMenu->setObjectName(QStringLiteral("sfenCollectionRecentMenu"));
    m_recentFilesMenu->setProperty("automationMenu", true);
    m_btnRecentFiles->setMenu(m_recentFilesMenu);
    fileLayout->addWidget(m_btnRecentFiles);

    m_fileLabel = new QLabel(tr("「ファイルを開く」から局面集を選んでください。"), this);
    m_fileLabel->setObjectName(QStringLiteral("collectionFileLabel"));
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setTextFormat(Qt::PlainText);
    m_fileLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #555; padding: 4px; }"));
    fileLayout->addWidget(m_fileLabel, 1);

    mainLayout->addLayout(fileLayout);

    // 盤・拡大縮小・回転・局面送り
    m_viewer = new BoardViewerWidget(BoardViewerWidget::ZoomToolbar | BoardViewerWidget::Navigation
                                         | BoardViewerWidget::ScrollArea | BoardViewerWidget::FitWindowOnZoom,
                                     BoardViewerWidget::ButtonLabels::Words, nullptr, this);
    m_viewer->setPositionText(tr("局面集が読み込まれていません"));
    connect(m_viewer, &BoardViewerWidget::currentIndexChanged,
            this, &SfenCollectionDialog::onCurrentIndexChanged);
    mainLayout->addWidget(m_viewer, 1);

    // 選択・閉じるボタン
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);
    actionLayout->addStretch();

    m_btnSelect = new QPushButton(tr("選択"), this);
    m_btnSelect->setMinimumWidth(100);
    m_btnSelect->setToolTip(tr("現在の局面をメインGUIに反映する"));
    m_btnSelect->setStyleSheet(ButtonStyles::primaryAction());
    m_btnSelect->setEnabled(false);
    connect(m_btnSelect, &QPushButton::clicked,
            this, &SfenCollectionDialog::onSelectClicked);
    actionLayout->addWidget(m_btnSelect);

    auto* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    closeBtn->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(closeBtn, &QPushButton::clicked,
            this, &QDialog::close);
    actionLayout->addWidget(closeBtn);

    mainLayout->addLayout(actionLayout);
}

void SfenCollectionDialog::onCurrentIndexChanged(int index)
{
    m_viewer->setPositionText(tr("局面: %1 / %2").arg(index + 1).arg(m_viewer->count()));
}

void SfenCollectionDialog::onSelectClicked()
{
    const QString sfen = m_viewer->currentSfen();
    if (!sfen.isEmpty()) emit positionSelected(sfen);
}

void SfenCollectionDialog::closeEvent(QCloseEvent* event)
{
    DialogUtils::saveDialogSize(this, GameSettings::sfenCollectionDialogSize);
    saveRecentFiles();
    GameSettings::sfenCollectionSquareSize.set(m_viewer->view()->squareSize());
    QDialog::closeEvent(event);
}

// ============================================================
// SFEN局面集ビューアのファイル読み込み・行の解析・最近使ったファイル
// ============================================================

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
    int skippedLines = 0;
    const QStringList sfens = parseSfenLines(text, &skippedLines);
    if (sfens.isEmpty()) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("%1 には局面がありません。\n"
                                "1行に1局面のSFENを書いたファイルを選んでください。").arg(fi.fileName()));
        return false;
    }

    // ファイル名ラベルを更新（局面として読めなかった行があれば数を添える）
    m_fileLabel->setText(skippedLines > 0
                             ? tr("ファイル: %1（読めない %2 行を飛ばしました）").arg(fi.fileName(), QString::number(skippedLines))
                             : tr("ファイル: %1").arg(fi.fileName()));
    m_fileLabel->setToolTip(fi.absoluteFilePath());

    // 最後に開いたディレクトリを保存
    GameSettings::sfenCollectionLastDirectory.set(fi.absolutePath());

    // 最近使ったファイルリストに追加
    addToRecentFiles(filePath);
    saveRecentFiles();

    m_viewer->setSfenList(sfens);
    m_btnSelect->setEnabled(true);
    return true;
}

QStringList SfenCollectionDialog::parseSfenLines(const QString& text, int* skippedLines)
{
    QStringList sfens;
    int skipped = 0;

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
            ++skipped;
        } else {
            sfens.append(sfen);
        }
    }
    if (skippedLines) *skippedLines = skipped;
    return sfens;
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
    GameSettings::sfenCollectionRecentFiles.set(m_recentFiles);
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
