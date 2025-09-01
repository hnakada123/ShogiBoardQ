#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>

#include "kifreader.h"

// 表示用1手（指し手/時間/コメント）
struct KifDisplayItem {
    QString prettyMove;   // "▲２六歩(27)" / "△投了" など
    QString timeText;     // "0:00/00:00:00" など（空可）
    QString comment;      // 直前の * コメントをまとめて格納（空可）
};

// 1本の手順（本譜 or 変化）
struct KifLine {
    QString baseSfen;             // 初期局面（本譜はdetectInitialSfen、変化はGUI側で設定推奨）
    QStringList usiMoves;         // USI手列（"7g7f" / "P*5e" など）
    QList<KifDisplayItem> disp;   // 表示用（指し手+時間+コメント）
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
