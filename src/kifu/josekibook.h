#ifndef JOSEKIBOOK_H
#define JOSEKIBOOK_H

/// @file josekibook.h
/// @brief 定跡のデータ（局面ごとの定跡手）の値型と、やねうら王定跡形式（YANEURAOU-DB2016）の読み書き

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <optional>

#include "josekitypes.h"

class JosekiBook;
struct JosekiLoadResult;
struct JosekiSaveResult;

/// やねうら王定跡形式の読み書き（UI に触れないので、ワーカースレッドから呼べる）
namespace JosekiBookFile {
[[nodiscard]] JosekiLoadResult load(const QString &filePath);
[[nodiscard]] JosekiSaveResult save(const QString &filePath, const JosekiBook &book);
} // namespace JosekiBookFile

/**
 * @brief 定跡のデータ（値型）
 *
 * 局面のキー（手数を除いた SFEN。持ち駒の並びは正規化する）から局面の登録を引く表を1つだけ持つ。
 * 局面は手数を区別せずに引く（局面は手数付きの SFEN のままで渡してよい）。
 *
 * やねうら王の定跡ファイルには、同じ局面が手数違いで別々に登録されていることがある（やねうら王は既定では
 * 手数まで一致する登録だけを使う）。保存してもそれぞれの手数の登録が残るように、1つのキーに登録をファイルの順に
 * 並べて持つ。引くときは、渡した SFEN と同じ手数の登録、無ければ最初の登録を使う（表示・編集・マージで同じ）。
 *
 * 大きな定跡（数百万手）も読めるように詰めた形で持つ。キーは UTF-8 のバイト列、定跡手は指し手と予想応手を
 * 16ビットの番号にした値で、1つの局面の登録は見出し（手数）と定跡手を1本の配列に並べる。USI として読めない
 * 指し手の文字列とコメントは別の表に入れ、取り出すときに JosekiMove に戻す。
 *
 * 定跡手のない登録は持たない（最後の定跡手を消すと登録も消える）。
 * 棋譜からマージで登録した指し手の印（局面のキーと指し手の組）も持つ。
 */
class JosekiBook
{
public:
    /// 定跡の局面のキー（手数を除き、持ち駒の並びを正規化した SFEN）
    static QString positionKey(const QString &sfen);

    bool isEmpty() const { return m_positions.isEmpty(); }
    /// 局面の数（同じ局面を手数違いで登録していても1つと数える）
    int positionCount() const { return static_cast<int>(m_positions.size()); }

    /// 局面が登録されているか
    bool contains(const QString &sfen) const;
    /// 局面の登録（手数違いの登録があれば sfen と同じ手数のもの、無ければ最初のもの）の手数付きの SFEN（無ければ空）
    QString registeredSfen(const QString &sfen) const;
    /// 局面を登録した手数（ファイルの順。無ければ空）
    QList<int> registeredPlies(const QString &sfen) const;
    /// 局面の定跡手（無ければ空）
    QList<JosekiMove> movesAt(const QString &sfen) const;
    bool hasMove(const QString &sfen, const QString &usiMove) const;

    /// 定跡手を足す。同じ指し手があればその場で置き換える。新しい局面は sfen の手数（無ければ1手目）で登録する
    void putMove(const QString &sfen, const JosekiMove &move);
    /// move.move の定跡手の値・コメントを書き換える（move.nextMove が null なら予想応手は変えない）。無ければ false
    bool updateMove(const QString &sfen, const JosekiMove &move);
    /// 定跡手を消す（最後の手なら局面も消す）。無ければ false
    bool removeMove(const QString &sfen, const QString &usiMove);

    /// 棋譜からのマージで登録する（あれば出現頻度を1増やし、無ければ足す）。登録済みの印を付ける
    void registerMergeMove(const QString &sfen, const QString &usiMove);
    /// マージで登録した印のある指し手か
    bool isMergeRegistered(const QString &sfen, const QString &usiMove) const;
    /// マージで登録した印（局面のキーと指し手の組）
    const QSet<JosekiMoveKey> &mergeRegistered() const { return m_mergeRegistered; }

    void clear();

private:
    friend JosekiLoadResult JosekiBookFile::load(const QString &filePath);
    friend JosekiSaveResult JosekiBookFile::save(const QString &filePath, const JosekiBook &book);

    /// 定跡手1つ（詰めた形）。登録の見出し（move が kHeader）では value が手数
    struct PackedMove {
        quint16 move = 0;      ///< 指し手の番号（encodeMove）
        quint16 next = 0;      ///< 予想応手の番号
        qint32 value = 0;
        qint32 depth = 0;
        qint32 frequency = 0;
        quint32 comment = 0;   ///< m_comments の番号（0 は無し）
    };
    /// 1つの局面の登録をファイルの順に並べた列（各登録は見出しの後に定跡手が続く）
    using Records = QList<PackedMove>;
    /// 1つの登録の範囲（見出しの位置と、定跡手の終わりの次の位置）
    struct Range {
        qsizetype header = 0;
        qsizetype end = 0;
    };

    /// 登録の見出しの印（PackedMove::move）
    static constexpr quint16 kHeader = 0xFFFF;

    static QByteArray keyOf(const QString &sfen);
    /// records の登録の範囲（ファイルの順）
    static QList<Range> rangesOf(const Records &records);
    /// sfen と同じ手数の登録の範囲、無ければ最初の登録の範囲（records は空でない）
    static Range resolve(const Records &records, const QString &sfen);
    /// 範囲の中の指し手の位置（無ければ -1）
    static qsizetype indexOf(const Records &records, Range range, quint16 move);

    /// 指し手の文字列を番号にする（USI として読めない文字列は表に入れる。空は "none"）
    quint16 encodeMove(QStringView text);
    /// 指し手の文字列の番号（表に無い読めない文字列なら nullopt。表には足さない）
    std::optional<quint16> findMoveCode(QStringView text) const;
    QString decodeMove(quint16 code) const;
    /// 指し手の文字列を out の後ろに足す（保存で使う）
    void appendMoveText(QByteArray &out, quint16 code) const;
    /// コメントを表に入れて番号を返す（空なら 0）。index が 0 でなければその番号の中身を書き換える
    quint32 storeComment(const QString &comment, quint32 index = 0);
    /// move の予想応手・値・コメントを target に入れる（コメントの番号は target のものを使い回す）
    void assign(PackedMove &target, const JosekiMove &move);
    JosekiMove unpack(const PackedMove &packed) const;
    /// sfen の局面の登録の範囲。無ければ sfen の手数で登録を作る
    Range findOrCreate(Records &records, const QString &sfen);

    QHash<QByteArray, Records> m_positions;
    QStringList m_tokens;                  ///< USI として読めない指し手の文字列（番号は kTokenBase から）
    QHash<QString, quint16> m_tokenCodes;  ///< m_tokens の逆引き
    /// コメント（[0] は空）。消した定跡手のコメントは次に読み込むまで残る
    QStringList m_comments{QString()};
    QSet<JosekiMoveKey> m_mergeRegistered;
};

/// 定跡ファイルの読み込み結果（ワーカースレッドで作り、メインスレッドで使う値）
struct JosekiLoadResult {
    bool success = false;
    QString errorMessage;
    JosekiBook book;
};

/// 定跡ファイルの保存結果
struct JosekiSaveResult {
    bool success = false;
    QString errorMessage;
    int savedCount = 0;  ///< 書いた局面の登録（sfen 行）の数
};

#endif // JOSEKIBOOK_H
