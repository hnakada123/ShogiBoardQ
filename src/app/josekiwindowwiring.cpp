#include "josekiwindowwiring.h"

#include <QDebug>

#include "josekiwindow.h"
#include "shogigamecontroller.h"
#include "kifurecordlistmodel.h"

JosekiWindowWiring::JosekiWindowWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_gameController(deps.gameController)
    , m_kifuRecordModel(deps.kifuRecordModel)
    , m_sfenRecord(deps.sfenRecord)
    , m_usiMoves(deps.usiMoves)
    , m_currentSfenStr(deps.currentSfenStr)
    , m_currentMoveIndex(deps.currentMoveIndex)
    , m_currentSelectedPly(deps.currentSelectedPly)
    , m_playMode(deps.playMode)
{
}

void JosekiWindowWiring::ensureJosekiWindow()
{
    if (m_josekiWindow) return;

    m_josekiWindow = new JosekiWindow(m_parentWidget);

    // 定跡手選択シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::josekiMoveSelected,
            this, &JosekiWindowWiring::onJosekiMoveSelected);

    // 棋譜データ要求シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::requestKifuDataForMerge,
            this, &JosekiWindowWiring::onRequestKifuDataForMerge);

    qDebug().noquote() << "[JosekiWindowWiring] JosekiWindow created and connected";
}

void JosekiWindowWiring::displayJosekiWindow()
{
    ensureJosekiWindow();

    // ウィンドウを表示する（独立ウィンドウとして）
    m_josekiWindow->show();
    m_josekiWindow->raise();
    m_josekiWindow->activateWindow();

    // 現在の局面のSFENを設定（show後に呼ぶ）
    if (m_currentSfenStr) {
        m_josekiWindow->setCurrentSfen(*m_currentSfenStr);
    }
}

void JosekiWindowWiring::updateJosekiWindow()
{
    qDebug() << "[JosekiWindowWiring] updateJosekiWindow() called";

    // 定跡ウィンドウが存在し、表示されている場合のみ更新
    if (!m_josekiWindow || !m_josekiWindow->isVisible()) {
        qDebug() << "[JosekiWindowWiring] updateJosekiWindow: window not visible, skipping";
        return;
    }

    if (!m_currentSfenStr) return;

    qDebug() << "[JosekiWindowWiring] updateJosekiWindow: updating with SFEN=" << *m_currentSfenStr;

    // 人間が着手可能かどうかを判定して設定
    const bool humanCanPlay = determineHumanCanPlay();
    m_josekiWindow->setHumanCanPlay(humanCanPlay);

    // 現在の局面のSFENを定跡ウィンドウに設定
    m_josekiWindow->setCurrentSfen(*m_currentSfenStr);
}

bool JosekiWindowWiring::determineHumanCanPlay() const
{
    if (!m_playMode || !m_currentSfenStr) return true;

    // SFENから手番を取得（b=先手、w=後手）
    bool isBlackTurn = true;  // デフォルト先手
    const QStringList sfenParts = m_currentSfenStr->split(QChar(' '));
    if (sfenParts.size() >= 2) {
        isBlackTurn = (sfenParts.at(1) == QStringLiteral("b"));
    }

    // PlayModeに応じて人間の手番かどうかを判定
    switch (*m_playMode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return isBlackTurn;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return !isBlackTurn;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        return false;
    case PlayMode::NotStarted:
    case PlayMode::AnalysisMode:
    case PlayMode::ConsiderationMode:
    case PlayMode::TsumiSearchMode:
        return true;
    case PlayMode::CsaNetworkMode:
    case PlayMode::PlayModeError:
        return false;
    }
    return true; // 到達しないが、コンパイラ警告を抑制
}

void JosekiWindowWiring::onJosekiMoveSelected(const QString& usiMove)
{
    qDebug() << "[JosekiWindowWiring] onJosekiMoveSelected: usiMove=" << usiMove;

    if (usiMove.isEmpty()) {
        qDebug() << "[JosekiWindowWiring] onJosekiMoveSelected: empty move";
        return;
    }

    QPoint from, to;
    bool promote = false;

    if (!parseUsiMove(usiMove, from, to, promote)) {
        qDebug() << "[JosekiWindowWiring] onJosekiMoveSelected: invalid move format";
        if (m_josekiWindow) {
            m_josekiWindow->onMoveResult(false, usiMove);
        }
        return;
    }

    // 定跡手からの着手の場合、成り/不成が決まっているので強制成りモードを設定
    Q_EMIT forcedPromotionRequested(true, promote);

    // 着手前の棋譜サイズを記録
    const qsizetype sfenSizeBefore = m_sfenRecord ? m_sfenRecord->size() : 0;

    // 指し手実行を要求
    Q_EMIT moveRequested(from, to);

    // 着手後の棋譜サイズを確認して成功/失敗を判定
    const qsizetype sfenSizeAfter = m_sfenRecord ? m_sfenRecord->size() : 0;
    const bool moveSuccess = (sfenSizeAfter > sfenSizeBefore);

    qDebug() << "[JosekiWindowWiring] Move result: sfenSizeBefore=" << sfenSizeBefore
             << "sfenSizeAfter=" << sfenSizeAfter << "success=" << moveSuccess;

    // 定跡ウィンドウに結果を通知（失敗時のみ）
    if (m_josekiWindow && !moveSuccess) {
        m_josekiWindow->onMoveResult(false, usiMove);
    }
}

