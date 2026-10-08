#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

/// @file kiftosfenconverter.h
/// @brief KIF形式棋譜コンバータクラスの定義


#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

class KifToSfenConverter
{
public:
    // 既存API：本譜の初期SFEN（手合割から判定）
    static QString detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel = nullptr);

    // 既存API：本譜のUSI列のみを抽出（終局/中断で打ち切り）
    static QStringList convertFile(const QString& kifPath, QString* errorMessage = nullptr);

    // 既存API：本譜の「指し手＋時間」（コメントも格納）を抽出
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& kifPath, QString* errorMessage = nullptr);

    // 新API：本譜＋全変化をまとめて抽出（コメントも格納）
    [[nodiscard]] static bool parseWithVariations(const QString& kifPath, KifParseResult& out, QString* errorMessage = nullptr);

    // 手合→初期SFEN（ユーザー提供のマップ）
    static QString mapHandicapToSfen(const QString& label);

    // 追加: KIFファイルから「対局情報」を抽出して順序付きで返す
    //   - ファイル全体を走査して「： or :」の区切りを持つ行を収集
    //   - 同一キーが複数回出る場合は値を改行で連結（備考など）
    //   - 値中の「\n」文字列は実改行に変換
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // 追加（任意）：キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    [[nodiscard]] static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);

private:
    // ---------- parseWithVariations ヘルパ ----------
    static void extractMainLine(const QString& kifPath, KifParseResult& out, QString* errorMessage);
    /// 分岐元の局面と、その局面に至る直前の指し手（「同」の解決用、USI形式）を求める。
    /// @param varPrevUsi vars と同じ並びで、各変化の分岐元に至る直前の指し手
    static QString findBranchBaseSfen(const QList<KifVariation>& vars,
                                      const QStringList& varPrevUsi,
                                      const KifLine& mainLine,
                                      int branchPointPly,
                                      QString* prevUsi = nullptr);
    static KifVariation parseVariationBlock(const QStringList& blockLines,
                                            int startPly,
                                            const QString& baseSfen,
                                            const QString& prevUsi = QString());
    static void extractMovesFromBlock(const QStringList& blockLines,
                                      int startPly,
                                      KifLine& line,
                                      const QString& prevUsi = QString());
};

#endif // KIFTOSFENCONVERTER_H
