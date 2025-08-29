#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

#include <QString>
#include <QStringList>
#include <QPoint>

/**
 * @brief KIF の指し手行を USI/SFEN 文字列へ変換する簡易コンバータ。
 *
 * 取り扱い仕様（簡潔版）
 * - 対応: 「７六歩(77)」「同　銀(42)」「８八角成(99)」「４五歩打」
 * - 打ち: 「打」を検出し USI では "P*5e" 形式にする
 * - 成り: 行に「不成」が無く「成」を含む → 末尾に '+'
 * - 「同」: 直前手の着手地点を再利用
 * - 無視: ヘッダ/コメント（先手：、手数＝、* で始まる等）、投了/中断など
 * - 前提: KIF 行に移動元 "(77)" のような表記が含まれていること（打ちは除く）
 */
class KifToSfenConverter
{
public:
    KifToSfenConverter();

    /**
     * @brief KIFファイルを読み込んで、各指し手を USI/SFEN の QStringList で返す。
     *        解析できなかった行はスキップ（errorMessage に詳細を追記）。
     * @param kifPath KIFファイルパス
     * @param errorMessage 失敗やスキップの詳細（任意）
     * @return USI/SFEN 指し手列（例: {"7g7f","3c3d","P*5e",...}）
     */
    QStringList convertFile(const QString& kifPath, QString* errorMessage = nullptr);

    /**
     * @brief KIFの1行から USI/SFEN の1手を得る。
     * @param line KIFの指し手行
     * @param usiMove 変換された1手（成功時のみ書き込み）
     * @return 変換に成功したら true
     */
    bool convertMoveLine(const QString& line, QString& usiMove);

    // KIF 内の「手合割」を見て初期SFENを返す（見つからない/未対応は平手）
    // detectedLabel には見つけた手合名（例: "平手", "香落ち" など）を返す
    static QString detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel = nullptr);

private:
    // --- 文字/数値の変換ユーティリティ ---
    static int zenkakuDigitToInt(QChar ch); // '１'..'９' → 1..9（それ以外は0）
    static int kanjiDigitToInt(QChar ch);   // '一'..'九' → 1..9（それ以外は0）
    static int asciiDigitToInt(QChar ch);   // '1'..'9'   → 1..9（それ以外は0）
    static QChar rankNumToLetter(int rank); // 1→'a' … 9→'i'

    // "(77)" や "（77）" を抽出（全角括弧対応、内側は半角/全角数字OK）
    static bool parseOrigin(const QString& src, int& fromFile, int& fromRank);

    // 行から着手先 (file, rank) を抽出する。「同」は isSameAsPrev=true にする
    static bool findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev);

    // 「打ち」で使う駒種の USI ドロップ用一文字（P/L/N/S/G/B/R/K）を返す
    static QChar pieceKanaToUsiDropLetter(const QString& line);

    // 行が指し手として無効/無視対象か
    static bool isSkippableLine(const QString& line);

    // 直前手の着手先（「同」用）: x=file(1..9), y=rank(1..9)。未設定は (-1,-1)
    QPoint m_lastDest;
};

#endif // KIFTOSFENCONVERTER_H
