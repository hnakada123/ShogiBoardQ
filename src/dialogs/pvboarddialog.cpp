/// @file pvboarddialog.cpp
/// @brief 読み筋盤面ダイアログクラスの実装

#include "pvboarddialog.h"
#include "buttonstyles.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "settingsservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "loggingcategory.h"
#include <QRegularExpression>
#include <QCloseEvent>
#include <QWheelEvent>
#include <QTimer>

// 前方宣言: SFENからUSI形式の手を盤面に適用する静的ヘルパー
static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isBlackToMove);
static bool isStartposSfen(QString sfen);
static bool diffSfenForHighlight(const QString& prevSfen, const QString& currSfen,
                                 int& fromFile, int& fromRank,
                                 int& toFile, int& toRank,
                                 QChar& droppedPiece);

PvBoardDialog::PvBoardDialog(const QString& baseSfen,
                             const QStringList& pvMoves,
                             QWidget* parent)
    : QDialog(parent)
    , m_baseSfen(baseSfen)
    , m_pvMoves(pvMoves)
{
    setWindowTitle(tr("読み筋表示"));
    // ダイアログのサイズは可変に（将棋盤のサイズ変更に対応）
    setMinimumSize(400, 500);
    
    // 前回保存されたウィンドウサイズを読み込む
    QSize savedSize = SettingsService::pvBoardDialogSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    } else {
        resize(620, 720);
    }

    // 初期盤面を履歴に追加
    m_sfenHistory.clear();
    m_sfenHistory.append(m_baseSfen);

    // 開始局面の手番を取得（"b"=先手、"w"=後手）
    bool blackToMove = !m_baseSfen.contains(QStringLiteral(" w "));

    // 各手を適用してSFEN履歴を構築
    ShogiBoard tempBoard;
    tempBoard.setSfen(m_baseSfen);

    for (qsizetype i = 0; i < m_pvMoves.size(); ++i) {
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

    // レイアウト確定後に自動調整（起動直後のサイズ不足を防ぐ）
    QTimer::singleShot(0, this, &PvBoardDialog::adjustWindowToContents);
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
    // 漢字表記を個々の手に分割
    parseKanjiMoves();
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

void PvBoardDialog::setLastMove(const QString& lastMove)
{
    m_lastMove = lastMove;
    qCDebug(lcUi) << "setLastMove: lastMove=" << lastMove
             << " m_currentPly=" << m_currentPly;
    // 現在が開始局面（手数0）であれば、ハイライトを更新
    if (m_currentPly == 0) {
        qCDebug(lcUi) << "setLastMove: calling updateMoveHighlights()";
        updateMoveHighlights();
    }
}

void PvBoardDialog::setPrevSfenForHighlight(const QString& prevSfen)
{
    m_prevSfen = prevSfen;
    qCDebug(lcUi) << "setPrevSfenForHighlight: prevSfen=" << m_prevSfen.left(60);
    if (m_currentPly == 0) {
        qCDebug(lcUi) << "setPrevSfenForHighlight: calling updateMoveHighlights()";
        updateMoveHighlights();
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
    m_btnReduce->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnReduce, &QPushButton::clicked, this, &PvBoardDialog::onReduceBoard);
    zoomLayout->addWidget(m_btnReduce);

    m_btnEnlarge = new QPushButton(QStringLiteral("将棋盤拡大 ➕"), this);
    m_btnEnlarge->setToolTip(tr("将棋盤を拡大する"));
    m_btnEnlarge->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnEnlarge, &QPushButton::clicked, this, &PvBoardDialog::onEnlargeBoard);
    zoomLayout->addWidget(m_btnEnlarge);

    m_btnFlip = new QPushButton(tr("盤面の回転"), this);
    m_btnFlip->setToolTip(tr("盤面を回転する"));
    m_btnFlip->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnFlip, &QPushButton::clicked, this, &PvBoardDialog::onFlipBoard);
    zoomLayout->addWidget(m_btnFlip);

    zoomLayout->addStretch();
    mainLayout->addLayout(zoomLayout);

    // 将棋盤
    m_board = new ShogiBoard(9, 9, this);
    m_shogiView = new ShogiView(this);
    m_shogiView->setMouseClickMode(false);  // クリック操作は無効
    m_shogiView->setNameFontScale(0.30);    // GUI本体と同じフォントスケールを設定
    
    // 駒画像を読み込んでから盤を設定
    m_shogiView->setPieces();
    m_shogiView->setBoard(m_board);
    
    // ShogiViewのCtrl+ホイールイベントを横取りして、ダイアログ側で処理する
    // これにより、盤サイズ変更とダイアログサイズ調整を同期的に行える
    m_shogiView->installEventFilter(this);
    
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
    m_btnFirst->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnFirst, &QPushButton::clicked, this, &PvBoardDialog::onGoFirst);
    btnLayout->addWidget(m_btnFirst);

    m_btnBack = new QPushButton(QStringLiteral("◀ 1手戻る"), this);
    m_btnBack->setMinimumWidth(100);
    m_btnBack->setToolTip(tr("1手戻る"));
    m_btnBack->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnBack, &QPushButton::clicked, this, &PvBoardDialog::onGoBack);
    btnLayout->addWidget(m_btnBack);

    m_btnForward = new QPushButton(QStringLiteral("1手進む ▶"), this);
    m_btnForward->setMinimumWidth(100);
    m_btnForward->setToolTip(tr("1手進む"));
    m_btnForward->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnForward, &QPushButton::clicked, this, &PvBoardDialog::onGoForward);
    btnLayout->addWidget(m_btnForward);

    m_btnLast = new QPushButton(QStringLiteral("最後 ⏭"), this);
    m_btnLast->setMinimumWidth(80);
    m_btnLast->setToolTip(tr("最後の局面まで進む"));
    m_btnLast->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnLast, &QPushButton::clicked, this, &PvBoardDialog::onGoLast);
    btnLayout->addWidget(m_btnLast);

    mainLayout->addLayout(btnLayout);

    // 閉じるボタン
    QPushButton* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    closeBtn->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout* closeBtnLayout = new QHBoxLayout();
    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);
    closeBtnLayout->addStretch();
    mainLayout->addLayout(closeBtnLayout);
}

