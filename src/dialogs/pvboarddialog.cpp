#include "pvboarddialog.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QRegularExpression>

// 前方宣言: SFENからUSI形式の手を盤面に適用する静的ヘルパー
static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isBlackToMove);

PvBoardDialog::PvBoardDialog(const QString& baseSfen,
                             const QStringList& pvMoves,
                             QWidget* parent)
    : QDialog(parent)
    , m_baseSfen(baseSfen)
    , m_pvMoves(pvMoves)
    , m_currentPly(0)
{
    setWindowTitle(tr("読み筋表示"));
    // ダイアログのサイズは可変に（将棋盤のサイズ変更に対応）
    setMinimumSize(400, 500);
    resize(620, 720);

    // 初期盤面を履歴に追加
    m_sfenHistory.clear();
    m_sfenHistory.append(m_baseSfen);

    // 開始局面の手番を取得（"b"=先手、"w"=後手）
    bool blackToMove = !m_baseSfen.contains(QStringLiteral(" w "));

    // 各手を適用してSFEN履歴を構築
    ShogiBoard tempBoard;
    tempBoard.setSfen(m_baseSfen);

    for (int i = 0; i < m_pvMoves.size(); ++i) {
        const QString& move = m_pvMoves.at(i);
        // 手を指す前の手番を渡す（blackToMoveは現在の手番）
        applyUsiMoveToBoard(&tempBoard, move, blackToMove);
        
        // 手番を切り替え（指し手を指した後なので手番が変わる）
        blackToMove = !blackToMove;
        QString nextTurn = blackToMove ? QStringLiteral("b") : QStringLiteral("w");
        
        QString nextSfen = tempBoard.convertBoardToSfen() + QStringLiteral(" ") +
                          nextTurn + QStringLiteral(" ") +
                          tempBoard.convertStandToSfen() + QStringLiteral(" 1");
        m_sfenHistory.append(nextSfen);
    }

    // UIを構築（この中でm_boardが作られる）
    buildUi();

    // 初期盤面を設定
    if (!m_sfenHistory.isEmpty()) {
        m_board->setSfen(m_sfenHistory.at(0));
    }
    
    // ShogiViewの初期サイズを強制的に設定してレイアウトを確定
    // （ダイアログ内で新規作成されたShogiViewはサイズが不確定のためエラーになる）
    m_shogiView->updateBoardSize();
    
    // updateBoardSize()が時計ラベルを再表示するため、再度非表示にする
    hideClockLabels();
    
    // 初期表示
    updateBoardDisplay();
    updateButtonStates();
    
    // ダイアログサイズを内容に合わせて自動調整
    adjustSize();
}

PvBoardDialog::~PvBoardDialog()
{
    // ハイライトをクリア
    clearMoveHighlights();
    // m_board と m_shogiView は Qt のオブジェクトツリーで管理
}

void PvBoardDialog::setKanjiPv(const QString& kanjiPv)
{
    m_kanjiPv = kanjiPv;
    if (m_pvLabel) {
        m_pvLabel->setText(m_kanjiPv);
    }
}

void PvBoardDialog::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    m_blackPlayerName = blackName.isEmpty() ? tr("先手") : blackName;
    m_whitePlayerName = whiteName.isEmpty() ? tr("後手") : whiteName;
    
    if (m_shogiView) {
        // ShogiViewのsetBlackPlayerName/setWhitePlayerNameを使用
        // これにより「▲」「▽」などのマークも自動で付与される
        m_shogiView->setBlackPlayerName(m_blackPlayerName);
        m_shogiView->setWhitePlayerName(m_whitePlayerName);
    }
}

