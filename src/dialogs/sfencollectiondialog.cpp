/// @file sfencollectiondialog.cpp
/// @brief SFEN局面集ビューアダイアログクラスの実装

#include "sfencollectiondialog.h"
#include "buttonstyles.h"
#include "boardviewerwidget.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "gamesettings.h"
#include "appsettings.h"
#include "dialogfontscale.h"
#include "sfenutils.h"
#include "positioncollectionfile.h"
#include "recentfilesmenu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMenu>
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

    buildUi();
    // ウィンドウは開くたびに盤（保存したマスの大きさ）に合わせるので、ウィンドウの大きさは保存しない
    DialogFontScale::install(this, AppSettings::sfenCollectionDialogFontSize);

    // 最近使ったファイルリストを読み込み
    m_recentFiles.setFiles(GameSettings::sfenCollectionRecentFiles());
    m_recentFilesMenu->setFiles(m_recentFiles.files());

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
    auto* recentMenu = new QMenu(this);
    recentMenu->setObjectName(QStringLiteral("sfenCollectionRecentMenu"));
    recentMenu->setProperty("automationMenu", true);
    m_btnRecentFiles->setMenu(recentMenu);
    m_recentFilesMenu = new RecentFilesMenu(recentMenu);
    connect(m_recentFilesMenu, &RecentFilesMenu::fileTriggered, this, &SfenCollectionDialog::onRecentFileTriggered);
    connect(m_recentFilesMenu, &RecentFilesMenu::clearTriggered,
            this, &SfenCollectionDialog::onClearRecentFilesTriggered);
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

void SfenCollectionDialog::done(int result)
{
    // 閉じるボタン・Esc・ウィンドウの×のどれでも通る
    GameSettings::sfenCollectionRecentFiles.set(m_recentFiles.files());
    GameSettings::sfenCollectionSquareSize.set(m_viewer->view()->squareSize());
    QDialog::done(result);
}

// ============================================================
// SFEN局面集ビューアのファイル読み込み・行の解析・最近使ったファイル
// ============================================================

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
    m_recentFiles.add(filePath);
    onRecentFilesChanged();

    m_viewer->setSfenList(sfens);
    m_btnSelect->setEnabled(true);
    return true;
}

QStringList SfenCollectionDialog::parseSfenLines(const QString& text, int* skippedLines)
{
    // 規則は詰将棋問題一覧と同じ（PositionCollectionFile）。position コマンドの行は指した後の局面にする
    const PositionCollectionFile::Contents contents = PositionCollectionFile::parse(text);
    QStringList sfens;
    int skipped = static_cast<int>(contents.invalidLines.size());
    for (const auto& line : contents.lines) {
        const QString sfen = line.position();
        if (sfen.isEmpty()) {
            ++skipped;
        } else {
            sfens.append(sfen);
        }
    }
    if (skippedLines) *skippedLines = skipped;
    return sfens;
}

void SfenCollectionDialog::onRecentFileTriggered(const QString& filePath)
{
    if (filePath.isEmpty()) return;
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルが見つかりません: %1").arg(filePath));
        if (m_recentFiles.remove(filePath)) onRecentFilesChanged();
        return;
    }
    loadFromFile(filePath);
}

void SfenCollectionDialog::onClearRecentFilesTriggered()
{
    m_recentFiles.clear();
    onRecentFilesChanged();
}

void SfenCollectionDialog::onRecentFilesChanged()
{
    m_recentFilesMenu->setFiles(m_recentFiles.files());
    GameSettings::sfenCollectionRecentFiles.set(m_recentFiles.files());
}
