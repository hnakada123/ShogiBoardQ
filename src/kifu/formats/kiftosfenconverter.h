#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

/// @file kiftosfenconverter.h
/// @brief KIF形式棋譜コンバータクラスの定義


#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QPair>
#include <QMap>
#include "shogimove.h"
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
    static bool parseWithVariations(const QString& kifPath, KifParseResult& out, QString* errorMessage = nullptr);

    // 手合→初期SFEN（ユーザー提供のマップ）
    static QString mapHandicapToSfen(const QString& label);

    // 追加: KIFファイルから「対局情報」を抽出して順序付きで返す
    //   - ファイル全体を走査して「： or :」の区切りを持つ行を収集
    //   - 同一キーが複数回出る場合は値を改行で連結（備考など）
    //   - 値中の「\n」文字列は実改行に変換
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // 追加（任意）：キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);

private:
    // ---------- ヘルパ（共通） ----------
    static bool isSkippableLine(const QString& line);
    static bool isBoardHeaderOrFrame(const QString& line);
    static bool isBodHeader(const QString& line);
    static bool containsAnyTerminal(const QString& s, QString* matched = nullptr);
    static int  kanjiDigitToInt(QChar c);
    static QChar rankNumToLetter(int r); // 1..9 -> 'a'..'i'

    // 目的地（「同」対応）を読む
    static bool findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev);

    // 1行の指し手をUSIに変換（prevTo参照；成功で更新）
    static bool convertMoveLine(const QString& line,
                                QString& usi,
                                int& prevToFile, int& prevToRank);

    // 「打」用：駒種（漢字）→ USIドロップ文字（'P','L',...）
    static QChar pieceKanjiToUsiUpper(const QString& s);

    // Promoted表記が含まれているか（"成" かつ "不成" でない）
    static bool isPromotionMoveText(const QString& line);

    // 追加API：初手前（開始局面）のコメント／しおりをまとめて取得
    // - 初手より前にある '*' コメント行と '&しおり' を収集し、改行で連結して返す
    // - 取得対象は「最初の指し手行」まで（BODや各種ヘッダは自動スキップ）
    static QString extractOpeningComment(const QString& filePath);

    // ---------- parseWithVariations ヘルパ ----------
    static void extractMainLine(const QString& kifPath, KifParseResult& out, QString* errorMessage);
    static QString findBranchBaseSfen(const QVector<KifVariation>& vars,
                                      const KifLine& mainLine,
                                      int branchPointPly);
    static KifVariation parseVariationBlock(const QStringList& blockLines,
                                            int startPly,
                                            const QString& baseSfen);
    static void extractMovesFromBlock(const QStringList& blockLines,
                                      int startPly,
                                      KifLine& line);
};

#endif // KIFTOSFENCONVERTER_H
