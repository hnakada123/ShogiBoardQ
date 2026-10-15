/// @file tsumeshogipositiongenerator.cpp
/// @brief ランダム詰将棋候補局面生成クラスの実装

#include "tsumeshogipositiongenerator.h"
#include "enginemovevalidator.h"

void TsumeshogiPositionGenerator::setSettings(const Settings& s)
{
    m_settings = s;
}

QString TsumeshogiPositionGenerator::generate()
{
    // 王手がかかっていない局面が生成されるまでリトライ
    constexpr int kMaxRetries = 100;
    for (int retry = 0; retry < kMaxRetries; ++retry) {
        const QString sfen = generateOnce();
        if (!sfen.isEmpty()) {
            return sfen;
        }
    }
    // リトライ上限到達 → 最後の生成結果を無条件で返す
    return generateOnce();
}

QString TsumeshogiPositionGenerator::generateOnce()
{
    clearState();

    auto* rng = QRandomGenerator::global();

    // 1. 後手の玉をランダム配置
    const int kingFile = rng->bounded(9);  // 0-8
    const int kingRank = rng->bounded(9);  // 0-8
    placeOnBoard(kingFile, kingRank, Piece::WhiteKing);

    // 2. 攻め駒をランダム配置
    const int numAttack = rng->bounded(1, m_settings.maxAttackPieces + 1);
    for (int i = 0; i < numAttack; ++i) {
        // 使用可能な駒種を選択
        PieceType pt = randomPieceType();
        if (!hasRemainingPiece(pt)) {
            // 使用上限に達した場合、別の駒種を探す
            bool found = false;
            for (int j = 0; j < PieceTypeCount; ++j) {
                pt = static_cast<PieceType>(j);
                if (hasRemainingPiece(pt)) {
                    found = true;
                    break;
                }
            }
            if (!found) break;  // 全駒種使い切り
        }

        // 盤上(70%) vs 持駒(30%)
        const bool onBoard = rng->bounded(100) < 70;

        if (onBoard) {
            // 玉周辺 attackRange マス以内にランダム配置
            bool placed = false;
            for (int attempt = 0; attempt < 50; ++attempt) {
                const int df = rng->bounded(2 * m_settings.attackRange + 1) - m_settings.attackRange;
                const int dr = rng->bounded(2 * m_settings.attackRange + 1) - m_settings.attackRange;
                const int f = kingFile + df;
                const int r = kingRank + dr;
                if (f < 0 || f > 8 || r < 0 || r > 8) continue;
                if (isOccupied(f, r)) continue;

                // 段制約チェック → 成駒に変換 or 再配置
                bool promote = false;
                if (needsPromotion(pt, r, true)) {
                    if (kCanPromote[pt]) {
                        promote = true;
                    } else {
                        continue;  // 金は成れないので再試行
                    }
                } else {
                    // ランダムに成る(盤上駒の30%)
                    if (kCanPromote[pt] && rng->bounded(100) < 30) {
                        promote = true;
                    }
                }

                // 二歩回避: 同じ筋に不成の歩があれば別のマスを試す
                if (pt == Pawn && !promote && hasUnpromotedPawnInFile(f, true)) {
                    continue;
                }

                placeOnBoard(f, r, pieceOf(pt, true, promote));
                usePiece(pt);
                placed = true;
                break;
            }
            if (!placed) {
                // 盤上配置失敗 → 持駒に入れる
                addToAttackerHand(pt);
            }
        } else {
            // 持駒に追加（成駒は不可なので生駒として）
            addToAttackerHand(pt);
        }
    }

    // 3. 守り駒をランダム配置（玉以外の後手駒）
    const int numDefend = rng->bounded(m_settings.maxDefendPieces + 1);
    for (int i = 0; i < numDefend; ++i) {
        PieceType pt = randomPieceType();
        if (!hasRemainingPiece(pt)) {
            bool found = false;
            for (int j = 0; j < PieceTypeCount; ++j) {
                pt = static_cast<PieceType>(j);
                if (hasRemainingPiece(pt)) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }

        // 玉周辺に後手駒として配置
        for (int attempt = 0; attempt < 50; ++attempt) {
            const int df = rng->bounded(5) - 2;
            const int dr = rng->bounded(5) - 2;
            const int f = kingFile + df;
            const int r = kingRank + dr;
            if (f < 0 || f > 8 || r < 0 || r > 8) continue;
            if (isOccupied(f, r)) continue;

            bool promote = false;
            if (needsPromotion(pt, r, false)) {
                if (kCanPromote[pt]) {
                    promote = true;
                } else {
                    continue;
                }
            }

            // 二歩回避: 同じ筋に不成の歩があれば別のマスを試す
            if (pt == Pawn && !promote && hasUnpromotedPawnInFile(f, false)) {
                continue;
            }

            placeOnBoard(f, r, pieceOf(pt, false, promote));
            usePiece(pt);
            break;
        }
    }

    // 4. 後手玉に王手がかかっていないか検証
    if (isKingInCheck()) {
        return QString();  // 空文字列 → リトライ
    }

    return buildSfen();
}

void TsumeshogiPositionGenerator::clearState()
{
    m_board.resetGameBoard();
    for (int i = 0; i < PieceTypeCount; ++i) {
        m_usedCounts[i] = 0;
    }
}

bool TsumeshogiPositionGenerator::isOccupied(int file, int rank) const
{
    return m_board.pieceAt(BoardSquare{file + 1, rank + 1}) != Piece::None;
}

bool TsumeshogiPositionGenerator::hasUnpromotedPawnInFile(int file, bool isAttacker) const
{
    const Piece pawn = isAttacker ? Piece::BlackPawn : Piece::WhitePawn;
    for (int rank = 0; rank < 9; ++rank) {
        if (m_board.pieceAt(BoardSquare{file + 1, rank + 1}) == pawn) return true;
    }
    return false;
}

void TsumeshogiPositionGenerator::placeOnBoard(int file, int rank, Piece piece)
{
    // 呼び出し側で盤内かつ空きマスであることを確認済み
    Q_ASSERT(file >= 0 && file <= 8 && rank >= 0 && rank <= 8);
    Q_ASSERT(!isOccupied(file, rank));
    m_board.setPieceAt(BoardSquare{file + 1, rank + 1}, piece);
}

TsumeshogiPositionGenerator::PieceType TsumeshogiPositionGenerator::randomPieceType()
{
    return static_cast<PieceType>(QRandomGenerator::global()->bounded(PieceTypeCount));
}

bool TsumeshogiPositionGenerator::hasRemainingPiece(PieceType pt) const
{
    return m_usedCounts[pt] < kMaxCounts[pt];
}

void TsumeshogiPositionGenerator::usePiece(PieceType pt)
{
    m_usedCounts[pt]++;
}

void TsumeshogiPositionGenerator::addToAttackerHand(PieceType pt)
{
    m_board.addStandPiece(kBlackPieces[pt]);
    usePiece(pt);
}

Piece TsumeshogiPositionGenerator::pieceOf(PieceType pt, bool isAttacker, bool promote)
{
    const Piece piece = isAttacker ? kBlackPieces[pt] : toWhite(kBlackPieces[pt]);
    return promote ? ::promote(piece) : piece;
}

bool TsumeshogiPositionGenerator::needsPromotion(PieceType pt, int rank, bool isAttacker) const
{
    // 先手(攻方)視点: rank 0 = 1段目(相手陣最奥)
    // 後手(守方)視点: rank 8 = 9段目(自陣最奥) → 反転して rank 0 が9段目
    // SFEN的には rank 0 = 1段目（上側）
    // 先手の歩・香: 1段目(rank=0)は不可 → 成る必要あり
    // 先手の桂: 1-2段目(rank=0,1)は不可 → 成る必要あり
    // 後手の歩・香: 9段目(rank=8)は不可
    // 後手の桂: 8-9段目(rank=7,8)は不可

    if (isAttacker) {
        if (pt == Pawn || pt == Lance) return rank == 0;
        if (pt == Knight) return rank <= 1;
    } else {
        if (pt == Pawn || pt == Lance) return rank == 8;
        if (pt == Knight) return rank >= 7;
    }
    return false;
}

bool TsumeshogiPositionGenerator::isKingInCheck() const
{
    // 後手(WHITE)の玉に先手(BLACK)の駒から王手がかかっているか
    return EngineMoveValidator::isInCheck(m_board, Turn::White);
}

bool TsumeshogiPositionGenerator::isDefenderKingInCheck(const QString& sfen)
{
    ShogiBoard board;
    if (!board.trySetSfen(sfen)) return false;
    return EngineMoveValidator::isInCheck(board, Turn::White);
}

QString TsumeshogiPositionGenerator::buildSfen()
{
    // 後手持駒（守方）: 未使用駒を全て追加する
    if (m_settings.addRemainingToDefenderHand) {
        for (int pt = 0; pt < PieceTypeCount; ++pt) {
            m_board.addStandPiece(toWhite(kBlackPieces[pt]), kMaxCounts[pt] - m_usedCounts[pt]);
        }
    }

    // 手番は先手（攻方）、手数は1
    return m_board.convertBoardToSfen() + QStringLiteral(" b ") + m_board.convertStandToSfen()
           + QStringLiteral(" 1");
}

QStringList TsumeshogiPositionGenerator::generateBatch(
    const Settings& settings, int count, const CancelFlag& cancelFlag)
{
    QStringList result;
    result.reserve(count);

    TsumeshogiPositionGenerator generator;
    generator.setSettings(settings);

    for (int i = 0; i < count; ++i) {
        if (cancelFlag && cancelFlag->load()) break;

        const QString sfen = generator.generate();
        if (!sfen.isEmpty()) {
            result.append(sfen);
        }
    }
    return result;
}