void PvBoardDialog::buildUi()
{
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

    // 将棋盤拡大・縮小ボタン（将棋盤の上に配置）
    QHBoxLayout* zoomLayout = new QHBoxLayout();
    zoomLayout->setSpacing(4);
    
    m_btnReduce = new QPushButton(QStringLiteral("将棋盤縮小 ➖"), this);
    m_btnReduce->setToolTip(tr("将棋盤を縮小する"));
    connect(m_btnReduce, &QPushButton::clicked, this, &PvBoardDialog::onReduceBoard);
    zoomLayout->addWidget(m_btnReduce);
    
    m_btnEnlarge = new QPushButton(QStringLiteral("将棋盤拡大 ➕"), this);
    m_btnEnlarge->setToolTip(tr("将棋盤を拡大する"));
    connect(m_btnEnlarge, &QPushButton::clicked, this, &PvBoardDialog::onEnlargeBoard);
    zoomLayout->addWidget(m_btnEnlarge);
    
    zoomLayout->addStretch();
    mainLayout->addLayout(zoomLayout);

    // 将棋盤
    m_board = new ShogiBoard(9, 9, this);
    m_shogiView = new ShogiView(this);
    m_shogiView->setMouseClickMode(false);  // クリック操作は無効
    
    // 駒画像を読み込んでから盤を設定
    m_shogiView->setPieces();
    m_shogiView->setBoard(m_board);
    
    // 時計ラベルは後でhideClockLabels()で非表示にする
    // 対局者名はShogiViewのメソッドを使用（マーク付き表示）
    m_shogiView->setBlackPlayerName(m_blackPlayerName.isEmpty() ? tr("先手") : m_blackPlayerName);
    m_shogiView->setWhitePlayerName(m_whitePlayerName.isEmpty() ? tr("後手") : m_whitePlayerName);
    
    // ShogiViewをレイアウトに追加（サイズはShogiView自身のsizeHintに任せる）
    mainLayout->addWidget(m_shogiView, 1);

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
    if (m_currentPly < 0 || m_currentPly >= m_sfenHistory.size()) {
        return;
    }

    // 現在の局面SFENを取得して盤面を更新
    const QString& sfen = m_sfenHistory.at(m_currentPly);
    m_board->setSfen(sfen);
    m_shogiView->update();

    // ハイライトを更新
    updateMoveHighlights();

    // 手番表示を更新（SFENから手番を取得）
    bool isBlackTurn = !sfen.contains(QStringLiteral(" w "));
    m_shogiView->setActiveSide(isBlackTurn);
    m_shogiView->updateTurnIndicator(isBlackTurn ? ShogiGameController::Player1 
                                                  : ShogiGameController::Player2);

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

void PvBoardDialog::onEnlargeBoard()
{
    if (m_shogiView) {
        // ShogiView::enlargeBoard()を使用（m_squareSizeを変更してレイアウト再計算）
        m_shogiView->enlargeBoard();
        // 時計ラベルが再表示されるため、再度非表示にする
        hideClockLabels();
        // ダイアログのサイズも調整（sizeHintを参考に）
        adjustSize();
    }
}

void PvBoardDialog::onReduceBoard()
{
    if (m_shogiView) {
        // ShogiView::reduceBoard()を使用（m_squareSizeを変更してレイアウト再計算）
        m_shogiView->reduceBoard();
        // 時計ラベルが再表示されるため、再度非表示にする
        hideClockLabels();
        // ダイアログのサイズも調整
        adjustSize();
    }
}

void PvBoardDialog::hideClockLabels()
{
    if (m_shogiView) {
        // 時計表示を無効にする（ShogiViewのフラグを使用）
        m_shogiView->setClockEnabled(false);
    }
}

// SFENからUSI形式の手を盤面に適用する静的ヘルパー
// isBlackToMove: trueなら先手の手、falseなら後手の手
static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isBlackToMove)
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

        // 手番に応じて駒文字を調整（後手なら小文字に）
        if (!isBlackToMove) {
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
    bool isBlackToMove = (currentTurn() == QStringLiteral("b"));
    applyUsiMoveToBoard(m_board, usiMove, isBlackToMove);
}

void PvBoardDialog::clearMoveHighlights()
{
    if (!m_shogiView) return;

    if (m_fromHighlight) {
        m_shogiView->removeHighlight(m_fromHighlight);
        delete m_fromHighlight;
        m_fromHighlight = nullptr;
    }
    if (m_toHighlight) {
        m_shogiView->removeHighlight(m_toHighlight);
        delete m_toHighlight;
        m_toHighlight = nullptr;
    }
}

