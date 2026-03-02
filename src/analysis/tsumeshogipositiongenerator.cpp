/// @file tsumeshogipositiongenerator.cpp
/// @brief ランダム詰将棋候補局面生成クラスの実装

#include "tsumeshogipositiongenerator.h"
#include "enginemovevalidator.h"

#include <QtConcurrent>


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
    placeOnBoard(kingFile, kingRank, QStringLiteral("k"));

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
                if (isInvalidPlacement(pt, r, true)) {
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

                const QString sfenPiece = promote ? promotedSfen(pt, true) : unpromatedSfen(pt, true);
                placeOnBoard(f, r, sfenPiece);
                usePiece(pt);
                placed = true;
                break;
            }
            if (!placed) {
                // 盤上配置失敗 → 持駒に入れる
                m_attackerHand[pt]++;
                usePiece(pt);
            }
        } else {
            // 持駒に追加（成駒は不可なので生駒として）
            m_attackerHand[pt]++;
            usePiece(pt);
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
            if (isInvalidPlacement(pt, r, false)) {
                if (kCanPromote[pt]) {
                    promote = true;
                } else {
                    continue;
                }
            }

            const QString sfenPiece = promote ? promotedSfen(pt, false) : unpromatedSfen(pt, false);
            placeOnBoard(f, r, sfenPiece);
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
    for (int r = 0; r < 9; ++r) {
        for (int f = 0; f < 9; ++f) {
            m_board[r][f] = QChar();
        }
    }
    for (int i = 0; i < PieceTypeCount; ++i) {
        m_usedCounts[i] = 0;
        m_attackerHand[i] = 0;
    }
}

bool TsumeshogiPositionGenerator::isOccupied(int file, int rank) const
{
    return !m_board[rank][file].isNull();
}

