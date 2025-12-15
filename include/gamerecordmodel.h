#ifndef GAMERECORDMODEL_H
#define GAMERECORDMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifutypes.h"
#include "playmode.h"

class QTableWidget;
class KifuRecordListModel;

/**
 * @brief 棋譜データの中央管理クラス
 * 
 * 棋譜のコメント編集、保存、読み込みを一元管理し、
 * データの整合性を保証します。
 * 
 * 設計方針:
 * - Single Source of Truth: コメントデータはこのクラスが権威を持つ
 * - 既存データへの参照を保持し、必要に応じて同期更新を行う
 * - KIF/CSA/JKF形式への出力機能を提供
 */
class GameRecordModel : public QObject
{
    Q_OBJECT

public:
    explicit GameRecordModel(QObject* parent = nullptr);
    ~GameRecordModel() override = default;

    // ========================================
    // 初期化・バインド
    // ========================================

    /**
     * @brief 既存のデータストアをバインド（参照を保持、所有しない）
     */
    void bind(QVector<ResolvedRow>* resolvedRows,
              int* activeResolvedRow,
              QList<KifDisplayItem>* liveDisp);

    /**
     * @brief 棋譜読み込み時の初期化
     * @param disp 読み込んだ棋譜の表示データ
     * @param rowCount SFEN記録の行数（コメント配列のサイズ決定に使用）
     */
    void initializeFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount);

    /**
     * @brief 新規対局開始時のクリア
     */
    void clear();

    // ========================================
    // コメント操作（Single Source of Truth）
    // ========================================

    /**
     * @brief 指定手数のコメントを設定
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @param comment 新しいコメント
     * 
     * この関数は以下のすべてを同期更新します:
     * - 内部コメント配列 (m_comments)
     * - ResolvedRow::comments
     * - ResolvedRow::disp[ply].comment
     * - liveDisp[ply].comment
     */
    void setComment(int ply, const QString& comment);

    /**
     * @brief 指定手数のコメントを取得
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @return コメント文字列（なければ空文字）
     */
    QString comment(int ply) const;

    /**
     * @brief コメント配列全体を取得
     */
    const QVector<QString>& comments() const { return m_comments; }

    /**
     * @brief コメント配列のサイズ
     */
    int commentCount() const { return m_comments.size(); }

    // ========================================
    // 棋譜出力
    // ========================================

    /**
     * @brief 出力に必要なコンテキスト情報
     */
    struct ExportContext {
        const QTableWidget* gameInfoTable = nullptr;
        const KifuRecordListModel* recordModel = nullptr;
        QString startSfen;
        PlayMode playMode = NotStarted;
        QString human1;
        QString human2;
        QString engine1;
        QString engine2;
    };

    /**
     * @brief KIF形式の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return KIF形式の行リスト
     */
    QStringList toKifLines(const ExportContext& ctx) const;

    /**
     * @brief KI2形式の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return KI2形式の行リスト
     * 
     * KI2形式は消費時間を含まず、指し手は手番記号（▲△）付きで出力されます。
     * 分岐の指し手もサポートされます。
     */
    QStringList toKi2Lines(const ExportContext& ctx) const;

    /**
     * @brief CSA形式の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @param usiMoves USI形式の指し手リスト
     * @return CSA形式の行リスト
     * 
     * CSA形式はコンピュータ将棋協会の標準棋譜ファイル形式です。
     * 分岐の指し手には対応していません（本譜のみ出力）。
     */
    QStringList toCsaLines(const ExportContext& ctx, const QStringList& usiMoves) const;

    /**
     * @brief JKF形式（JSON棋譜フォーマット）の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return JKF形式の行リスト（1要素のJSONテキスト）
     * 
     * JKF形式はJSON形式の棋譜フォーマットです。
     * 分岐の指し手にも対応しています。
     * https://github.com/na2hiro/Kifu-for-JS/tree/master/packages/json-kifu-format
     */
    QStringList toJkfLines(const ExportContext& ctx) const;

    // ========================================
    // ライブ対局用
    // ========================================

    /**
     * @brief ライブ対局で1手追加時にコメント配列を拡張
     * @param ply 追加された手数
     */
    void ensureCommentCapacity(int ply);

    // ========================================
    // 状態取得
    // ========================================

    /**
     * @brief 変更があったか（未保存の変更があるか）
     */
    bool isDirty() const { return m_isDirty; }

    /**
     * @brief 変更フラグをクリア（保存後に呼ぶ）
     */
    void clearDirty() { m_isDirty = false; }

    /**
     * @brief アクティブ行のインデックス
     */
    int activeRow() const;