void PvBoardDialog::updateButtonStates()
{
    const qsizetype maxPly = m_pvMoves.size();

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
    const qsizetype totalMoves = m_pvMoves.size();
    QString plyText = tr("手数: %1 / %2").arg(m_currentPly).arg(totalMoves);
    if (m_currentPly == 0) {
        plyText += tr(" (開始局面)");
    } else if (m_currentPly <= m_pvMoves.size()) {
        // 現在の手を表示（漢字表記があればそれを使用、なければUSI形式）
        QString moveText;
        if (m_currentPly - 1 < m_kanjiMoves.size() && !m_kanjiMoves.at(m_currentPly - 1).isEmpty()) {
            moveText = m_kanjiMoves.at(m_currentPly - 1);
        } else {
            moveText = m_pvMoves.at(m_currentPly - 1);
        }
        plyText += QStringLiteral(" [%1]").arg(moveText);
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
    m_currentPly = static_cast<int>(m_pvMoves.size());
    updateBoardDisplay();
    updateButtonStates();
}

void PvBoardDialog::onEnlargeBoard()
{
    if (m_shogiView) {
        // ShogiView::enlargeBoard()を使用（m_squareSizeを変更してレイアウト再計算）
        // シグナル発火を抑制して同期的に処理
        m_shogiView->enlargeBoard(false);
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
        // シグナル発火を抑制して同期的に処理
        m_shogiView->reduceBoard(false);
        // 時計ラベルが再表示されるため、再度非表示にする
        hideClockLabels();
        // ダイアログのサイズも調整
        adjustSize();
    }
}

void PvBoardDialog::setFlipMode(bool flipped)
{
    if (!m_shogiView) return;

    m_shogiView->setFlipMode(flipped);
    if (flipped) {
        m_shogiView->setPiecesFlip();
    } else {
        m_shogiView->setPieces();
    }
    m_shogiView->update();
}

void PvBoardDialog::onFlipBoard()
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

bool PvBoardDialog::eventFilter(QObject* watched, QEvent* event)
{
    // ShogiViewのCtrl+ホイールイベントを横取りして処理
    if (watched == m_shogiView && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            const int delta = wheelEvent->angleDelta().y();
            if (delta > 0) {
                // 上方向スクロール → 拡大
                onEnlargeBoard();
            } else if (delta < 0) {
                // 下方向スクロール → 縮小
                onReduceBoard();
            }
            return true;  // イベントを消費（ShogiViewには渡さない）
        }
    }
    return QDialog::eventFilter(watched, event);
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

    // USI形式の座標検証ヘルパ（筋: 1-9, 段: a-i → 1-9）
    auto isValidFile = [](QChar ch) { return ch >= '1' && ch <= '9'; };
    auto isValidRank = [](QChar ch) { return ch >= 'a' && ch <= 'i'; };

    // 打つ手（駒打ち）の場合
    if (usiMove.at(1) == '*') {
        // 例: "P*5e"
        if (!isValidFile(usiMove.at(2)) || !isValidRank(usiMove.at(3))) return;

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
    if (!isValidFile(usiMove.at(0)) || !isValidRank(usiMove.at(1)) ||
        !isValidFile(usiMove.at(2)) || !isValidRank(usiMove.at(3))) {
        return;
    }

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

static bool isStartposSfen(QString sfen)
{
    sfen = sfen.trimmed();
    if (sfen.startsWith(QStringLiteral("position sfen "))) {
        sfen = sfen.mid(14).trimmed();
    } else if (sfen.startsWith(QStringLiteral("position "))) {
        sfen = sfen.mid(9).trimmed();
    }

    if (sfen == QLatin1String("startpos")) {
        return true;
    }

    static const QString startposSfen =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return sfen.startsWith(startposSfen.left(60));
}

static bool diffSfenForHighlight(const QString& prevSfen, const QString& currSfen,
                                 int& fromFile, int& fromRank,
                                 int& toFile, int& toRank,
                                 QChar& droppedPiece)
{
    fromFile = 0;
    fromRank = 0;
    toFile = 0;
    toRank = 0;
    droppedPiece = QChar();

    auto parseOneBoard = [](const QString& sfen, QString grid[9][9])->bool {
        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 9; ++x) grid[y][x].clear();
        }

        if (sfen.isEmpty()) return false;
        const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
        const QStringList rows = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (rows.size() != 9) return false;

        for (int r = 0; r < 9; ++r) {
            const QString& row = rows.at(r);
            const int y = r;
            int x = 8;
            for (qsizetype i = 0; i < row.size(); ++i) {
                const QChar ch = row.at(i);
                if (ch.isDigit()) {
                    x -= (ch.toLatin1() - '0');
                } else if (ch == QLatin1Char('+')) {
                    if (i + 1 >= row.size() || x < 0) return false;
                    grid[y][x] = QStringLiteral("+") + row.at(++i);
                    --x;
                } else {
                    if (x < 0) return false;
                    grid[y][x] = QString(ch);
                    --x;
                }
            }
            if (x != -1) return false;
        }
        return true;
    };

    QString ga[9][9], gb[9][9];
    if (!parseOneBoard(prevSfen, ga) || !parseOneBoard(currSfen, gb)) return false;

    int fromX = -1;
    int fromY = -1;
    int toX = -1;
    int toY = -1;
    int emptyCount = 0;    // 駒がなくなったマスの数（移動元候補）
    int filledCount = 0;   // 駒が現れたマスの数（駒打ちの移動先候補）
    int changedCount = 0;  // 駒が置き換わったマスの数（駒取りの移動先候補）

    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            if (ga[y][x] == gb[y][x]) continue;
            if (!ga[y][x].isEmpty() && gb[y][x].isEmpty()) {
                // 駒がなくなった（移動元）
                ++emptyCount;
                fromX = x;
                fromY = y;
            } else if (ga[y][x].isEmpty() && !gb[y][x].isEmpty()) {
                // 駒が現れた（駒打ちの移動先）
                ++filledCount;
                toX = x;
                toY = y;
                droppedPiece = gb[y][x].at(0);
                if (droppedPiece == QLatin1Char('+') && gb[y][x].size() >= 2) {
                    droppedPiece = gb[y][x].at(1);
                }
            } else {
                // 駒が置き換わった（駒取りの移動先）
                ++changedCount;
                toX = x;
                toY = y;
            }
        }
    }

    // 1手の移動としては以下のパターンのみ有効:
    // - 通常移動: emptyCount=1, changedCount=0, filledCount=1 (移動元が空になり、移動先に駒が現れる)
    // - 駒取り移動: emptyCount=1, changedCount=1, filledCount=0 (移動元が空になり、移動先の駒が置き換わる)
    // - 駒打ち: emptyCount=0, changedCount=0, filledCount=1 (移動元なし、移動先に駒が現れる)
    // それ以外（複数手分の差分など）は失敗を返す
    bool validMove = (emptyCount == 1 && changedCount == 0 && filledCount == 1) ||
                     (emptyCount == 1 && changedCount == 1 && filledCount == 0) ||
                     (emptyCount == 0 && changedCount == 0 && filledCount == 1);

    if (!validMove) {
        qCDebug(lcUi) << "Invalid diff pattern:"
                 << " emptyCount=" << emptyCount
                 << " filledCount=" << filledCount
                 << " changedCount=" << changedCount;
        return false;
    }

    // 移動先が見つからない場合は失敗
    if (toX < 0 || toY < 0) return false;

    auto toFileRank = [](int x, int y, int& file, int& rank) {
        // x=8は9筋、x=0は1筋なので、file = x + 1
        file = x + 1;
        rank = y + 1;
    };

    if (fromX >= 0 && fromY >= 0) {
        toFileRank(fromX, fromY, fromFile, fromRank);
    }
    toFileRank(toX, toY, toFile, toRank);
    return true;
}

