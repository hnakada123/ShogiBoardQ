#ifndef TSUMELINEMETRICS_H
#define TSUMELINEMETRICS_H

/// @file tsumelinemetrics.h
/// @brief 詰将棋の局面と詰み手順の評価（手順の特徴・ビームの優先度・類似作の判定キー）

#include <QString>
#include <QStringList>

#include <optional>

/// 局面と詰み手順（PV）を調べる評価。局面は変えない（候補局面を作るのは TsumeRetrogradeOps）。
namespace TsumeLineMetrics {

/// 局面と合法PVのメトリクス（長手数ヒューリスティック評価用）
struct Metrics {
    int plies = 0;               ///< 手数
    int surplusCount = 0;        ///< 詰み上がり時の攻方持駒枚数（駒余り）
    int finalImmediateMates = 1; ///< 最終手直前局面での即詰み攻手数（1なら最終手一意）
    int kingMoves = 0;           ///< PV中の玉移動回数
    int kingDistance = 0;        ///< PV中の玉の移動距離合計（マンハッタン距離）
    int distinctKingSquares = 0; ///< PV中に玉が訪れた異なるマス数
    int sacrifices = 0;          ///< 玉方が攻方駒を取った回数（捨駒）
    int interpositions = 0;      ///< 玉方が合駒を打った回数
    int attackPieces = 0;        ///< 開始局面の攻方駒数（盤上+持駒）
    int defendBoardPieces = 0;   ///< 開始局面の玉以外の玉方盤上駒数
};

/// 局面とPVを辿り、終端詰み・連続王手・合法性を確認した上でメトリクスを返す。
std::optional<Metrics> analyzeLine(const QString& sfen, const QStringList& pv);

/// 長手数向けヒューリスティック（案B）によるビームノード優先スコア。
double scoreNode(const Metrics& metrics, int targetMoves, int expansionCount,
                 bool firstMoveUnique, bool strictNoSurplus,
                 bool strictFinalMove = true);

/// 攻方手番局面における1手詰（即詰）の攻手数を数える（打ち歩詰め除外、成・不成は別手）。
int countImmediateMates(const QString& sfen);

/// ビームの多様性維持用の系統キー（玉位置・先頭手順の移動先と打ち駒種）。
QString structuralSignature(const QString& sfen, const QStringList& pv);

/// scripts/tsume_diversity.py と同じ基準の類似作判定キー一覧を返す
/// （最終手を除く玉原点・左右反転正規化 prefix、絶対座標、9手以上の中間手順1手違い core）。
QStringList diversityKeys(const QString& sfen, const QStringList& pv);

} // namespace TsumeLineMetrics

#endif // TSUMELINEMETRICS_H
