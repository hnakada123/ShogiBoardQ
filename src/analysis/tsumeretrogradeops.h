#ifndef TSUMERETROGRADEOPS_H
#define TSUMERETROGRADEOPS_H

/// @file tsumeretrogradeops.h
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・長手数向け評価（純粋局面演算）

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <random>

namespace TsumeRetrogradeOps {

/// 逆算・盤面編集時の駒数と盤面構成の上限
struct Limits {
    int maxAttackPieces = 20;      ///< 攻方の盤上+持駒の合計上限（玉除く）
    int maxDefendPieces = 14;      ///< 玉以外の玉方盤上駒の上限
    int maxTotalBoardPieces = 30;  ///< 盤上の総駒数上限（玉含む）
    bool addRemainingToDefenderHand = true; ///< 未使用駒をすべて玉方持駒へ入れる（40駒保存則）
};

/// 逆算・編集・リペアで生成した候補局面
struct Candidate {
    QString sfen;              ///< 正規化済みの攻方（先手）手番SFEN
    QString tag;               ///< 操作種別（ログ・統計用）
    QString intendedFirstMove; ///< 逆算時に想定した攻方の初手（USI、不明なら空）
};

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

/// 目標手数超過PVから切り出した目標手数の部分局面
struct SuffixSlice {
    QString sfen;
    QStringList pv;
};

/// 攻方持駒ゼロ・後手玉詰み上がり（玉方手番・王手あり・合法手0）の初期局面を生成する。
/// 盤端・隅・中段寄りの多様な収束形を作り、1手以上逆算可能なものだけを返す。
QStringList generateMateEndings(const Limits& limits, int count, std::mt19937_64& rng);

/// 0手詰み上がり局面（玉方手番・王手あり・合法手0）から直前の攻方王手1手を戻し、
/// 攻方手番の1手詰候補を生成する。
QList<Candidate> reverseFromMateEnding(const QString& mateEndingSfen, const Limits& limits,
                                       int maxCandidates, std::mt19937_64& rng);

/// 攻方手番の局面から「玉方応手1手＋攻方王手1手」を逆算し、2手前の攻方手番候補を生成する。
/// 玉移動・玉の駒取り（捨駒）・打合・移動合・玉方駒の取り返しと、
/// 駒打ち・盤上移動・成り戻し・駒取り戻し・開き王手・両王手を組み合わせ、
/// 全候補について順方向シミュレーションで完全な合法性を保証する。
QList<Candidate> generatePredecessors(const QString& sfen, const Limits& limits,
                                      int maxCandidates, std::mt19937_64& rng);

/// 攻方手番の局面に対して駒の移動・追加・削除・駒種変更・成不成・持駒調整を行う。
QList<Candidate> generateMutations(const QString& sfen, const Limits& limits,
                                   int maxCandidates, std::mt19937_64& rng);

/// 余詰の攻方初手 cookMove を阻止し、主手順 mainMove を活かす標的型リペア候補を生成する。
QList<Candidate> generateCookRepairs(const QString& sfen, const QString& mainMove,
                                     const QString& cookMove, const Limits& limits,
                                     int maxCandidates, std::mt19937_64& rng);

/// 目標手数を超える詰み手順（pv.size() > targetMoves）の先頭を進め、
/// ちょうど targetMoves 手の接尾辞局面を切り出す。
std::optional<SuffixSlice> sliceToTargetLength(const QString& sfen, const QStringList& pv, int targetMoves);

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

/// 詰将棋開始局面としての妥当性（片玉・40駒以内・二歩なし・行き所のない駒なし・初期王手なし）を検査し、
/// 正規化した攻方手番SFENを返す。不正な場合は空文字列。
QString normalizeProblemSfen(const QString& sfen, bool addRemainingToDefenderHand = true);

/// 詰み上がり局面（玉方手番・王手あり・合法手0）として妥当かを判定する。
bool isMateEnding(const QString& sfen);

} // namespace TsumeRetrogradeOps

#endif // TSUMERETROGRADEOPS_H