void PvBoardDialog::updateMoveHighlights()
{
    qCDebug(lcUi) << "updateMoveHighlights: m_currentPly=" << m_currentPly
             << " m_lastMove=" << m_lastMove
             << " m_pvMoves.size()=" << m_pvMoves.size();
    qCDebug(lcUi) << "updateMoveHighlights: baseSfen=" << m_baseSfen.left(60)
             << " prevSfen=" << m_prevSfen.left(60);
    
    // 既存のハイライトをクリア
    clearMoveHighlights();

    if (!m_shogiView) {
        qCDebug(lcUi) << "updateMoveHighlights: m_shogiView is null!";
        return;
    }

    QString usiMove;
    bool isBasePosition = false;  // 開始局面（手数0）かどうか

    // 開始局面（手数0）の場合
    if (m_currentPly == 0) {
        if (!m_lastMove.isEmpty() && m_lastMove.length() >= 4) {
            usiMove = m_lastMove;
            isBasePosition = true;
            qCDebug(lcUi) << "updateMoveHighlights: using m_lastMove=" << usiMove;
        } else if (!m_prevSfen.isEmpty()) {
            int fromFile = 0;
            int fromRank = 0;
            int toFile = 0;
            int toRank = 0;
            QChar droppedPiece;
            if (diffSfenForHighlight(m_prevSfen, m_baseSfen, fromFile, fromRank, toFile, toRank, droppedPiece)) {
                qCDebug(lcUi) << "updateMoveHighlights: diff ok"
                         << " from=" << fromFile << fromRank
                         << " to=" << toFile << toRank
                         << " drop=" << droppedPiece;
                if (fromFile > 0 && fromRank > 0) {
                    m_fromHighlight = new ShogiView::FieldHighlight(fromFile, fromRank, QColor(255, 0, 0, 50));
                    m_shogiView->addHighlight(m_fromHighlight);
                } else if (!droppedPiece.isNull()) {
                    const bool isBlack = droppedPiece.isUpper();
                    QPoint standCoord = getStandPseudoCoord(droppedPiece, isBlack);
                    if (!standCoord.isNull()) {
                        m_fromHighlight = new ShogiView::FieldHighlight(
                            standCoord.x(), standCoord.y(), QColor(255, 0, 0, 50));
                        m_shogiView->addHighlight(m_fromHighlight);
                    }
                }
                if (toFile > 0 && toRank > 0) {
                    m_toHighlight = new ShogiView::FieldHighlight(toFile, toRank, QColor(255, 255, 0));
                    m_shogiView->addHighlight(m_toHighlight);
                }
                m_shogiView->update();
                qCDebug(lcUi) << "updateMoveHighlights: diff highlight applied";
                return;
            } else {
                qCDebug(lcUi) << "updateMoveHighlights: diff failed";
            }
        } else if (!m_pvMoves.isEmpty() && m_pvMoves.first().length() >= 4 && isStartposSfen(m_baseSfen)) {
            usiMove = m_pvMoves.first();
            isBasePosition = false;
            qCDebug(lcUi) << "updateMoveHighlights: using first pvMove(startpos)=" << usiMove;
        } else {
            qCDebug(lcUi) << "updateMoveHighlights: no usable move for base position";
            return;  // ハイライトなし
        }
    } else if (m_currentPly > m_pvMoves.size()) {
        qCDebug(lcUi) << "updateMoveHighlights: m_currentPly > m_pvMoves.size(), no highlight";
        return;
    } else {
        // 現在表示している局面に至った手を取得
        usiMove = m_pvMoves.at(m_currentPly - 1);
        qCDebug(lcUi) << "updateMoveHighlights: using pvMove=" << usiMove;
    }

    if (usiMove.length() < 4) {
        qCDebug(lcUi) << "updateMoveHighlights: usiMove too short:" << usiMove;
        return;
    }

    int toFile = 0, toRank = 0;

    // 駒打ちの場合（例: "P*5e"）
    if (usiMove.at(1) == '*') {
        QChar pieceChar = usiMove.at(0);
        toFile = usiMove.at(2).toLatin1() - '0';
        toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        qCDebug(lcUi) << "updateMoveHighlights: drop move, pieceChar=" << pieceChar
                 << " toFile=" << toFile << " toRank=" << toRank;

        // 手番を取得
        bool isBlackMove = true;
        if (isBasePosition) {
            // 開始局面の場合、baseSfenの手番の「逆」がm_lastMoveを指したプレイヤー
            // （baseSfenは指した後の局面なので、手番は相手に移っている）
            isBlackMove = m_baseSfen.contains(QStringLiteral(" w "));  // 後手番なら先手が指した
        } else if (m_currentPly >= 1 && m_currentPly <= m_sfenHistory.size()) {
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
        qCDebug(lcUi) << "updateMoveHighlights: added drop highlights";
    } else {
        // 通常の移動（例: "7g7f" または "7g7f+"）
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        toFile = usiMove.at(2).toLatin1() - '0';
        toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        qCDebug(lcUi) << "updateMoveHighlights: normal move, fromFile=" << fromFile
                 << " fromRank=" << fromRank << " toFile=" << toFile << " toRank=" << toRank;

        // 移動元（薄いピンク/赤）
        m_fromHighlight = new ShogiView::FieldHighlight(fromFile, fromRank, QColor(255, 0, 0, 50));
        m_shogiView->addHighlight(m_fromHighlight);

        // 移動先（黄色）
        m_toHighlight = new ShogiView::FieldHighlight(toFile, toRank, QColor(255, 255, 0));
        m_shogiView->addHighlight(m_toHighlight);
        qCDebug(lcUi) << "updateMoveHighlights: added normal move highlights";
    }

    m_shogiView->update();
    qCDebug(lcUi) << "updateMoveHighlights: done, called m_shogiView->update()";
}

void PvBoardDialog::parseKanjiMoves()
{
    m_kanjiMoves.clear();
    
    if (m_kanjiPv.isEmpty()) {
        return;
    }
    
    // 漢字表記の読み筋を個々の手に分割
    // 例: "△３四歩(33)▲２六歩(27)△８四歩(83)" → ["△３四歩(33)", "▲２六歩(27)", "△８四歩(83)"]
    // ▲ または △ で始まる各手を抽出
    static const QRegularExpression re(QStringLiteral("([▲△][^▲△]+)"));
    QRegularExpressionMatchIterator it = re.globalMatch(m_kanjiPv);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString move = match.captured(1).trimmed();
        if (!move.isEmpty()) {
            m_kanjiMoves.append(move);
        }
    }
    
    qCDebug(lcUi) << "parseKanjiMoves: parsed" << m_kanjiMoves.size() << "moves from kanjiPv";
}

void PvBoardDialog::saveWindowSize()
{
    SettingsService::setPvBoardDialogSize(size());
}

void PvBoardDialog::adjustWindowToContents()
{
    if (m_shogiView) {
        m_shogiView->updateBoardSize();
        hideClockLabels();
    }
    adjustSize();
}

void PvBoardDialog::closeEvent(QCloseEvent* event)
{
    // ウィンドウサイズを保存
    saveWindowSize();
    QDialog::closeEvent(event);
}