signals:
    /**
     * @brief コメントが変更された
     * @param ply 変更された手数
     * @param newComment 新しいコメント
     */
    void commentChanged(int ply, const QString& newComment);

    /**
     * @brief データが変更された（保存が必要）
     */
    void dataChanged();

private:
    // === 内部データ（権威を持つ） ===
    QVector<QString> m_comments;  ///< 手数インデックス → コメント
    bool m_isDirty = false;       ///< 変更フラグ

    // === 外部データへの参照（同期更新用、所有しない） ===
    QVector<ResolvedRow>* m_resolvedRows = nullptr;
    int* m_activeResolvedRow = nullptr;
    QList<KifDisplayItem>* m_liveDisp = nullptr;

    // === 内部ヘルパ ===
    void syncToExternalStores_(int ply, const QString& comment);
    QList<KifDisplayItem> collectMainlineForExport_() const;
    static QList<KifGameInfoItem> collectGameInfo_(const ExportContext& ctx);
    static void resolvePlayerNames_(const ExportContext& ctx, QString& outBlack, QString& outWhite);

    // === 分岐出力用ヘルパ ===
    /**
     * @brief 分岐が存在する手数のセットを収集
     * @return 分岐開始手数のセット（本譜基準）
     */
    QSet<int> collectBranchPoints_() const;

    /**
     * @brief 指定した行の変化をKIF形式で出力
     * @param rowIndex ResolvedRow のインデックス
     * @param out 出力先
     */
    void outputVariation_(int rowIndex, QStringList& out) const;

    /**
     * @brief 再帰的に変化を出力（子の変化を含む）
     * @param parentRowIndex 親行のインデックス
     * @param out 出力先
     * @param visitedRows 訪問済み行のセット（無限ループ防止）
     */
    void outputVariationsRecursively_(int parentRowIndex, QStringList& out, QSet<int>& visitedRows) const;

    // === KI2形式出力用ヘルパ ===
    /**
     * @brief KI2形式で指定した行の変化を出力
     * @param rowIndex ResolvedRow のインデックス
     * @param out 出力先
     */
    void outputKi2Variation_(int rowIndex, QStringList& out) const;

    /**
     * @brief KI2形式で再帰的に変化を出力
     * @param parentRowIndex 親行のインデックス
     * @param out 出力先
     * @param visitedRows 訪問済み行のセット（無限ループ防止）
     */
    void outputKi2VariationsRecursively_(int parentRowIndex, QStringList& out, QSet<int>& visitedRows) const;

    // === JKF形式出力用ヘルパ ===
    /**
     * @brief JKF形式のヘッダ部分を構築
     * @param ctx 出力コンテキスト
     * @return QJsonObject 形式のヘッダ
     */
    QJsonObject buildJkfHeader_(const ExportContext& ctx) const;

    /**
     * @brief JKF形式の初期局面部分を構築
     * @param ctx 出力コンテキスト
     * @return QJsonObject 形式の初期局面
     */
    QJsonObject buildJkfInitial_(const ExportContext& ctx) const;

    /**
     * @brief 指し手をJKF形式の move オブジェクトに変換
     * @param disp 表示用指し手データ
     * @param prevToX 前回の移動先X座標（"同〜"判定用）
     * @param prevToY 前回の移動先Y座標（"同〜"判定用）
     * @param ply 手数
     * @return QJsonObject 形式の指し手
     */
    QJsonObject convertMoveToJkf_(const KifDisplayItem& disp, int& prevToX, int& prevToY, int ply) const;

    /**
     * @brief JKF形式の moves 配列を構築（本譜）
     * @param disp 表示用指し手データリスト
     * @return QJsonArray 形式の指し手配列
     */
    QJsonArray buildJkfMoves_(const QList<KifDisplayItem>& disp) const;

    /**
     * @brief JKF形式の moves 配列に分岐を追加
     * @param movesArray 本譜の指し手配列
     * @param mainRowIndex 本譜の行インデックス
     */
    void addJkfForks_(QJsonArray& movesArray, int mainRowIndex) const;
};

#endif // GAMERECORDMODEL_H