bool TsumeshogiPositionGenerator::placeOnBoard(int file, int rank, const QString& sfenPiece)
{
    if (file < 0 || file > 8 || rank < 0 || rank > 8) return false;
    if (isOccupied(file, rank)) return false;

    // sfenPiece を1文字のマーカーとして格納（後でbuildSfenで展開）
    // 特殊エンコーディング: 成駒は '+' + 英字なので2文字
    // board配列には先頭文字を格納し、成駒フラグは上位ビットで表現
    // → 簡易実装: boardをQString[9][9]に変更する代わりに、別途マップを使う
    // ここでは QChar の unicode を利用して成駒をエンコード:
    //   生駒: そのままの文字 P,L,N,S,G,B,R,p,l,n,s,g,b,r,k
    //   成駒: 0xE000 + 元文字コード (Private Use Area)
    if (sfenPiece.startsWith('+')) {
        const QChar base = sfenPiece.at(1);
        m_board[rank][file] = QChar(0xE000 + base.unicode());
    } else {
        m_board[rank][file] = sfenPiece.at(0);
    }
    return true;
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

QString TsumeshogiPositionGenerator::promotedSfen(PieceType pt, bool isAttacker) const
{
    const QChar c = isAttacker ? QChar(kSfenChars[pt]).toUpper() : QChar(kSfenChars[pt]).toLower();
    return QStringLiteral("+") + c;
}

QString TsumeshogiPositionGenerator::unpromatedSfen(PieceType pt, bool isAttacker) const
{
    const QChar c = isAttacker ? QChar(kSfenChars[pt]).toUpper() : QChar(kSfenChars[pt]).toLower();
    return QString(c);
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

bool TsumeshogiPositionGenerator::isInvalidPlacement(PieceType pt, int rank, bool isAttacker) const
{
    return needsPromotion(pt, rank, isAttacker);
}

QChar TsumeshogiPositionGenerator::promotedPieceChar(QChar base)
{
    // SFEN文字 → ShogiBoard内部の成駒文字に変換
    // P→Q, L→M, N→O, S→T, B→C, R→U (大文字/小文字両対応)
    static const QMap<QChar, QChar> promotionMap = {
        {'P', 'Q'}, {'L', 'M'}, {'N', 'O'}, {'S', 'T'}, {'B', 'C'}, {'R', 'U'},
        {'p', 'q'}, {'l', 'm'}, {'n', 'o'}, {'s', 't'}, {'b', 'c'}, {'r', 'u'}
    };
    return promotionMap.value(base, base);
}

QList<Piece> TsumeshogiPositionGenerator::buildBoardData() const
{
    QList<Piece> boardData(81, Piece::None);

    for (int rank = 0; rank < 9; ++rank) {
        for (int file = 0; file < 9; ++file) {
            const QChar c = m_board[rank][file];
            if (c.isNull()) continue;

            if (c.unicode() >= 0xE000) {
                // 成駒: Private Use Area からベース文字を復元し、成駒文字に変換
                const QChar base = QChar(c.unicode() - 0xE000);
                boardData[rank * 9 + file] = charToPiece(promotedPieceChar(base));
            } else {
                boardData[rank * 9 + file] = charToPiece(c);
            }
        }
    }
    return boardData;
}

bool TsumeshogiPositionGenerator::isKingInCheck() const
{
    const QList<Piece> boardData = buildBoardData();
    EngineMoveValidator validator;
    // 後手(WHITE)の玉に先手(BLACK)の駒から王手がかかっているか
    return validator.checkIfKingInCheck(EngineMoveValidator::WHITE, boardData) > 0;
}

QString TsumeshogiPositionGenerator::buildSfen() const
{
    QString boardStr;
    boardStr.reserve(200);

    // 盤面部分
    for (int rank = 0; rank < 9; ++rank) {
        int emptyCount = 0;
        // SFENは9筋(file=8)から1筋(file=0)の順
        for (int file = 8; file >= 0; --file) {
            const QChar c = m_board[rank][file];
            if (c.isNull()) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    boardStr += QString::number(emptyCount);
                    emptyCount = 0;
                }
                // 成駒判定
                if (c.unicode() >= 0xE000) {
                    boardStr += '+';
                    boardStr += QChar(c.unicode() - 0xE000);
                } else {
                    boardStr += c;
                }
            }
        }
        if (emptyCount > 0) {
            boardStr += QString::number(emptyCount);
        }
        if (rank < 8) boardStr += '/';
    }

    // 手番（先手=攻方）
    boardStr += QStringLiteral(" b ");

    // 持駒部分
    QString handStr;

    // 先手持駒（攻方）: R, B, G, S, N, L, P の順
    static constexpr PieceType handOrder[] = {Rook, Bishop, Gold, Silver, Knight, Lance, Pawn};
    for (PieceType pt : handOrder) {
        if (m_attackerHand[pt] > 0) {
            if (m_attackerHand[pt] > 1) {
                handStr += QString::number(m_attackerHand[pt]);
            }
            handStr += QChar(kSfenChars[pt]).toUpper();
        }
    }

    // 後手持駒（守方）: 未使用駒を全て追加
    if (m_settings.addRemainingToDefenderHand) {
        for (PieceType pt : handOrder) {
            const int remaining = kMaxCounts[pt] - m_usedCounts[pt];
            if (remaining > 0) {
                if (remaining > 1) {
                    handStr += QString::number(remaining);
                }
                handStr += QChar(kSfenChars[pt]).toLower();
            }
        }
    }

    if (handStr.isEmpty()) {
        boardStr += '-';
    } else {
        boardStr += handStr;
    }

    // 手数
    boardStr += QStringLiteral(" 1");

    return boardStr;
}

QStringList TsumeshogiPositionGenerator::generateBatch(
    const Settings& settings, int count, const CancelFlag& cancelFlag)
{
    QList<int> indices;
    indices.reserve(count);
    for (int i = 0; i < count; ++i)
        indices.append(i);

    return QtConcurrent::blockingMappedReduced<QStringList>(
        indices,
        [settings, cancelFlag](int /*index*/) -> QString {
            if (cancelFlag && cancelFlag->load()) return {};
            TsumeshogiPositionGenerator gen;
            gen.setSettings(settings);
            return gen.generate();
        },
        [](QStringList& result, const QString& sfen) {
            if (!sfen.isEmpty()) {
                result.append(sfen);
            }
        },
        QtConcurrent::UnorderedReduce);
}
