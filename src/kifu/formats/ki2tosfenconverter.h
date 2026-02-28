#ifndef KI2TOSFENCONVERTER_H
#define KI2TOSFENCONVERTER_H

/// @file ki2tosfenconverter.h
/// @brief KI2形式棋譜コンバータクラスの定義

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include "ki2lexer.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "shogimove.h"

/**
 * @brief KI2形式の棋譜を解析し、USI形式やGUI用データに変換するクラス
 *
 * KI2形式はKIF形式と異なり、移動元座標が省略されているため、
 * 盤面状態を追跡しながら移動元を推測する必要がある。
 * 字句解析・曖昧指し手解決は Ki2Lexer に委譲する。
 */
class Ki2ToSfenConverter
{
public:
    static QString detectInitialSfenFromFile(const QString& ki2Path, QString* detectedLabel = nullptr);
    static QStringList convertFile(const QString& ki2Path, QString* errorMessage = nullptr);
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& ki2Path, QString* errorMessage = nullptr);
    [[nodiscard]] static bool parseWithVariations(const QString& ki2Path, KifParseResult& out, QString* errorMessage = nullptr);
    static QString mapHandicapToSfen(const QString& label);
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);
    [[nodiscard]] static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);

    // 盤面操作API（gamerecordmodel等から使用）
    static void initBoardFromSfen(const QString& sfen,
                                   QString boardState[9][9],
                                   QMap<Piece, int>& blackHands,
                                   QMap<Piece, int>& whiteHands);
    static void applyMoveToBoard(const QString& usi,
                                  QString boardState[9][9],
                                  QMap<Piece, int>& blackHands,
                                  QMap<Piece, int>& whiteHands,
                                  bool blackToMove);

    /// KIF形式の prettyMove を KI2形式に変換（修飾子自動生成、盤面更新）
    static QString convertPrettyMoveToKi2(
        const QString& prettyMove,
        QString boardState[9][9],
        QMap<Piece, int>& blackHands,
        QMap<Piece, int>& whiteHands,
        bool blackToMove,
        int& prevToFile, int& prevToRank);

private:
    static QString generateModifier(
        const QVector<Ki2Lexer::Candidate>& candidates,
        int srcFile, int srcRank,
        int dstFile, int dstRank,
        bool blackToMove);
};

#endif // KI2TOSFENCONVERTER_H
