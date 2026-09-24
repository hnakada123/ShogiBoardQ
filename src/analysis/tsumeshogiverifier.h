#ifndef TSUMESHOGIVERIFIER_H
#define TSUMESHOGIVERIFIER_H

#include <QString>
#include <QStringList>
#include <memory>

/// 全変化・最終手・成不成を区別する、生成用の厳格な唯一解検査。
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

    TsumeshogiVerifier();
    ~TsumeshogiVerifier();
    void start(const QString& sfen, int targetMoves);
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