// 駒種（USI形式）から駒台の疑似座標を取得
// 戻り値: (file, rank) の QPoint。file=10は先手駒台、file=11は後手駒台
static QPoint getStandPseudoCoord(QChar pieceChar, bool isBlack)
{
    // USI形式の駒種は大文字（P, L, N, S, G, B, R, K）
    QChar upperPiece = pieceChar.toUpper();
    
    if (isBlack) {
        // 先手駒台: file=10
        // 左列(rank=7,5,3,1): R, G, N, P
        // 右列(rank=8,6,4,2): K, B, S, L
        switch (upperPiece.toLatin1()) {
            case 'P': return QPoint(10, 1);
            case 'L': return QPoint(10, 2);
            case 'N': return QPoint(10, 3);
            case 'S': return QPoint(10, 4);
            case 'G': return QPoint(10, 5);
            case 'B': return QPoint(10, 6);
            case 'R': return QPoint(10, 7);
            case 'K': return QPoint(10, 8);
            default: return QPoint();
        }
    } else {
        // 後手駒台: file=11
        // 左列(rank=3,5,7,9): r, g, n, p
        // 右列(rank=2,4,6,8): k, b, s, l
        switch (upperPiece.toLatin1()) {
            case 'P': return QPoint(11, 9);
            case 'L': return QPoint(11, 8);
            case 'N': return QPoint(11, 7);
            case 'S': return QPoint(11, 6);
            case 'G': return QPoint(11, 5);
            case 'B': return QPoint(11, 4);
            case 'R': return QPoint(11, 3);
            case 'K': return QPoint(11, 2);
            default: return QPoint();
        }
    }
}

void PvBoardDialog::updateMoveHighlights()
{
    // 既存のハイライトをクリア
    clearMoveHighlights();

    if (!m_shogiView) return;

    // 開始局面（手数0）の場合はハイライトなし
    if (m_currentPly == 0 || m_currentPly > m_pvMoves.size()) {
        return;
    }

    // 現在表示している局面に至った手を取得
    const QString& usiMove = m_pvMoves.at(m_currentPly - 1);
    if (usiMove.length() < 4) return;

    int toFile = 0, toRank = 0;

    // 駒打ちの場合（例: "P*5e"）
    if (usiMove.at(1) == '*') {
        QChar pieceChar = usiMove.at(0);
        toFile = usiMove.at(2).toLatin1() - '0';
        toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        // 手番を取得（1つ前の局面の手番 = この手を指したプレイヤー）
        // m_currentPly - 1 の局面の手番を見る
        bool isBlackMove = true;
        if (m_currentPly >= 1 && m_currentPly <= m_sfenHistory.size()) {
            // m_currentPly - 1 番目の局面（指す前）の手番を確認
            const QString& prevSfen = m_sfenHistory.at(m_currentPly - 1);
            isBlackMove = !prevSfen.contains(QStringLiteral(" w "));
        }

        // 駒台の疑似座標を取得
        QPoint standCoord = getStandPseudoCoord(pieceChar, isBlackMove);
        if (!standCoord.isNull()) {
            // 移動元（駒台）を薄い赤でハイライト
            m_fromHighlight = new ShogiView::FieldHighlight(
                standCoord.x(), standCoord.y(), QColor(255, 0, 0, 50));
            m_shogiView->addHighlight(m_fromHighlight);
        }

        // 移動先（盤上）を黄色でハイライト
        m_toHighlight = new ShogiView::FieldHighlight(toFile, toRank, QColor(255, 255, 0));
        m_shogiView->addHighlight(m_toHighlight);
    } else {
        // 通常の移動（例: "7g7f" または "7g7f+"）
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        toFile = usiMove.at(2).toLatin1() - '0';
        toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        // 移動元（薄いピンク/赤）
        m_fromHighlight = new ShogiView::FieldHighlight(fromFile, fromRank, QColor(255, 0, 0, 50));
        m_shogiView->addHighlight(m_fromHighlight);

        // 移動先（黄色）
        m_toHighlight = new ShogiView::FieldHighlight(toFile, toRank, QColor(255, 255, 0));
        m_shogiView->addHighlight(m_toHighlight);
    }

    m_shogiView->update();
}
