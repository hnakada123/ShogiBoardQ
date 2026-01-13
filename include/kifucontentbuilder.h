#ifndef KIFUCONTENTBUILDER_H
#define KIFUCONTENTBUILDER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include "kiftosfenconverter.h"
#include "playmode.h"
#include "kifdisplayitem.h"
#include "kifutypes.h" // ResolvedRow用

class QTableWidget;
class KifuRecordListModel;

// 保存に必要な情報をまとめた構造体
struct KifuExportContext {
    // UI/Model参照（読み取り専用）
    const QTableWidget* gameInfoTable = nullptr;
    const KifuRecordListModel* recordModel = nullptr;
    const QVector<ResolvedRow>* resolvedRows = nullptr;
    const QList<KifDisplayItem>* liveDisp = nullptr;

    // ★ 追加: 編集されたコメント配列（MainWindow::m_commentsByRow）
    const QVector<QString>* commentsByRow = nullptr;

    // アクティブな行インデックス（分岐選択時に使用）
    int activeResolvedRow = 0;

    // ゲーム状態
    QString startSfen;
    PlayMode playMode = NotStarted;
    QString human1;
    QString human2;
    QString engine1;
    QString engine2;
};

class KifuContentBuilder
{
public:
    // メインの構築関数
    static QStringList buildKifuDataList(const KifuExportContext& ctx);

    /**
     * @brief コメント文字列をリッチHTMLに変換する
     *
     * 以下の変換を行う:
     * 1. '*' の直前で改行（行頭以外の場合）
     * 2. URL（http(s)://...）を <a href="...">...</a> にリンク化
     * 3. 改行を <br/> に変換（非URL部分はHTMLエスケープ）
     *
     * @param raw 元のコメント文字列
     * @return リッチHTML文字列
     */
    static QString toRichHtmlWithStarBreaksAndLinks(const QString& raw);

private:
    // 内部ヘルパ
    static QList<KifGameInfoItem> collectGameInfo(const KifuExportContext& ctx);
    static QList<KifDisplayItem> collectMainline(const KifuExportContext& ctx);
    static void resolvePlayerNames(const KifuExportContext& ctx, QString& outBlack, QString& outWhite);
};

#endif // KIFUCONTENTBUILDER_H
