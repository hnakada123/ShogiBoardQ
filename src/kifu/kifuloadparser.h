#ifndef KIFULOADPARSER_H
#define KIFULOADPARSER_H

/// @file kifuloadparser.h
/// @brief 棋譜の読み込み口（ファイルを1回読み、形式ごとの変換器へ振り分ける。文字コードは KifuTextDecoder）

#include "kifuformat.h"
#include "kifparsetypes.h"
#include "threadtypes.h"
#include "shogimove.h"

class Position;

/// GUIオブジェクトを含まない読み込み結果。ワーカーから値で受け渡す。
struct KifuLoadResult {
    bool success = false;
    KifuFormat format = KifuFormat::Kif;  ///< 読んだ形式
    QString filePath;
    StartKind startKind = StartKind::Default;
    bool positionOnly = false;            ///< 指し手のない局面だけの棋譜（USI・SFEN・BOD）
    QString error;
    QString warning;
    QList<KifGameInfoItem> gameInfo;
    KifParseResult record;                ///< 開始局面は record.mainline.startSfen だけ

    const QString& startSfen() const { return record.mainline.startSfen; }
};

namespace KifuLoadParser {
/// ファイルを1回読み（文字コードは形式によらず KifuTextDecoder::readAllTextAuto。BOM → 先頭の宣言 → 判別）、
/// parseContent に渡す。形式がなければファイルから判定する
KifuLoadResult parseFile(const QString& path, std::optional<KifuFormat> format = std::nullopt,
                         const CancelFlag& cancel = {});
/// 貼り付けたテキストの形式を判定して読む（判定できなければ KIF として読む）
KifuLoadResult parseText(const QString& content, const CancelFlag& cancel = {});
/// 形式が決まっているテキストを読む（指し手のない USI・SFEN と BOD は positionOnly にする。指し手も開始局面の指定も
/// ない棋譜は失敗にする）。
/// sourceName は失敗時のメッセージに使う（ファイル名など。空でもよい）
KifuLoadResult parseContent(const QString& content, KifuFormat format,
                            const CancelFlag& cancel = {}, const QString& sourceName = QString());
/**
 * @brief 開始局面にできない理由（訳した文。できれば空）
 *
 * 盤と持駒として成り立つか（駒の数・持駒の玉・玉の重複）だけを見る。玉の無い図（駒箱に入れた編集中の盤など）と
 * 手番でない側に王手がかかっている図は受け付ける。棋譜の読み込み（GUI・CLI・自動化 API の kifu.load）・
 * 局面の読み込み（KifuLoader::loadPositionFromSfen。局面の貼り付け・局面集）・自動化 API の position.set で同じ規則。
 * 局面集・詰将棋問題一覧のファイルの行は、検討・詰み探索に使う局面なので、より厳しい SfenValidator で読む
 */
QString startPositionProblem(const QString& sfen);
}

#endif // KIFULOADPARSER_H
