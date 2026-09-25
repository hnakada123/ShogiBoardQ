#ifndef TSUMESHOGIVERIFIER_H
#define TSUMESHOGIVERIFIER_H

#include <QString>
#include <QStringList>
#include <memory>

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
    enum class Status { Running, Unique, Multiple, NoMate, WrongLength, Unknown, Invalid };
    enum class Reply { Mate, NoMate, Unknown };
    struct Result {
        Status status = Status::Invalid;
        QStringList pv;
    };
    struct Options {
        bool allowFinalMoveAlternatives = true; ///< 主手順の最終手（根を除く）の複数解を許容する
    };

    TsumeshogiVerifier();
    ~TsumeshogiVerifier();
    void start(const QString& sfen, int targetMoves, const Options& options);
    void start(const QString& sfen, int targetMoves) { start(sfen, targetMoves, Options()); }
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
