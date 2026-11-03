#ifndef KIFUEXPORT_H
#define KIFUEXPORT_H

/// @file kifuexport.h
/// @brief 棋譜の書き出し（形式ごとの書き出し関数の表）

#include <QList>
#include <QStringList>

#include "kifuexportmetadata.h"
#include "kifuformat.h"
#include "terminaltype.h"

class KifuBranchTree;
class KifuDocument;

/**
 * @brief 棋譜を形式ごとの行にする
 *
 * 書き出しは「棋譜（分岐ツリー）と書き出しの文脈から行を返す関数」を形式ごとに並べた表で選ぶ。
 * GUI の保存・コピーと CLI の変換で同じ表を使う。
 */
namespace KifuExport {

/// 文書の書き出しの文脈（対局情報・開始局面）
KifuExportMetadata metadataOf(const KifuDocument& document);
/// format の行（SFEN・BOD は棋譜を書けないので KIF にする）。文字コードの宣言は metadata.encoding
QStringList lines(const KifuBranchTree& tree, const KifuExportMetadata& metadata, KifuFormat format);
/// 文書を format の行にする（encoding はファイルに保存する文字コード。先頭行の宣言に書く）
QStringList lines(const KifuDocument& document, KifuFormat format, KifuEncoding encoding = KifuEncoding::Utf8);

/// 形式の書き出しが書けるもの（終局の行は writesTerminal）。形式ごとに書けるものの知識はここだけに置く
struct Capabilities {
    bool variations = false;  ///< 変化（書けない形式は本譜だけを書く）
    bool times = false;       ///< 消費時間
    bool comments = false;    ///< コメント（開始局面のコメントを含む）
    bool bookmarks = false;   ///< しおり
};

/// format の書き出しが書けるもの（KIF は全部。KI2 は消費時間、CSA は変化としおり、JKF はしおり、
/// USEN は消費時間・コメント・しおり、USI は全部を書けない。SFEN・BOD は局面だけの形式なので何も書けない）
Capabilities capabilities(KifuFormat format);

/// format の書き出しが変化を書けるか（KIF・KI2・JKF・USEN）。CSA・USI は本譜だけを書き、
/// SFEN・BOD は局面だけの形式なので false（CLI の変換の SFEN の出力は本譜の局面列）
bool writesVariations(KifuFormat format);

/// format の書き出しが type の終局の行を書けるか（CSA・USI・USEN は終局の符号のある種類だけ。
/// KIF・KI2・JKF は終局語で書けるので全部）
bool writesTerminal(KifuFormat format, TerminalType type);

/// 棋譜を format で書くと失うもの
struct Loss {
    bool variations = false;
    bool times = false;
    bool comments = false;
    bool bookmarks = false;
    QList<TerminalType> terminals;  ///< 書けない終局の行の種類（出てきた順、重複なし）

    bool isEmpty() const { return !variations && !times && !comments && !bookmarks && terminals.isEmpty(); }
};

/// tree を format で書くと失うもの（保存の確認と、コピーの知らせで使う）。変化を書けない形式は本譜だけを書くので、
/// 本譜の手だけを見る（変化の中の消費時間・コメントなどは、変化を失うことに含める）
Loss lossIn(const KifuBranchTree& tree, KifuFormat format);

} // namespace KifuExport

#endif // KIFUEXPORT_H
