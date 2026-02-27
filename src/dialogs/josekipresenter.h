/// @file josekipresenter.h
/// @brief 定跡ウィンドウのプレゼンタークラス

#ifndef JOSEKIPRESENTER_H
#define JOSEKIPRESENTER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class JosekiRepository;
struct JosekiMove;
struct KifuMergeEntry;
class SfenPositionTracer;

/**
 * @brief 定跡ウィンドウの操作ロジックを担当するプレゼンター
 *
 * UI非依存のビジネスロジック（SFEN正規化、指し手変換、データ操作フロー）を
 * JosekiWindow から分離する。
 */
class JosekiPresenter : public QObject
{
    Q_OBJECT

public:
    explicit JosekiPresenter(JosekiRepository *repository, QObject *parent = nullptr);

    // --- SFEN操作 ---

    /**
     * @brief SFEN文字列を正規化する（手数部分を除く）
     * @param sfen SFEN文字列
     * @return 正規化されたSFEN文字列
     */
    static QString normalizeSfen(const QString &sfen);

    // --- 指し手変換 ---

    /**
     * @brief USI形式の指し手を日本語表記に変換する
     * @param usiMove USI形式の指し手
     * @param plyNumber 手数
     * @param tracer 現在局面をセットしたSfenPositionTracer
     * @return 日本語表記
     */
    static QString usiMoveToJapanese(const QString &usiMove, int plyNumber,
                                     SfenPositionTracer &tracer);

    /**
     * @brief 駒の日本語名を取得
     */
    static QString pieceToKanji(QChar pieceChar, bool promoted = false);

    // --- データ操作 ---

    /**
     * @brief 定跡手の追加を実行する
     * @param normalizedSfen 正規化SFEN
     * @param currentSfen 現在のSFEN（手数付き）
     * @param newMove 追加する定跡手
     * @return true=追加成功, false=同一指し手が既に存在しキャンセルされた
     *
     * 同じ指し手が既に存在する場合は既存を削除してから追加する。
     * 呼び出し側で重複確認UIを行い、キャンセル時はこのメソッドを呼ばない。
     */
    void addMove(const QString &normalizedSfen, const QString &currentSfen,
                 const JosekiMove &newMove);

    /**
     * @brief 定跡手の編集を実行する
     */
    void editMove(const QString &normalizedSfen, const QString &usiMove,
                  int value, int depth, int frequency, const QString &comment);

    /**
     * @brief 定跡手の削除を実行する
     */
    void deleteMove(const QString &normalizedSfen, int index);

    /**
     * @brief マージ登録を実行し、必要なら自動保存する
     * @param normalizedSfen 正規化SFEN
     * @param sfenWithPly 手数付きSFEN
     * @param usiMove USI形式の指し手
     * @param currentFilePath 現在のファイルパス（自動保存用）
     * @return 自動保存に成功した場合 true
     */
    bool registerMergeMove(const QString &normalizedSfen, const QString &sfenWithPly,
                           const QString &usiMove, const QString &currentFilePath);

    /**
     * @brief 棋譜データからマージエントリを作成する
     * @param sfenList 各手番のSFEN文字列リスト
     * @param moveList 各手のUSI形式指し手リスト
     * @param japaneseMoveList 各手の日本語表記リスト
     * @param currentPly 現在選択中の手数
     * @return マージエントリのリスト
     */
    QVector<KifuMergeEntry> buildMergeEntries(const QStringList &sfenList,
                                               const QStringList &moveList,
                                               const QStringList &japaneseMoveList,
                                               int currentPly) const;

    /**
     * @brief 棋譜ファイルからマージエントリを作成する
     * @param kifFilePath 棋譜ファイルパス
     * @param entries 出力: マージエントリのリスト
     * @param errorMessage 出力: エラーメッセージ
     * @return 成功時 true
     */
    bool buildMergeEntriesFromKifFile(const QString &kifFilePath,
                                      QVector<KifuMergeEntry> &entries,
                                      QString *errorMessage) const;

    /**
     * @brief 指定局面の指し手にUSI指し手が既に含まれているか調べる
     * @param normalizedSfen 正規化SFEN
     * @param usiMove USI形式の指し手
     * @return true=既に存在する
     */
    bool hasDuplicateMove(const QString &normalizedSfen, const QString &usiMove) const;

    /** @brief リポジトリへのアクセス */
    JosekiRepository *repository() const { return m_repository; }

signals:
    /**
     * @brief データが変更されたことを通知する
     *
     * JosekiWindow はこのシグナルを受けて表示を更新する。
     */
    void dataChanged();

    /**
     * @brief 修正状態が変更されたことを通知する
     * @param modified true=変更あり
     */
    void modifiedChanged(bool modified);

private:
    JosekiRepository *m_repository;
};

#endif // JOSEKIPRESENTER_H
