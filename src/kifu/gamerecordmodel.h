#ifndef GAMERECORDMODEL_H
#define GAMERECORDMODEL_H

/// @file gamerecordmodel.h
/// @brief 棋譜データの中央管理クラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>

#include "recordline.h"
#include "kifuexportmetadata.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"

class KifuBranchNode;

/**
 * @brief 棋譜（分岐ツリー）の編集・未保存の印・書き出しの窓口
 *
 * コメント・しおりは分岐ツリーのノードに書き、書いたノードを commentChanged / bookmarkChanged で知らせる。
 * 棋譜の追記・取り込みは markDirty() で未保存として記録する。書き出しは形式ごとの Exporter に任せる。
 */
class GameRecordModel : public QObject
{
    Q_OBJECT

public:
    explicit GameRecordModel(QObject* parent = nullptr);
    ~GameRecordModel() override = default;

    /// 書き出す棋譜（分岐ツリー）を設定する
    void setBranchTree(KifuBranchTree* tree);

    // --- コメント・しおり ---

    /// node のコメントを書き換える（変わったら未保存にして commentChanged を出す）
    void setComment(KifuBranchNode* node, const QString& comment);
    /// node のしおりを書き換える（変わったら未保存にして bookmarkChanged を出す）
    void setBookmark(KifuBranchNode* node, const QString& bookmark);

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
     * @return CSA形式の行リスト
     * 
     * CSA形式はコンピュータ将棋協会の標準棋譜ファイル形式です。
     * 分岐の指し手には対応していません（本譜のみ出力）。
     */
    QStringList toCsaLines(const ExportContext& ctx) const;

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
     * @return USEN形式の行リスト（1要素のUSEN文字列）
     * 
     * USEN形式はURLセーフな短い文字列で棋譜を表現するフォーマットです。
     * 分岐の指し手にも対応しています。
     * https://www.slideshare.net/slideshow/scalajs-web/92707205#15
     */
    QStringList toUsenLines(const ExportContext& ctx) const;

    /**
     * @brief USIプロトコル形式（position コマンド文字列）の行リストを生成
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return USI形式の行リスト（1要素のposition コマンド文字列）
     * 
     * USI形式は将棋GUIとエンジン間の通信プロトコルで使用される棋譜フォーマットです。
     * 分岐の指し手には対応していません（本譜のみ出力）。
     * https://shogidokoro2.stars.ne.jp/usi.html
     */
    QStringList toUsiLines(const ExportContext& ctx) const;

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

    // --- データアクセス（エクスポータ向け） ---

    /**
     * @brief KifuBranchTree への読み取り専用アクセス
     */
    KifuBranchTree* branchTree() const { return m_branchTree; }

    /**
     * @brief 本譜の表示データを収集（エクスポート用）
     */
    RecordLine collectMainlineForExport() const;

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
    /// ノードのコメントが変わった
    void commentChanged(int nodeId, const QString& comment);
    /// ノードのしおりが変わった
    void bookmarkChanged(int nodeId, const QString& bookmark);

private:
    bool m_isDirty = false;                     ///< 未保存の変更があるか
    KifuBranchTree* m_branchTree = nullptr;     ///< 棋譜の正本（非所有）
};

#endif // GAMERECORDMODEL_H
