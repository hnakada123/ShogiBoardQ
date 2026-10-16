#ifndef JOSEKIBOOK_H
#define JOSEKIBOOK_H

/// @file josekibook.h
/// @brief 定跡のデータ（局面ごとの定跡手）の値型と、やねうら王定跡形式（YANEURAOU-DB2016）の読み書き

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

#include "josekitypes.h"

/// 定跡の局面1つ
struct JosekiPosition {
    QString sfen;             ///< 登録したときの手数付きの SFEN（ファイルの sfen 行に書く）
    QList<JosekiMove> moves;  ///< 定跡手（1つ以上）
};

/**
 * @brief 定跡のデータ（値型）
 *
 * 局面のキー（手数を除いた SFEN。持ち駒の並びは正規化する）から局面1つを引く map を1つだけ持つ。
 * 定跡手のない局面は持たない（最後の定跡手を消すと局面も消える）。局面は手数付きの SFEN のままで渡してよい。
 * 棋譜からマージで登録した指し手の印（局面のキーと指し手の組）も持つ。
 */
class JosekiBook
{
public:
    /// 定跡の局面のキー（手数を除き、持ち駒の並びを正規化した SFEN）
    static QString positionKey(const QString &sfen);

    bool isEmpty() const { return m_positions.isEmpty(); }
    int positionCount() const { return static_cast<int>(m_positions.size()); }
    /// 全局面（キーの順）
    const QMap<QString, JosekiPosition> &positions() const { return m_positions; }

    /// 局面（無ければ nullptr）
    const JosekiPosition *find(const QString &sfen) const;
    /// 局面の定跡手（無ければ空）
    QList<JosekiMove> movesAt(const QString &sfen) const;
    bool hasMove(const QString &sfen, const QString &usiMove) const;

    /// 定跡手を足す。同じ指し手があれば置き換える。新しい局面は sfen（手数がなければ1手目）で覚える
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
    QMap<QString, JosekiPosition> m_positions;
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
    int savedCount = 0;
};

/// やねうら王定跡形式の読み書き（UI に触れないので、ワーカースレッドから呼べる）
namespace JosekiBookFile {
[[nodiscard]] JosekiLoadResult load(const QString &filePath);
[[nodiscard]] JosekiSaveResult save(const QString &filePath, const JosekiBook &book);
} // namespace JosekiBookFile

#endif // JOSEKIBOOK_H
