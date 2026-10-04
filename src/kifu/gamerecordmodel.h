#ifndef GAMERECORDMODEL_H
#define GAMERECORDMODEL_H

/// @file gamerecordmodel.h
/// @brief 棋譜データの中央管理クラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>

#include "kifdisplayitem.h"
#include "kifuexportmetadata.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"
#include "playmode.h"

class KifuNavigationState;
class KifuBranchNode;

/**
 * @brief 棋譜データの中央管理クラス
 *
 * 棋譜のコメント編集、保存、読み込みを一元管理し、
 * データの整合性を保証する。
 *
 * 設計方針:
 * - コメント・しおりは分岐ツリーの各ノードを参照し、このクラス経由で同期更新する
 * - 既存データへの参照を保持し、必要に応じて同期更新を行う
 * - KIF/CSA/JKF/USEN/USI形式への出力機能を提供
 *
 */
class GameRecordModel : public QObject
{
    Q_OBJECT

public:
    explicit GameRecordModel(QObject* parent = nullptr);
    ~GameRecordModel() override = default;

    // --- 初期化・バインド ---

    /**
     * @brief 既存のデータストアをバインド（参照を保持、所有しない）
     */
    void bind(QList<KifDisplayItem>* liveDisp);

    /**
     * @brief KifuBranchTree を設定（新システム用）
     */
    void setBranchTree(KifuBranchTree* tree);

    /**
     * @brief KifuNavigationState を設定（新システム用、現在のライン取得に使用）
     */
    void setNavigationState(KifuNavigationState* state);

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

    // --- コメント操作（Single Source of Truth） ---

    /**
     * @brief 指定手数のコメントを設定
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @param comment 新しいコメント
     *
     * この関数は以下のすべてを同期更新します:
     * - KifuBranchTree のノード（ツリーがない場合のみ内部配列）
     * - 本譜に対応する liveDisp[ply].comment
     */
    void setComment(int ply, const QString& comment);

    /**
     * @brief 指定手数のコメントを取得
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @return コメント文字列（なければ空文字）
     */
    QString comment(int ply) const;

    // --- しおり操作 ---

    /**
     * @brief 指定手数のしおりを設定
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @param bookmark 新しいしおり
     */
    void setBookmark(int ply, const QString& bookmark);

    /**
     * @brief 指定手数のしおりを取得
     * @param ply 手数（0=開始局面, 1=1手目, ...）
     * @return しおり文字列（なければ空文字）
     */
    QString bookmark(int ply) const;

    // --- 棋譜出力 ---

    /**
     * @brief 出力に必要なコンテキスト情報
     */
    using ExportContext = KifuExportMetadata;

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

    /**
     * @brief USEN形式（Url Safe sfen-Extended Notation）の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @param usiMoves USI形式の指し手リスト（本譜用）
     * @return USEN形式の行リスト（1要素のUSEN文字列）
     * 
     * USEN形式はURLセーフな短い文字列で棋譜を表現するフォーマットです。
     * 分岐の指し手にも対応しています。
     * https://www.slideshare.net/slideshow/scalajs-web/92707205#15
     */
    QStringList toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const;

    /**
     * @brief USIプロトコル形式（position コマンド文字列）の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @param usiMoves USI形式の指し手リスト（本譜用）
     * @return USI形式の行リスト（1要素のposition コマンド文字列）
     * 
     * USI形式は将棋GUIとエンジン間の通信プロトコルで使用される棋譜フォーマットです。
     * 分岐の指し手には対応していません（本譜のみ出力）。
     * https://shogidokoro2.stars.ne.jp/usi.html
     */
    QStringList toUsiLines(const ExportContext& ctx, const QStringList& usiMoves) const;

    // --- ライブ対局用 ---

    // --- 状態取得 ---

    /**
     * @brief 変更があったか（未保存の変更があるか）
     */
    bool isDirty() const { return m_isDirty; }

    /**
     * @brief 変更フラグをクリア（保存後に呼ぶ）
     */
    void clearDirty() { m_isDirty = false; }

    /// 棋譜の追記や取り込みを未保存として記録する。
    void markDirty() { m_isDirty = true; }

    /**
     * @brief アクティブ行のインデックス
     */
    int activeRow() const;

    // --- データアクセス（エクスポータ向け） ---

    /**
     * @brief KifuBranchTree への読み取り専用アクセス
     */
    KifuBranchTree* branchTree() const { return m_branchTree; }

    /**
     * @brief 本譜の表示データを収集（エクスポート用）
     */
    QList<KifDisplayItem> collectMainlineForExport() const;

    /// 本譜ツリーの局面列からUSI指し手を収集する（終局手は含めない）。
    QStringList collectMainlineUsiForExport() const;

    /// 本譜の開始SFEN。ツリーがない場合は指定されたSFENを使用する。
    QString initialSfenForExport(const QString& fallback) const;

    /**
     * @brief 出力コンテキストからヘッダ情報を収集
     */
    static QList<KifGameInfoItem> collectGameInfo(const ExportContext& ctx);

    /**
     * @brief 出力コンテキストから対局者名を解決
     */
    static void resolvePlayerNames(const ExportContext& ctx, QString& outBlack, QString& outWhite);

signals:
    /**
     * @brief コメントが変更された
     * @param ply 変更された手数
     * @param newComment 新しいコメント
     */
    void commentChanged(int ply, const QString& newComment);

    void bookmarkChanged(int ply, const QString& bookmark);
private:
    // === ツリーがない場合のフォールバックデータ ===
    QList<QString> m_comments;   ///< 手数インデックス → コメント
    QList<QString> m_bookmarks;  ///< 手数インデックス → しおり
    bool m_isDirty = false;        ///< 変更フラグ

    // === 外部データへの参照（同期更新用、所有しない） ===
    QList<KifDisplayItem>* m_liveDisp = nullptr;

    // === 新システム（KifuBranchTree）への参照 ===
    KifuBranchTree* m_branchTree = nullptr;
    KifuNavigationState* m_navState = nullptr;

    // === 内部ヘルパ ===
    KifuBranchNode* nodeForCurrentLine(int ply) const;
    bool hasBranchTree() const;
    bool isMainlineNode(int ply, const KifuBranchNode* node) const;

};

#endif // GAMERECORDMODEL_H
