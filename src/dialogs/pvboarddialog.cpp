#include "pvboarddialog.h"
#include "shogiview.h"
#include "shogiboard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QRegularExpression>

// 前方宣言: SFENからUSI形式の手を盤面に適用する静的ヘルパー
static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove);

PvBoardDialog::PvBoardDialog(const QString& baseSfen,
                             const QStringList& pvMoves,
                             QWidget* parent)
    : QDialog(parent)
    , m_baseSfen(baseSfen)
    , m_pvMoves(pvMoves)
    , m_currentPly(0)
{
    qDebug() << "[PvBoardDialog] Constructor called";
    qDebug() << "[PvBoardDialog] baseSfen:" << baseSfen;
    qDebug() << "[PvBoardDialog] pvMoves:" << pvMoves;
    
    setWindowTitle(tr("読み筋表示"));
    setMinimumSize(620, 680);
    resize(650, 720);

    // 初期盤面を履歴に追加
    m_sfenHistory.clear();
    m_sfenHistory.append(m_baseSfen);
    qDebug() << "[PvBoardDialog] Initial SFEN added to history";

    // 各手を適用してSFEN履歴を構築
    ShogiBoard tempBoard;
    tempBoard.setSfen(m_baseSfen);
    qDebug() << "[PvBoardDialog] tempBoard setSfen done";

    for (int i = 0; i < m_pvMoves.size(); ++i) {
        const QString& move = m_pvMoves.at(i);
        qDebug() << "[PvBoardDialog] Applying move" << i << ":" << move;
        applyUsiMoveToBoard(&tempBoard, move);
        // 手番を切り替え
        QString nextTurn = (i % 2 == 0) ? QStringLiteral("w") : QStringLiteral("b");
        if (m_baseSfen.contains(QStringLiteral(" w "))) {
            nextTurn = (i % 2 == 0) ? QStringLiteral("b") : QStringLiteral("w");
        }
        QString nextSfen = tempBoard.convertBoardToSfen() + QStringLiteral(" ") +
                          nextTurn + QStringLiteral(" ") +
                          tempBoard.convertStandToSfen() + QStringLiteral(" 1");
        m_sfenHistory.append(nextSfen);
        qDebug() << "[PvBoardDialog] Generated SFEN" << (i+1) << ":" << nextSfen;
    }

    qDebug() << "[PvBoardDialog] SFEN history size:" << m_sfenHistory.size();

    // UIを構築（この中でm_boardが作られる）
    buildUi();
    qDebug() << "[PvBoardDialog] buildUi done";

    // 初期盤面を設定
    if (!m_sfenHistory.isEmpty()) {
        qDebug() << "[PvBoardDialog] Setting initial SFEN to m_board:" << m_sfenHistory.at(0);
        m_board->setSfen(m_sfenHistory.at(0));
        qDebug() << "[PvBoardDialog] m_board setSfen done";
    }
    
    // 初期表示
    updateBoardDisplay();
    updateButtonStates();
    qDebug() << "[PvBoardDialog] Constructor finished";
}

PvBoardDialog::~PvBoardDialog()
{
    // m_board と m_shogiView は Qt のオブジェクトツリーで管理
}

void PvBoardDialog::setKanjiPv(const QString& kanjiPv)
{
    m_kanjiPv = kanjiPv;
    if (m_pvLabel) {
        m_pvLabel->setText(m_kanjiPv);
    }
}

