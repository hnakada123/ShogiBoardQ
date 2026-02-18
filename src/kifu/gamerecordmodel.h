#ifndef GAMERECORDMODEL_H
#define GAMERECORDMODEL_H

/// @file gamerecordmodel.h
/// @brief 棋譜データの中央管理クラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QDateTime>
#include <functional>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifutypes.h"
#include "kifubranchtree.h"
#include "playmode.h"

class QTableWidget;
class KifuRecordListModel;
class KifuNavigationState;

/**
 * @brief 棋譜データの中央管理クラス
 *
 * 棋譜のコメント編集、保存、読み込みを一元管理し、
 * データの整合性を保証する。
 *
 * 設計方針:
 * - Single Source of Truth: コメントデータはこのクラスが権威を持つ
 * - 既存データへの参照を保持し、必要に応じて同期更新を行う
 * - KIF/CSA/JKF/USEN/USI形式への出力機能を提供
 *
 * @todo remove コメントスタイルガイド適用済み
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
     * - 内部コメント配列 (m_comments)
     * - KifuBranchTree のノードコメント
     * - liveDisp[ply].comment
     */
    void setComment(int ply, const QString& comment);

    /**
     * @brief コメント更新時の外部通知コールバック型
     * @param ply 更新された手数
     * @param comment 新しいコメント
     */
    using CommentUpdateCallback = std::function<void(int ply, const QString& comment)>;

    /**
     * @brief コメント更新時の通知コールバックを設定
     * @param callback コールバック関数
     *
     * このコールバックは setComment でコメントが変更された後に呼ばれます。
     * RecordPresenterへの通知やUI更新に使用します。
     */
    void setCommentUpdateCallback(const CommentUpdateCallback& callback);

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
    int commentCount() const { return static_cast<int>(m_comments.size()); }

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

    /**
     * @brief しおり更新時の外部通知コールバック型
     */
    using BookmarkUpdateCallback = std::function<void(int ply, const QString& bookmark)>;

    /**
     * @brief しおり更新時の通知コールバックを設定
     */
    void setBookmarkUpdateCallback(const BookmarkUpdateCallback& callback);

    /**
     * @brief しおり配列の容量を確保
     */
    void ensureBookmarkCapacity(int ply);

    // --- 棋譜出力 ---

    /**
     * @brief 出力に必要なコンテキスト情報
     */
    struct ExportContext {
        const QTableWidget* gameInfoTable = nullptr;
        const KifuRecordListModel* recordModel = nullptr;
        QString startSfen;
        PlayMode playMode = PlayMode::NotStarted;
        QString human1;
        QString human2;
        QString engine1;
        QString engine2;
        
        // --- 時間制御情報（CSA出力用） ---
        bool hasTimeControl = false;           ///< 時間制御が有効かどうか
        int initialTimeMs = 0;                 ///< 初期持ち時間（ミリ秒）
        int byoyomiMs = 0;                     ///< 秒読み（ミリ秒）
        int fischerIncrementMs = 0;            ///< フィッシャー加算（ミリ秒）
        QDateTime gameStartDateTime;           ///< 対局開始日時
        QDateTime gameEndDateTime;             ///< 対局終了日時
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

    /**
     * @brief ライブ対局で1手追加時にコメント配列を拡張
     * @param ply 追加された手数
     */
    void ensureCommentCapacity(int ply);

    // --- 状態取得 ---

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

    // --- データアクセス（エクスポータ向け） ---

    /**
     * @brief KifuBranchTree への読み取り専用アクセス
     */
    KifuBranchTree* branchTree() const { return m_branchTree; }

    /**
     * @brief 本譜の表示データを収集（エクスポート用）
     */
    QList<KifDisplayItem> collectMainlineForExport() const;

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

    /**
     * @brief しおりが変更された
     * @param ply 変更された手数
     * @param newBookmark 新しいしおり
     */
    void bookmarkChanged(int ply, const QString& newBookmark);

    /**
     * @brief データが変更された（保存が必要）
     */
    void dataChanged();

private:
    // === 内部データ（権威を持つ） ===
    QVector<QString> m_comments;   ///< 手数インデックス → コメント
    QVector<QString> m_bookmarks;  ///< 手数インデックス → しおり
    bool m_isDirty = false;        ///< 変更フラグ

    // === 外部データへの参照（同期更新用、所有しない） ===
    QList<KifDisplayItem>* m_liveDisp = nullptr;

    // === 新システム（KifuBranchTree）への参照 ===
    KifuBranchTree* m_branchTree = nullptr;
    KifuNavigationState* m_navState = nullptr;

    // === コールバック ===
    CommentUpdateCallback m_commentUpdateCallback;    ///< コメント更新時の通知コールバック
    BookmarkUpdateCallback m_bookmarkUpdateCallback;  ///< しおり更新時の通知コールバック

    // === 内部ヘルパ ===
    void syncToExternalStores(int ply, const QString& comment);

    // === 分岐出力用ヘルパ ===
    /**
     * @brief BranchLineからKIF形式で変化を出力
     * @param line 分岐ライン
     * @param out 出力先
     */
    void outputVariationFromBranchLine(const BranchLine& line, QStringList& out) const;

    // === KI2形式出力用ヘルパ ===
    /**
     * @brief BranchLineからKI2形式で変化を出力
     * @param line 分岐ライン
     * @param out 出力先
     */
    void outputKi2VariationFromBranchLine(const BranchLine& line, QStringList& out) const;

    // === USEN形式出力用ヘルパ ===
    /**
     * @brief SFENをUSEN形式に変換
     * @param sfen SFEN形式の局面文字列
     * @return USEN形式の局面文字列（平手の場合は空文字列）
     */
    static QString sfenToUsenPosition(const QString& sfen);

    /**
     * @brief USI形式の指し手をUSEN形式にエンコード
     * @param usiMove USI形式の指し手（例: "7g7f", "P*5e", "2h3g+"）
     * @return USEN形式（3文字のbase36）
     */
    static QString encodeUsiMoveToUsen(const QString& usiMove);

    /**
     * @brief 指し手番号をbase36（3文字）にエンコード
     * @param moveCode 指し手コード（0-46655）
     * @return 3文字のbase36文字列
     */
    static QString intToBase36(int moveCode);

    /**
     * @brief 終局語からUSEN終局コードを取得
     * @param terminalMove 終局語（例: "▲投了"）
     * @return USENの終局コード（例: "r"）、該当なしは空文字列
     */
    static QString getUsenTerminalCode(const QString& terminalMove);

    /**
     * @brief 2つのSFEN間の差分からUSI形式の指し手を推測
     * @param sfenBefore 指し手前のSFEN
     * @param sfenAfter 指し手後のSFEN
     * @param isSente 先手番かどうか
     * @return USI形式の指し手（推測できない場合は空文字列）
     */
    static QString inferUsiFromSfenDiff(const QString& sfenBefore, const QString& sfenAfter, bool isSente);

    // === USI（position コマンド）形式出力用ヘルパ ===
    /**
     * @brief 終局語からUSI終局コードを取得
     * @param terminalMove 終局語（例: "▲投了"、"千日手"）
     * @return USIの終局コード（例: "resign", "rep_draw"）、該当なしは空文字列
     */
    static QString getUsiTerminalCode(const QString& terminalMove);
};

#endif // GAMERECORDMODEL_H
