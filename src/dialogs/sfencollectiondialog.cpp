/// @file sfencollectiondialog.cpp
/// @brief SFEN局面集ビューアダイアログクラスの実装

#include "sfencollectiondialog.h"
#include "buttonstyles.h"
#include "boardviewerwidget.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "gamesettings.h"
#include "dialogutils.h"
#include "dialogfontscale.h"
#include "sfenutils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>

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
    DialogFontScale::install(this, QStringLiteral("sfenCollection"));

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