bool JosekiWindowWiring::parseUsiMove(const QString& usiMove, QPoint& from, QPoint& to, bool& promote) const
{
    promote = false;

    // 駒打ちのパターン: P*5e（駒種*筋段）
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        return parseDropMove(usiMove, from, to);
    }
    // 通常移動のパターン: 7g7f または 7g7f+
    if (usiMove.size() >= 4) {
        return parseNormalMove(usiMove, from, to, promote);
    }

    return false;
}

bool JosekiWindowWiring::parseDropMove(const QString& usiMove, QPoint& from, QPoint& to) const
{
    const QChar pieceChar = usiMove.at(0);
    const int toFile = usiMove.at(2).toLatin1() - '0';
    const int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

    // 現在の手番を取得
    bool isBlackTurn = true;
    if (m_gameController) {
        isBlackTurn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    }

    // 駒台のファイル番号（先手=10, 後手=11）
    const int standFile = isBlackTurn ? 10 : 11;

    // 駒台のランク（駒種番号）
    // 先手: P=1, L=2, N=3, S=4, G=5, B=6, R=7
    // 後手: R=3, B=4, G=5, S=6, N=7, L=8, P=9
    int pieceRank = 0;
    if (isBlackTurn) {
        switch (pieceChar.toUpper().toLatin1()) {
        case 'P': pieceRank = 1; break;
        case 'L': pieceRank = 2; break;
        case 'N': pieceRank = 3; break;
        case 'S': pieceRank = 4; break;
        case 'G': pieceRank = 5; break;
        case 'B': pieceRank = 6; break;
        case 'R': pieceRank = 7; break;
        default: return false;
        }
    } else {
        switch (pieceChar.toUpper().toLatin1()) {
        case 'R': pieceRank = 3; break;
        case 'B': pieceRank = 4; break;
        case 'G': pieceRank = 5; break;
        case 'S': pieceRank = 6; break;
        case 'N': pieceRank = 7; break;
        case 'L': pieceRank = 8; break;
        case 'P': pieceRank = 9; break;
        default: return false;
        }
    }

    from = QPoint(standFile, pieceRank);
    to = QPoint(toFile, toRank);

    qDebug() << "[JosekiWindowWiring] Drop move: piece=" << pieceChar
             << "isBlackTurn=" << isBlackTurn
             << "from=" << from << "to=" << to;

    return true;
}

bool JosekiWindowWiring::parseNormalMove(const QString& usiMove, QPoint& from, QPoint& to, bool& promote) const
{
    const int fromFile = usiMove.at(0).toLatin1() - '0';
    const int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
    const int toFile = usiMove.at(2).toLatin1() - '0';
    const int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
    promote = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));

    from = QPoint(fromFile, fromRank);
    to = QPoint(toFile, toRank);

    qDebug() << "[JosekiWindowWiring] Normal move: from=" << from << "to=" << to << "promote=" << promote;

    return true;
}

void JosekiWindowWiring::onRequestKifuDataForMerge()
{
    qDebug() << "[JosekiWindowWiring] onRequestKifuDataForMerge called";

    if (!m_josekiWindow) {
        qWarning() << "[JosekiWindowWiring] onRequestKifuDataForMerge: m_josekiWindow is null";
        return;
    }

    QStringList sfenList;
    QStringList moveList;
    QStringList japaneseMoveList;
    int currentPly = 0;

    // SFENレコードから局面リストを取得
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        for (int i = 0; i < m_sfenRecord->size(); ++i) {
            sfenList.append(m_sfenRecord->at(i));
        }
        qDebug() << "[JosekiWindowWiring] sfenList from m_sfenRecord, size=" << sfenList.size();
    }

    // 棋譜表示モデルから指し手リストを取得
    if (m_kifuRecordModel && m_kifuRecordModel->rowCount() > 0) {
        for (int i = 0; i < m_kifuRecordModel->rowCount(); ++i) {
            QModelIndex index = m_kifuRecordModel->index(i, 0);
            QString japaneseMove = m_kifuRecordModel->data(index, Qt::DisplayRole).toString();
            japaneseMoveList.append(japaneseMove);
        }
        qDebug() << "[JosekiWindowWiring] japaneseMoveList size=" << japaneseMoveList.size();
    }

    // USI形式の指し手リストを取得
    if (m_usiMoves && !m_usiMoves->isEmpty()) {
        for (const QString& move : std::as_const(*m_usiMoves)) {
            moveList.append(move);
        }
        qDebug() << "[JosekiWindowWiring] moveList from m_usiMoves, size=" << moveList.size();
    } else if (m_sfenRecord && m_sfenRecord->size() > 1) {
        // SFEN差分からUSI形式を生成
        moveList = generateUsiMovesFromSfen();
        qDebug() << "[JosekiWindowWiring] moveList from sfenRecord, size=" << moveList.size();
    }

    // 現在選択中の手数を取得
    if (m_currentMoveIndex && *m_currentMoveIndex > 0) {
        currentPly = *m_currentMoveIndex;
    } else if (m_currentSelectedPly && *m_currentSelectedPly > 0) {
        currentPly = *m_currentSelectedPly;
    }

    qDebug() << "[JosekiWindowWiring] FINAL RESULT"
             << "sfenList.size=" << sfenList.size()
             << "moveList.size=" << moveList.size()
             << "japaneseMoveList.size=" << japaneseMoveList.size()
             << "currentPly=" << currentPly;

    // 定跡ウィンドウにデータを送信
    m_josekiWindow->setKifuDataForMerge(sfenList, moveList, japaneseMoveList, currentPly);
}

