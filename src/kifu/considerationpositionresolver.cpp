/// @file considerationpositionresolver.cpp
/// @brief 検討モード向け局面解決ロジックの実装

#include "considerationpositionresolver.h"

#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifuloadcoordinator.h"
#include "kifurecordlistmodel.h"
#include "shogiutils.h"
#include "tsumepositionutil.h"

namespace {
const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

ConsiderationPositionResolver::ConsiderationPositionResolver(const Inputs& inputs)
    : m_inputs(inputs)
{
}

ConsiderationPositionResolver::UpdateParams
ConsiderationPositionResolver::resolveForRow(int row) const
{
    UpdateParams params;
    params.position = buildPositionStringForIndex(row);
    if (params.position.isEmpty()) {
        return params;
    }

    resolvePreviousMoveCoordinates(row, params.previousFileTo, params.previousRankTo);
    params.lastUsiMove = resolveLastUsiMoveForPly(row);
    return params;
}

QString ConsiderationPositionResolver::buildPositionStringForIndex(int moveIndex) const
{
    // 0) 分岐局面の現在表示SFENを最優先（DialogCoordinator経由の場合）
    if (m_inputs.currentSfenStr) {
        const QString currentSfen = m_inputs.currentSfenStr->trimmed();
        if (!currentSfen.isEmpty()) {
            if (currentSfen.startsWith(QStringLiteral("position "))) {
                return currentSfen;
            }
            if (currentSfen.startsWith(QStringLiteral("sfen "))) {
                return QStringLiteral("position ") + currentSfen;
            }
            return QStringLiteral("position sfen ") + currentSfen;
        }
    }

    // 1) 棋譜読み込み時に構築済みのposition文字列があればそれを優先
    if (m_inputs.positionStrList &&
        moveIndex >= 0 && moveIndex < m_inputs.positionStrList->size()) {
        return m_inputs.positionStrList->at(moveIndex);
    }

    // 2) 対局中/棋譜読み込み後のUSI指し手列から position ... moves を構築
    const QStringList* usiMoves = nullptr;
    if (m_inputs.gameUsiMoves && !m_inputs.gameUsiMoves->isEmpty()) {
        usiMoves = m_inputs.gameUsiMoves;
    } else if (m_inputs.kifuLoadCoordinator) {
        usiMoves = &m_inputs.kifuLoadCoordinator->kifuUsiMoves();
    }

    QString startCmd;
    const QString startSfen = m_inputs.startSfenStr ? *m_inputs.startSfenStr : QString();
    if (startSfen.isEmpty() || startSfen == kHirateSfen) {
        startCmd = QStringLiteral("startpos");
    } else {
        startCmd = QStringLiteral("sfen ") + startSfen;
    }

    if (usiMoves && !usiMoves->isEmpty()) {
        return TsumePositionUtil::buildPositionWithMoves(usiMoves, startCmd, moveIndex);
    }

    // 3) フォールバック: SFEN履歴から構築
    if (m_inputs.sfenRecord &&
        moveIndex >= 0 && moveIndex < m_inputs.sfenRecord->size()) {
        const QString sfen = m_inputs.sfenRecord->at(moveIndex);
        return QStringLiteral("position sfen ") + sfen;
    }

    // 4) 開始局面フォールバック（sfenRecordもUSI movesもない場合）
    if (startSfen.isEmpty() || startSfen == kHirateSfen) {
        return QStringLiteral("position startpos");
    }
    return QStringLiteral("position sfen ") + startSfen;
}

void ConsiderationPositionResolver::resolvePreviousMoveCoordinates(
    int row, int& fileTo, int& rankTo) const
{
    fileTo = 0;
    rankTo = 0;

    if (m_inputs.gameMoves && row >= 0 && row < m_inputs.gameMoves->size()) {
        const QPoint& toSquare = m_inputs.gameMoves->at(row).toSquare;
        fileTo = toSquare.x();
        rankTo = toSquare.y();
    } else if (m_inputs.kifuRecordModel &&
               row > 0 && row < m_inputs.kifuRecordModel->rowCount()) {
        // 棋譜読み込み時: 「同」表記も含めて表示モデルから座標を解決
        if (auto coord = ShogiUtils::parseMoveCoordinateFromModel(
                m_inputs.kifuRecordModel, row)) {
            auto [file, rank] = *coord;
            fileTo = file;
            rankTo = rank;
        }
    }
}

QString ConsiderationPositionResolver::resolveLastUsiMoveForPly(int row) const
{
    if (row <= 0) {
        return {};
    }

    QString lastUsiMove;

    // 分岐ライン上では現在ラインの実際の指し手を優先する。
    if (m_inputs.navState && m_inputs.branchTree) {
        const int lineIndex = m_inputs.navState->currentLineIndex();
        const QList<BranchLine> lines = m_inputs.branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            for (KifuBranchNode* node : line.nodes) {
                if (!node) {
                    continue;
                }
                if (node->ply() == row && node->isActualMove()) {
                    const ShogiMove mv = node->move();
                    if (mv.movingPiece != Piece::None) {
                        lastUsiMove = ShogiUtils::moveToUsi(mv);
                    }
                    break;
                }
            }
        }
    }

    const int usiIdx = row - 1;
    if (lastUsiMove.isEmpty() && usiIdx >= 0) {
        if (m_inputs.gameUsiMoves &&
            !m_inputs.gameUsiMoves->isEmpty() &&
            usiIdx < m_inputs.gameUsiMoves->size()) {
            lastUsiMove = m_inputs.gameUsiMoves->at(usiIdx);
        } else if (m_inputs.kifuLoadCoordinator) {
            const QStringList& kifuUsiMoves = m_inputs.kifuLoadCoordinator->kifuUsiMoves();
            if (usiIdx < kifuUsiMoves.size()) {
                lastUsiMove = kifuUsiMoves.at(usiIdx);
            }
        } else if (m_inputs.gameMoves && usiIdx < m_inputs.gameMoves->size()) {
            lastUsiMove = ShogiUtils::moveToUsi(m_inputs.gameMoves->at(usiIdx));
        }
    }

    return lastUsiMove;
}
