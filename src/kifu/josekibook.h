#ifndef JOSEKIBOOK_H
#define JOSEKIBOOK_H

/// @file josekibook.h
/// @brief 定跡のデータ（局面ごとの定跡手）の値型と、やねうら王定跡形式（YANEURAOU-DB2016）の読み書き

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

#include "josekitypes.h"

/// 定跡の局面1つの登録（ファイルの sfen 行1つと、その下の定跡手）
struct JosekiPosition {
    int ply = 1;              ///< 登録したときの手数（ファイルの sfen 行の手数）
    QList<JosekiMove> moves;  ///< 定跡手（1つ以上）
};

/**
 * @brief 定跡のデータ（値型）
 *
 * 局面のキー（手数を除いた SFEN。持ち駒の並びは正規化する）から局面の登録を引く map を1つだけ持つ。
 * 局面は手数を区別せずに引く（局面は手数付きの SFEN のままで渡してよい）。
 *
 * やねうら王の定跡ファイルには、同じ局面が手数違いで別々に登録されていることがある（やねうら王は既定では
 * 手数まで一致する登録だけを使う）。保存してもそれぞれの手数の登録が残るように、1つのキーに登録をファイルの順に
 * 並べて持つ。引くときは、渡した SFEN と同じ手数の登録、無ければ最初の登録を使う（表示・編集・マージで同じ）。
 *
 * 定跡手のない登録は持たない（最後の定跡手を消すと登録も消える）。
 * 棋譜からマージで登録した指し手の印（局面のキーと指し手の組）も持つ。
 */
class JosekiBook
{
public:
    /// 定跡の局面のキー（手数を除き、持ち駒の並びを正規化した SFEN）
    static QString positionKey(const QString &sfen);
    /// ファイルから読んだ登録で作る（定跡手のない登録は捨てる。JosekiBookFile::load が使う）
    static JosekiBook fromPositions(QMap<QString, QList<JosekiPosition>> positions);

    bool isEmpty() const { return m_positions.isEmpty(); }
    /// 局面の数（同じ局面を手数違いで登録していても1つと数える）
    int positionCount() const { return static_cast<int>(m_positions.size()); }
    /// 全局面（キーの順）。手数違いの登録は1つのキーにファイルの順に並ぶ
    const QMap<QString, QList<JosekiPosition>> &positions() const { return m_positions; }

    /// 局面の登録（無ければ nullptr）。手数違いの登録があれば sfen と同じ手数のもの、無ければ最初のもの
    const JosekiPosition *find(const QString &sfen) const;
    /// find() の登録の手数付きの SFEN（ファイルの sfen 行。無ければ空）
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
    /// find() の登録（書き換え用。無ければ nullptr）
    JosekiPosition *findForWrite(const QString &key, const QString &sfen);
    /// find() の登録。無ければ sfen の手数で登録を作る
    JosekiPosition &findOrCreate(const QString &key, const QString &sfen);

    QMap<QString, QList<JosekiPosition>> m_positions;
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

/// やねうら王定跡形式の読み書き（UI に触れないので、ワーカースレッドから呼べる）
namespace JosekiBookFile {
[[nodiscard]] JosekiLoadResult load(const QString &filePath);
[[nodiscard]] JosekiSaveResult save(const QString &filePath, const JosekiBook &book);
} // namespace JosekiBookFile

#endif // JOSEKIBOOK_H