QStringList JosekiWindowWiring::generateUsiMovesFromSfen() const
{
    QStringList result;

    if (!m_sfenRecord || m_sfenRecord->size() < 2) {
        return result;
    }

    // 盤面を展開するラムダ
    auto expandBoard = [](const QString& boardStr) -> QVector<QVector<QString>> {
        QVector<QVector<QString>> board(9, QVector<QString>(9));
        const QStringList ranks = boardStr.split(QLatin1Char('/'));
        for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
            const QString& rankStr = ranks[rank];
            int file = 0;
            bool promoted = false;
            for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
                QChar c = rankStr[k];
                if (c == QLatin1Char('+')) {
                    promoted = true;
                } else if (c.isDigit()) {
                    int skip = c.toLatin1() - '0';
                    for (int s = 0; s < skip && file < 9; ++s) {
                        board[rank][file++] = QString();
                    }
                    promoted = false;
                } else {
                    QString piece = promoted ? QStringLiteral("+") + QString(c) : QString(c);
                    board[rank][file++] = piece;
                    promoted = false;
                }
            }
        }
        return board;
    };

    for (int i = 1; i < m_sfenRecord->size(); ++i) {
        const QString& prevSfen = m_sfenRecord->at(i - 1);
        const QString& currSfen = m_sfenRecord->at(i);

        const QStringList prevParts = prevSfen.split(QLatin1Char(' '));
        const QStringList currParts = currSfen.split(QLatin1Char(' '));

        if (prevParts.size() < 3 || currParts.size() < 3) {
            result.append(QString());
            continue;
        }

        QVector<QVector<QString>> prevBoardArr = expandBoard(prevParts[0]);
        QVector<QVector<QString>> currBoardArr = expandBoard(currParts[0]);

        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
        bool isDrop = false;
        bool isPromotion = false;

        QVector<QPoint> emptyPositions;
        QVector<QPoint> filledPositions;

        for (int rank = 0; rank < 9; ++rank) {
            for (int file = 0; file < 9; ++file) {
                const QString& prev = prevBoardArr[rank][file];
                const QString& curr = currBoardArr[rank][file];

                if (prev != curr) {
                    if (!prev.isEmpty() && curr.isEmpty()) {
                        emptyPositions.append(QPoint(file, rank));
                    } else if (!curr.isEmpty()) {
                        filledPositions.append(QPoint(file, rank));
                    }
                }
            }
        }

        if (emptyPositions.size() == 1 && filledPositions.size() == 1) {
            fromPos = emptyPositions[0];
            toPos = filledPositions[0];
            const QString movedPiece = prevBoardArr[fromPos.y()][fromPos.x()];
            const QString& movedPieceFinal = currBoardArr[toPos.y()][toPos.x()];
            QString baseFrom = movedPiece.startsWith(QLatin1Char('+')) ? movedPiece.mid(1) : movedPiece;
            QString baseTo = movedPieceFinal.startsWith(QLatin1Char('+')) ? movedPieceFinal.mid(1) : movedPieceFinal;
            if (baseFrom.toUpper() == baseTo.toUpper() &&
                movedPieceFinal.startsWith(QLatin1Char('+')) && !movedPiece.startsWith(QLatin1Char('+'))) {
                isPromotion = true;
            }
        } else if (emptyPositions.isEmpty() && filledPositions.size() == 1) {
            isDrop = true;
            toPos = filledPositions[0];
        }

        QString usiMove;
        if (isDrop && toPos.x() >= 0) {
            QString droppedPiece = currBoardArr[toPos.y()][toPos.x()];
            QChar pieceChar = droppedPiece.isEmpty() ? QLatin1Char('P') : droppedPiece[0].toUpper();
            int toFileNum = 9 - toPos.x();
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFileNum).arg(toRankChar);
        } else if (fromPos.x() >= 0 && toPos.x() >= 0) {
            int fromFileNum = 9 - fromPos.x();
            int toFileNum = 9 - toPos.x();
            QChar fromRankChar = QChar('a' + fromPos.y());
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1%2%3%4").arg(fromFileNum).arg(fromRankChar).arg(toFileNum).arg(toRankChar);
            if (isPromotion) {
                usiMove += QLatin1Char('+');
            }
        }

        result.append(usiMove);
    }

    return result;
}