void PvBoardDialog::buildUi()
{
    qDebug() << "[PvBoardDialog::buildUi] Start";
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 読み筋ラベル（スクロール可能なラベル）
    m_pvLabel = new QLabel(this);
    m_pvLabel->setWordWrap(true);
    m_pvLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #f5f5f5; border: 1px solid #ccc; "
        "padding: 8px; font-size: 12px; }"));
    m_pvLabel->setMinimumHeight(60);
    m_pvLabel->setMaximumHeight(100);
    mainLayout->addWidget(m_pvLabel);
    qDebug() << "[PvBoardDialog::buildUi] pvLabel created";

    // 将棋盤
    m_board = new ShogiBoard(9, 9, this);
    qDebug() << "[PvBoardDialog::buildUi] ShogiBoard created, m_board=" << m_board;
    
    m_shogiView = new ShogiView(this);
    qDebug() << "[PvBoardDialog::buildUi] ShogiView created, m_shogiView=" << m_shogiView;
    
    m_shogiView->setMouseClickMode(false);  // クリック操作は無効
    
    // 明示的にサイズを設定（フォント計算のエラーを防ぐ）
    // 駒台も含めた全体が表示されるよう十分な幅を確保
    m_shogiView->setFieldSize(QSize(40, 40));  // 1マスのサイズを少し小さく
    m_shogiView->setMinimumSize(580, 500);
    m_shogiView->setFixedSize(580, 500);  // 固定サイズを設定
    
    // 駒画像を読み込んでから盤を設定
    qDebug() << "[PvBoardDialog::buildUi] Calling setPieces()...";
    m_shogiView->setPieces();
    qDebug() << "[PvBoardDialog::buildUi] setPieces() done";
    
    qDebug() << "[PvBoardDialog::buildUi] Calling setBoard()...";
    m_shogiView->setBoard(m_board);
    qDebug() << "[PvBoardDialog::buildUi] setBoard() done, view->board()=" << m_shogiView->board();
    
    // 時計・名前ラベルを非表示にする
    qDebug() << "[PvBoardDialog::buildUi] blackClockLabel=" << m_shogiView->blackClockLabel();
    qDebug() << "[PvBoardDialog::buildUi] whiteClockLabel=" << m_shogiView->whiteClockLabel();
    qDebug() << "[PvBoardDialog::buildUi] blackNameLabel=" << m_shogiView->blackNameLabel();
    qDebug() << "[PvBoardDialog::buildUi] whiteNameLabel=" << m_shogiView->whiteNameLabel();
    
    if (m_shogiView->blackClockLabel()) {
        m_shogiView->blackClockLabel()->hide();
        qDebug() << "[PvBoardDialog::buildUi] blackClockLabel hidden";
    }
    if (m_shogiView->whiteClockLabel()) {
        m_shogiView->whiteClockLabel()->hide();
        qDebug() << "[PvBoardDialog::buildUi] whiteClockLabel hidden";
    }
    if (m_shogiView->blackNameLabel()) {
        m_shogiView->blackNameLabel()->hide();
        qDebug() << "[PvBoardDialog::buildUi] blackNameLabel hidden";
    }
    if (m_shogiView->whiteNameLabel()) {
        m_shogiView->whiteNameLabel()->hide();
        qDebug() << "[PvBoardDialog::buildUi] whiteNameLabel hidden";
    }
    
    mainLayout->addWidget(m_shogiView, 1);
    qDebug() << "[PvBoardDialog::buildUi] shogiView added to layout";

    // 手数ラベル
    m_plyLabel = new QLabel(this);
    m_plyLabel->setAlignment(Qt::AlignCenter);
    m_plyLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 14px; font-weight: bold; padding: 4px; }"));
    mainLayout->addWidget(m_plyLabel);

    // ナビゲーションボタン
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnFirst = new QPushButton(QStringLiteral("⏮ 最初"), this);
    m_btnFirst->setMinimumWidth(80);
    m_btnFirst->setToolTip(tr("最初の局面に戻る"));
    connect(m_btnFirst, &QPushButton::clicked, this, &PvBoardDialog::onGoFirst);
    btnLayout->addWidget(m_btnFirst);

    m_btnBack = new QPushButton(QStringLiteral("◀ 1手戻る"), this);
    m_btnBack->setMinimumWidth(100);
    m_btnBack->setToolTip(tr("1手戻る"));
    connect(m_btnBack, &QPushButton::clicked, this, &PvBoardDialog::onGoBack);
    btnLayout->addWidget(m_btnBack);

    m_btnForward = new QPushButton(QStringLiteral("1手進む ▶"), this);
    m_btnForward->setMinimumWidth(100);
    m_btnForward->setToolTip(tr("1手進む"));
    connect(m_btnForward, &QPushButton::clicked, this, &PvBoardDialog::onGoForward);
    btnLayout->addWidget(m_btnForward);

    m_btnLast = new QPushButton(QStringLiteral("最後 ⏭"), this);
    m_btnLast->setMinimumWidth(80);
    m_btnLast->setToolTip(tr("最後の局面まで進む"));
    connect(m_btnLast, &QPushButton::clicked, this, &PvBoardDialog::onGoLast);
    btnLayout->addWidget(m_btnLast);

    mainLayout->addLayout(btnLayout);

    // 閉じるボタン
    QPushButton* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout* closeBtnLayout = new QHBoxLayout();
    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);
    closeBtnLayout->addStretch();
    mainLayout->addLayout(closeBtnLayout);
}

void PvBoardDialog::updateButtonStates()
{
    const int maxPly = m_pvMoves.size();

    m_btnFirst->setEnabled(m_currentPly > 0);
    m_btnBack->setEnabled(m_currentPly > 0);
    m_btnForward->setEnabled(m_currentPly < maxPly);
    m_btnLast->setEnabled(m_currentPly < maxPly);
}

