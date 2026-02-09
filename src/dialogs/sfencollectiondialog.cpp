/// @file sfencollectiondialog.cpp
/// @brief SFEN局面集ビューアダイアログクラスの実装

#include "sfencollectiondialog.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "settingsservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QCloseEvent>
#include <QWheelEvent>
#include <QTimer>

SfenCollectionDialog::SfenCollectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("局面集ビューア"));
    setMinimumSize(400, 500);

    // 前回保存されたウィンドウサイズを読み込む
    QSize savedSize = SettingsService::sfenCollectionDialogSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    } else {
        resize(620, 780);
    }

    // UIを構築
    buildUi();

    // 初期盤面（平手初期局面）を設定
    m_board->setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
    m_shogiView->updateBoardSize();
    hideClockLabels();

    // ボタン状態を初期化（ファイル未読み込み状態）
    updateButtonStates();

    // レイアウト確定後に自動調整
    QTimer::singleShot(0, this, &SfenCollectionDialog::adjustWindowToContents);
}

SfenCollectionDialog::~SfenCollectionDialog()
{
    // m_board と m_shogiView は Qt のオブジェクトツリーで管理
}

void SfenCollectionDialog::buildUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ファイルを開くボタン + ファイル名ラベル
    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->setSpacing(8);

    m_btnOpenFile = new QPushButton(tr("ファイルを開く"), this);
    m_btnOpenFile->setToolTip(tr("SFEN局面集ファイルを開く"));
    connect(m_btnOpenFile, &QPushButton::clicked,
            this, &SfenCollectionDialog::onOpenFileClicked);
    fileLayout->addWidget(m_btnOpenFile);

    m_fileLabel = new QLabel(this);
    m_fileLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #555; padding: 4px; }"));
    fileLayout->addWidget(m_fileLabel, 1);

    mainLayout->addLayout(fileLayout);

    // 将棋盤拡大・縮小ボタン
    QHBoxLayout* zoomLayout = new QHBoxLayout();
    zoomLayout->setSpacing(4);

    m_btnReduce = new QPushButton(QStringLiteral("将棋盤縮小 ➖"), this);
    m_btnReduce->setToolTip(tr("将棋盤を縮小する"));
    connect(m_btnReduce, &QPushButton::clicked,
            this, &SfenCollectionDialog::onReduceBoard);
    zoomLayout->addWidget(m_btnReduce);

    m_btnEnlarge = new QPushButton(QStringLiteral("将棋盤拡大 ➕"), this);
    m_btnEnlarge->setToolTip(tr("将棋盤を拡大する"));
    connect(m_btnEnlarge, &QPushButton::clicked,
            this, &SfenCollectionDialog::onEnlargeBoard);
    zoomLayout->addWidget(m_btnEnlarge);

    m_btnFlip = new QPushButton(tr("盤面の回転"), this);
    m_btnFlip->setToolTip(tr("盤面を回転する"));
    connect(m_btnFlip, &QPushButton::clicked,
            this, &SfenCollectionDialog::onFlipBoard);
    zoomLayout->addWidget(m_btnFlip);

    zoomLayout->addStretch();
    mainLayout->addLayout(zoomLayout);

    // 将棋盤
    m_board = new ShogiBoard(9, 9, this);
    m_shogiView = new ShogiView(this);
    m_shogiView->setMouseClickMode(false);
    m_shogiView->setNameFontScale(0.30);

    m_shogiView->setPieces();
    m_shogiView->setBoard(m_board);

    // Ctrl+ホイールイベントを横取り
    m_shogiView->installEventFilter(this);

    m_shogiView->setBlackPlayerName(tr("先手"));
    m_shogiView->setWhitePlayerName(tr("後手"));

    mainLayout->addWidget(m_shogiView, 1);

    // 局面ラベル
    m_positionLabel = new QLabel(this);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 14px; font-weight: bold; padding: 4px; }"));
    mainLayout->addWidget(m_positionLabel);

    // ナビゲーションボタン
    QHBoxLayout* navLayout = new QHBoxLayout();
    navLayout->setSpacing(8);

    m_btnFirst = new QPushButton(QStringLiteral("⏮ 最初"), this);
    m_btnFirst->setMinimumWidth(80);
    m_btnFirst->setToolTip(tr("最初の局面に移動"));
    connect(m_btnFirst, &QPushButton::clicked,
            this, &SfenCollectionDialog::onGoFirst);
    navLayout->addWidget(m_btnFirst);

    m_btnBack = new QPushButton(QStringLiteral("◀ 前へ"), this);
    m_btnBack->setMinimumWidth(80);
    m_btnBack->setToolTip(tr("前の局面に移動"));
    connect(m_btnBack, &QPushButton::clicked,
            this, &SfenCollectionDialog::onGoBack);
    navLayout->addWidget(m_btnBack);

    m_btnForward = new QPushButton(QStringLiteral("次へ ▶"), this);
    m_btnForward->setMinimumWidth(80);
    m_btnForward->setToolTip(tr("次の局面に移動"));
    connect(m_btnForward, &QPushButton::clicked,
            this, &SfenCollectionDialog::onGoForward);
    navLayout->addWidget(m_btnForward);

    m_btnLast = new QPushButton(QStringLiteral("最後 ⏭"), this);
    m_btnLast->setMinimumWidth(80);
    m_btnLast->setToolTip(tr("最後の局面に移動"));
    connect(m_btnLast, &QPushButton::clicked,
            this, &SfenCollectionDialog::onGoLast);
    navLayout->addWidget(m_btnLast);

    mainLayout->addLayout(navLayout);

    // 選択・閉じるボタン
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);
    actionLayout->addStretch();

    m_btnSelect = new QPushButton(tr("選択"), this);
    m_btnSelect->setMinimumWidth(100);
    m_btnSelect->setToolTip(tr("現在の局面をメインGUIに反映する"));
    connect(m_btnSelect, &QPushButton::clicked,
            this, &SfenCollectionDialog::onSelectClicked);
    actionLayout->addWidget(m_btnSelect);

    QPushButton* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    connect(closeBtn, &QPushButton::clicked,
            this, &QDialog::close);
    actionLayout->addWidget(closeBtn);

    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);
}

