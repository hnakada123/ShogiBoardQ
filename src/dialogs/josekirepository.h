/// @file josekirepository.h
/// @brief 定跡データのファイルI/Oを担当するリポジトリクラス

#ifndef JOSEKIREPOSITORY_H
#define JOSEKIREPOSITORY_H

#include <QMap>
#include <QVector>
#include <QString>
#include <QSet>

struct JosekiMove;

/**
 * @brief 定跡ファイルの読み書きとデータ保持を担当するリポジトリ
 *
 * YANEURAOU-DB2016 形式の定跡ファイルを読み込み・保存する。
 * 定跡データのインメモリ管理も行う。
 */
class JosekiRepository
{
public:
    JosekiRepository() = default;

    // --- ファイルI/O ---

    /**
     * @brief 定跡ファイルを読み込む
     * @param filePath ファイルパス
     * @param errorMessage エラーメッセージ（エラー時に設定される）
     * @return 読み込み成功時 true
     */
    bool loadFromFile(const QString &filePath, QString *errorMessage = nullptr);

    /**
     * @brief 定跡データをファイルに保存する
     * @param filePath 保存先ファイルパス
     * @param errorMessage エラーメッセージ（エラー時に設定される）
     * @return 保存成功時 true
     */
    bool saveToFile(const QString &filePath, QString *errorMessage = nullptr) const;

    // --- データアクセス ---

    /** @brief 定跡データ全体への参照 */
    const QMap<QString, QVector<JosekiMove>> &josekiData() const { return m_josekiData; }
    QMap<QString, QVector<JosekiMove>> &josekiData() { return m_josekiData; }

    /** @brief 元のSFEN（手数付き）マップへの参照 */
    const QMap<QString, QString> &sfenWithPlyMap() const { return m_sfenWithPlyMap; }
    QMap<QString, QString> &sfenWithPlyMap() { return m_sfenWithPlyMap; }

    /** @brief マージ登録済み指し手セットへの参照 */
    const QSet<QString> &mergeRegisteredMoves() const { return m_mergeRegisteredMoves; }
    QSet<QString> &mergeRegisteredMoves() { return m_mergeRegisteredMoves; }

    /** @brief 全データをクリアする */
    void clear();

    /** @brief 定跡データが空かどうか */
    bool isEmpty() const { return m_josekiData.isEmpty(); }

    /** @brief 局面数を返す */
    int positionCount() const { return static_cast<int>(m_josekiData.size()); }

    /** @brief 指定局面の定跡手を検索する */
    bool containsPosition(const QString &normalizedSfen) const;

    /** @brief 指定局面の定跡手を取得する */
    const QVector<JosekiMove> &movesForPosition(const QString &normalizedSfen) const;

    /** @brief 指定局面に定跡手を追加する */
    void addMove(const QString &normalizedSfen, const JosekiMove &move);

    /** @brief 指定局面の指し手を更新する */
    void updateMove(const QString &normalizedSfen, const QString &usiMove,
                    int value, int depth, int frequency, const QString &comment);

    /** @brief 指定局面の指し手を削除する */
    void deleteMove(const QString &normalizedSfen, int index);

    /** @brief 指定局面から同じUSI指し手を削除する */
    void removeMoveByUsi(const QString &normalizedSfen, const QString &usiMove);

    /**
     * @brief マージ登録（既存なら頻度+1、なければ新規追加）
     * @param normalizedSfen 正規化SFEN
     * @param sfenWithPly 手数付きSFEN
     * @param usiMove USI形式の指し手
     */
    void registerMergeMove(const QString &normalizedSfen, const QString &sfenWithPly,
                           const QString &usiMove);

    /**
     * @brief 手数付きSFENを登録する（未登録時のみ）
     * @param normalizedSfen 正規化SFEN
     * @param sfenWithPly 手数付きSFEN
     */
    void ensureSfenWithPly(const QString &normalizedSfen, const QString &sfenWithPly);

    /**
     * @brief 指定局面の元SFEN（手数付き）を取得する
     * @param normalizedSfen 正規化SFEN
     * @return 手数付きSFEN。未登録なら空文字列
     */
    QString sfenWithPly(const QString &normalizedSfen) const;

private:
    /// 定跡データ（正規化SFEN → 指し手リスト）
    QMap<QString, QVector<JosekiMove>> m_josekiData;

    /// 元のSFEN（手数付き）を保持（正規化SFEN → 元のSFEN）
    QMap<QString, QString> m_sfenWithPlyMap;

    /// マージダイアログで登録済みの指し手セット（「正規化SFEN:USI指し手」形式）
    QSet<QString> m_mergeRegisteredMoves;

    /// 空の定跡手リスト（参照返却のフォールバック用）
    static const QVector<JosekiMove> s_emptyMoves;
};

#endif // JOSEKIREPOSITORY_H
