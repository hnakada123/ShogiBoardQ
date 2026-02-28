#ifndef CSATOSFENCONVERTER_H
#define CSATOSFENCONVERTER_H

/// @file csatosfenconverter.h
/// @brief CSA形式棋譜コンバータクラスの定義

#include "csalexer.h"
#include "kifparsetypes.h"

#include <QString>
#include <QStringList>

/**
 * @brief CSA形式の棋譜ファイルを解析し、SFEN/USI形式およびGUI用データに変換するクラス
 *
 * CSA形式はコンピュータ将棋協会が策定した棋譜フォーマット。
 * PI/P1..P9による初期局面指定、+/-による指し手、$による対局情報を解析する。
 *
 * 字句解析・トークン化は CsaLexer 名前空間に委譲し、
 * 本クラスはファイルI/Oとパーサ状態管理のオーケストレーションを担当する。
 */
class CsaToSfenConverter
{
public:
    // 型エイリアス（既存API互換）
    using Color = CsaLexer::Color;
    using Piece = CsaLexer::Piece;
    using Cell  = CsaLexer::Cell;
    using Board = CsaLexer::Board;

    // 定数エイリアス（既存コード互換）
    static constexpr Color Black = CsaLexer::Black;
    static constexpr Color White = CsaLexer::White;
    static constexpr Color None  = CsaLexer::None;
    static constexpr Piece NO_P  = CsaLexer::NO_P;
    static constexpr Piece FU    = CsaLexer::FU;
    static constexpr Piece KY    = CsaLexer::KY;
    static constexpr Piece KE    = CsaLexer::KE;
    static constexpr Piece GI    = CsaLexer::GI;
    static constexpr Piece KI    = CsaLexer::KI;
    static constexpr Piece KA    = CsaLexer::KA;
    static constexpr Piece HI    = CsaLexer::HI;
    static constexpr Piece OU    = CsaLexer::OU;
    static constexpr Piece TO    = CsaLexer::TO;
    static constexpr Piece NY    = CsaLexer::NY;
    static constexpr Piece NK    = CsaLexer::NK;
    static constexpr Piece NG    = CsaLexer::NG;
    static constexpr Piece UM    = CsaLexer::UM;
    static constexpr Piece RY    = CsaLexer::RY;

    // --- 公開API ---

    [[nodiscard]] static bool parse(const QString& filePath, KifParseResult& out, QString* warn);
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

private:
    static bool readAllLinesDetectEncoding(const QString& path, QStringList& outLines, QString* warn);
};

#endif // CSATOSFENCONVERTER_H
