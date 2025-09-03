#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QPair>
#include <QMap>
#include "shogimove.h"
#include "kifdisplayitem.h"

// KIFの「(キーワード)：(内容)」行を表すペア
struct KifGameInfoItem {
    QString key;    // 例: "開始日時"
    QString value;  // 例: "2025/03/02 09:00"
};

// ==== 分岐用データ構造 ====
// 1本の手順（分岐ライン）
struct KifLine {
    int startPly = 1;            // どの手目から開始か（例: 5）
    QString baseSfen;            // 分岐開始局面（メインを startPly-1 まで進めたSFEN）
    QStringList usiMoves;        // 分岐のUSI列
    QList<KifDisplayItem> disp;  // 表示用（▲△＋時間）: 先頭=分岐の最初の手
    QStringList sfenList;        // base含む局面列（0=base, 以降=各手後）
    QVector<ShogiMove> gameMoves;// GUI用指し手列
    bool endsWithTerminal=false;
};
// 変化（どの手から始まるか）
struct KifVariation {
    int startPly = 1;  // 例: 「変化：65手」なら 65
    KifLine line;
};

// 解析結果（本譜＋変化）
struct KifParseResult {
    KifLine mainline;
    QVector<KifVariation> variations;
};

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
    static int  asciiDigitToInt(QChar c);
    static int  zenkakuDigitToInt(QChar c);
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
};

#endif // KIFTOSFENCONVERTER_H