void SfenCollectionDialog::onOpenFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("SFEN局面集ファイルを開く"),
        QString(),
        tr("テキストファイル (*.txt *.sfen);;すべてのファイル (*)"));

    if (!filePath.isEmpty()) {
        loadFromFile(filePath);
    }
}

bool SfenCollectionDialog::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString text = in.readAll();
    file.close();

    parseSfenLines(text);

    if (m_sfenList.isEmpty()) {
        return false;
    }

    m_currentFilePath = filePath;
    m_currentIndex = 0;

    // ファイル名ラベルを更新
    QFileInfo fi(filePath);
    m_fileLabel->setText(tr("ファイル: %1").arg(fi.fileName()));

    updateBoardDisplay();
    updateButtonStates();
    return true;
}

void SfenCollectionDialog::parseSfenLines(const QString& text)
{
    m_sfenList.clear();

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : std::as_const(lines)) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        // "sfen " プレフィックスを除去
        if (trimmed.startsWith(QStringLiteral("sfen "), Qt::CaseInsensitive)) {
            trimmed = trimmed.mid(5);
        }
        // "position sfen " プレフィックスを除去
        if (trimmed.startsWith(QStringLiteral("position sfen "), Qt::CaseInsensitive)) {
            trimmed = trimmed.mid(14);
        }

        // SFEN形式の検証: 最低4パート（盤面/手番/持ち駒/手数）
        QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 4) {
            m_sfenList.append(trimmed);
        }
    }
}

void SfenCollectionDialog::updateBoardDisplay()
{
    if (m_sfenList.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_sfenList.size()) {
        return;
    }

    const QString& sfen = m_sfenList.at(m_currentIndex);
    m_board->setSfen(sfen);
    m_shogiView->update();

    // 手番表示を更新
    bool isBlackTurn = !sfen.contains(QStringLiteral(" w "));
    m_shogiView->setActiveSide(isBlackTurn);
    m_shogiView->updateTurnIndicator(isBlackTurn ? ShogiGameController::Player1
                                                  : ShogiGameController::Player2);

    // 位置ラベルを更新
    m_positionLabel->setText(tr("局面: %1 / %2").arg(m_currentIndex + 1).arg(m_sfenList.size()));
}

void SfenCollectionDialog::updateButtonStates()
{
    const bool hasData = !m_sfenList.isEmpty();
    const bool canGoBack = hasData && m_currentIndex > 0;
    const bool canGoForward = hasData && m_currentIndex < m_sfenList.size() - 1;

    m_btnFirst->setEnabled(canGoBack);
    m_btnBack->setEnabled(canGoBack);
    m_btnForward->setEnabled(canGoForward);
    m_btnLast->setEnabled(canGoForward);
    m_btnSelect->setEnabled(hasData);
}

void SfenCollectionDialog::onGoFirst()
{
    if (!m_sfenList.isEmpty()) {
        m_currentIndex = 0;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void SfenCollectionDialog::onGoBack()
{
    if (m_currentIndex > 0) {
        --m_currentIndex;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void SfenCollectionDialog::onGoForward()
{
    if (m_currentIndex < m_sfenList.size() - 1) {
        ++m_currentIndex;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void SfenCollectionDialog::onGoLast()
{
    if (!m_sfenList.isEmpty()) {
        m_currentIndex = static_cast<int>(m_sfenList.size()) - 1;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void SfenCollectionDialog::onEnlargeBoard()
{
    if (m_shogiView) {
        m_shogiView->enlargeBoard(false);
        hideClockLabels();
        adjustSize();
    }
}

void SfenCollectionDialog::onReduceBoard()
{
    if (m_shogiView) {
        m_shogiView->reduceBoard(false);
        hideClockLabels();
        adjustSize();
    }
}

void SfenCollectionDialog::onFlipBoard()
{
    if (!m_shogiView) return;

    const bool flipped = !m_shogiView->getFlipMode();
    m_shogiView->setFlipMode(flipped);
    if (flipped) {
        m_shogiView->setPiecesFlip();
    } else {
        m_shogiView->setPieces();
    }
    m_shogiView->update();
}

void SfenCollectionDialog::onSelectClicked()
{
    if (!m_sfenList.isEmpty() && m_currentIndex >= 0 && m_currentIndex < m_sfenList.size()) {
        emit positionSelected(m_sfenList.at(m_currentIndex));
    }
}

bool SfenCollectionDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_shogiView && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            const int delta = wheelEvent->angleDelta().y();
            if (delta > 0) {
                onEnlargeBoard();
            } else if (delta < 0) {
                onReduceBoard();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SfenCollectionDialog::hideClockLabels()
{
    if (m_shogiView) {
        m_shogiView->setClockEnabled(false);
    }
}

void SfenCollectionDialog::saveWindowSize()
{
    SettingsService::setSfenCollectionDialogSize(size());
}

void SfenCollectionDialog::adjustWindowToContents()
{
    if (m_shogiView) {
        m_shogiView->updateBoardSize();
        hideClockLabels();
    }
    adjustSize();
}

void SfenCollectionDialog::closeEvent(QCloseEvent* event)
{
    saveWindowSize();
    QDialog::closeEvent(event);
}