void PvBoardDialog::updateBoardDisplay()
{
    qDebug() << "[PvBoardDialog::updateBoardDisplay] currentPly=" << m_currentPly 
             << ", historySize=" << m_sfenHistory.size();
    
    if (m_currentPly < 0 || m_currentPly >= m_sfenHistory.size()) {
        qDebug() << "[PvBoardDialog::updateBoardDisplay] Out of range, returning";
        return;
    }

    // 現在の局面SFENを取得して盤面を更新
    const QString& sfen = m_sfenHistory.at(m_currentPly);
    qDebug() << "[PvBoardDialog::updateBoardDisplay] SFEN to set:" << sfen;
    qDebug() << "[PvBoardDialog::updateBoardDisplay] m_board=" << m_board;
    qDebug() << "[PvBoardDialog::updateBoardDisplay] m_shogiView=" << m_shogiView;
    qDebug() << "[PvBoardDialog::updateBoardDisplay] m_shogiView->board()=" << m_shogiView->board();
    
    m_board->setSfen(sfen);
    qDebug() << "[PvBoardDialog::updateBoardDisplay] setSfen done";
    
    // 盤面データの確認
    qDebug() << "[PvBoardDialog::updateBoardDisplay] Board data after setSfen:";
    qDebug() << "  files=" << m_board->files() << ", ranks=" << m_board->ranks();
    
    m_shogiView->update();
    qDebug() << "[PvBoardDialog::updateBoardDisplay] view update() called";

    // 手数ラベルを更新
    const int totalMoves = m_pvMoves.size();
    QString plyText = tr("手数: %1 / %2").arg(m_currentPly).arg(totalMoves);
    if (m_currentPly == 0) {
        plyText += tr(" (開始局面)");
    } else if (m_currentPly <= m_pvMoves.size()) {
        // 現在の手を表示
        plyText += QStringLiteral(" [%1]").arg(m_pvMoves.at(m_currentPly - 1));
    }
    m_plyLabel->setText(plyText);
}

void PvBoardDialog::onGoFirst()
{
    m_currentPly = 0;
    updateBoardDisplay();
    updateButtonStates();
}

void PvBoardDialog::onGoBack()
{
    if (m_currentPly > 0) {
        --m_currentPly;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void PvBoardDialog::onGoForward()
{
    if (m_currentPly < m_pvMoves.size()) {
        ++m_currentPly;
        updateBoardDisplay();
        updateButtonStates();
    }
}

void PvBoardDialog::onGoLast()
{
    m_currentPly = m_pvMoves.size();
    updateBoardDisplay();
    updateButtonStates();
}

// SFENからUSI形式の手を盤面に適用する静的ヘルパー
static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove)
{
    if (!board || usiMove.isEmpty()) return;

    // USI形式の手を解析
    // 例: "7g7f" (移動), "P*5e" (打つ)
    if (usiMove.length() < 4) return;

    // 打つ手（駒打ち）の場合
    if (usiMove.at(1) == '*') {
        // 例: "P*5e"
        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';  // '5' -> 5
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;  // 'e' -> 5

        // 手番に応じて駒文字を調整
        QString turn = board->currentPlayer();
        if (turn == QStringLiteral("w")) {
            pieceChar = pieceChar.toLower();
        }

        // 駒を配置
        board->movePieceToSquare(pieceChar, 0, 0, toFile, toRank, false);

        // 駒台から減らす
        QMap<QChar, int>& stand = board->m_pieceStand;
        if (stand.contains(pieceChar) && stand[pieceChar] > 0) {
            stand[pieceChar]--;
        }
        return;
    }

    // 移動手の場合
    // 例: "7g7f" または "7g7f+"
    int fromFile = usiMove.at(0).toLatin1() - '0';  // '7' -> 7
    int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;  // 'g' -> 7
    int toFile = usiMove.at(2).toLatin1() - '0';
    int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
    bool promote = (usiMove.length() >= 5 && usiMove.at(4) == '+');

    // 移動元の駒を取得
    QChar piece = board->getPieceCharacter(fromFile, fromRank);

    // 移動先に駒があれば取る
    QChar captured = board->getPieceCharacter(toFile, toRank);
    if (captured != ' ') {
        // 駒を駒台に追加
        QChar standPiece = captured;
        // 成駒を元に戻す
        static const QMap<QChar, QChar> demoteMap = {
            {'Q','P'},{'M','L'},{'O','N'},{'T','S'},{'C','B'},{'U','R'},
            {'q','p'},{'m','l'},{'o','n'},{'t','s'},{'c','b'},{'u','r'}
        };
        if (demoteMap.contains(standPiece)) {
            standPiece = demoteMap[standPiece];
        }
        // 相手の駒を自分の駒に変換
        standPiece = standPiece.isUpper() ? standPiece.toLower() : standPiece.toUpper();
        board->m_pieceStand[standPiece]++;
    }

    // 駒を移動
    board->movePieceToSquare(piece, fromFile, fromRank, toFile, toRank, promote);
}

QString PvBoardDialog::currentTurn() const
{
    if (m_currentPly < m_sfenHistory.size()) {
        // SFENから手番を抽出
        const QString& sfen = m_sfenHistory.at(m_currentPly);
        QStringList parts = sfen.split(' ');
        if (parts.size() >= 2) {
            return parts.at(1);
        }
    }
    return QStringLiteral("b");
}

void PvBoardDialog::applyMove(const QString& usiMove)
{
    applyUsiMoveToBoard(m_board, usiMove);
}
