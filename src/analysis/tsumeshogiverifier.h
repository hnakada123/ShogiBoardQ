#ifndef TSUMESHOGIVERIFIER_H
#define TSUMESHOGIVERIFIER_H

#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include <string>

/// 詰将棋の慣例に沿った、生成用の唯一解検査。
///
/// 主手順（玉方が最も長く抵抗する変化。同手数なら全部）の各攻手が一意であることを要求する。
/// 別の攻手で詰む場合は手数の長短を問わず余詰として棄却し、成・不成は別の手として数える。
/// 玉方が早く詰む短い変化での攻方の別の詰め方（変化別詰）は許容する。
/// Options::allowFinalMoveAlternatives なら、主手順の最終手（根の局面を除く）に複数の詰手があっても採択する。
///
/// 問い合わせは常に攻方手番。外部エンジンの nomate だけを不詰証明とする。
/// 深さ制限による不詰扱いはせず、時間切れ等は Unknown として採択しない。
class TsumeshogiVerifier
{
public:
    enum class Status { Running, Unique, Multiple, NoMate, WrongLength, Unknown, Invalid, Surplus };
    enum class Reply { Mate, NoMate, Unknown };
    struct CacheEntry {
        Reply reply = Reply::Unknown;
        int len = 0;
    };
    struct Result {
        Status status = Status::Invalid;
        QStringList pv;
        int checkedMatePositions = 0;
        QString mainMove; ///< 余詰検出時に先に見つかった詰む攻手
        QString cookMove; ///< 余詰検出時に2つ目として見つかった詰む攻手
        int cookPly = -1; ///< 余詰を検出した手数（0 = 初手）
    };
    struct Options {
        bool allowFinalMoveAlternatives = true; ///< 主手順の最終手（根を除く）の複数解を許容する
        /// 最長抵抗の全主手順・全最終手で攻方持駒が空であることを要求する保守的な収集条件。
        /// 駒余りになる長い変化から、駒余りのない短い変化への手順の選び直しは行わない。
        bool requireNoSurplus = false;
        int maxVisitedNodes = 10000;            ///< 検査ノード上限（超過時は Unknown）
        int maxCacheEntries = 10000;            ///< 応答キャッシュ上限（超過時は Unknown）
        bool skipImmediateMateQueries = true;   ///< 応手後に1手詰が存在する場合、外部問い合わせを省略して1手と確定する
    };

    TsumeshogiVerifier();
    ~TsumeshogiVerifier();
    void start(const QString& sfen, int targetMoves, const Options& options);
    void start(const QString& sfen, int targetMoves) { start(sfen, targetMoves, Options()); }
    /// 確定済みの問い合わせキャッシュ（Mate / NoMate）を注入する（start() 後に呼ぶ）
    void seedCache(const std::map<std::string, CacheEntry>& entries);
    /// 現在の確定済み問い合わせキャッシュ（Mate / NoMate）を取り出す
    std::map<std::string, CacheEntry> exportCache() const;
    /// 次に go mate で調べる攻方手番のSFEN。
    /// 空文字列でもRunningなら、イベントループに戻ってから再度呼び出す。
    QString nextPosition();
    void submit(Reply reply, const QStringList& pv = {});
    void abort();
    const Result& result() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // TSUMESHOGIVERIFIER_H
