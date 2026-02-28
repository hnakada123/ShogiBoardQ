#ifndef PVBOARDCONTROLLER_H
#define PVBOARDCONTROLLER_H

/// @file pvboardcontroller.h
/// @brief PV盤面コントローラクラスの定義

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPoint>

class ShogiBoard;

/// PV手のハイライト座標
struct PvHighlightCoords {
    bool valid = false;
    bool hasFrom = false;
    int fromFile = 0;
    int fromRank = 0;
    bool hasTo = false;
    int toFile = 0;
    int toRank = 0;
};

/**
 * @brief PV盤面のナビゲーション・状態管理コントローラ
 *
 * 読み筋（PV）の盤面ナビゲーションに関するロジックを担当:
 * - SFEN履歴の構築・管理
 * - 手数ナビゲーション（前進・後退）
 * - ハイライト座標の計算
 * - 漢字表記の手の分割
 */
class PvBoardController {
public:
    PvBoardController(const QString& baseSfen, const QStringList& pvMoves);

    // Navigation
    void goFirst();
    bool goBack();
    bool goForward();
    void goLast();

    // State queries
    int currentPly() const { return m_currentPly; }
    int totalMoves() const { return static_cast<int>(m_pvMoves.size()); }
    bool canGoBack() const { return m_currentPly > 0; }
    bool canGoForward() const { return m_currentPly < totalMoves(); }

    // Display data
    QString currentSfen() const;
    bool isBlackTurn() const;
    QString currentMoveText() const;

    // Property setters
    void setKanjiPv(const QString& kanjiPv);
    void setLastMove(const QString& lastMove) { m_lastMove = lastMove; }
    void setPrevSfen(const QString& prevSfen) { m_prevSfen = prevSfen; }

    // Highlight computation
    PvHighlightCoords computeHighlightCoords() const;

    // Static: apply USI move to board (for SFEN history construction)
    static void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isBlackToMove);

private:
    void buildSfenHistory();
    void parseKanjiMoves();
    PvHighlightCoords computeHighlightFromUsiMove(const QString& usiMove, bool isBasePosition) const;

    static bool isStartposSfen(QString sfen);
    static bool diffSfenForHighlight(const QString& prevSfen, const QString& currSfen,
                                     int& fromFile, int& fromRank,
                                     int& toFile, int& toRank,
                                     QChar& droppedPiece);
    static QPoint getStandPseudoCoord(QChar pieceChar, bool isBlack);

    QString m_baseSfen;
    QStringList m_pvMoves;
    QString m_kanjiPv;
    QStringList m_kanjiMoves;
    QString m_lastMove;
    QString m_prevSfen;
    QVector<QString> m_sfenHistory;
    int m_currentPly = 0;
};

#endif // PVBOARDCONTROLLER_H
