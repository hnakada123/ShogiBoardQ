#ifndef KIFUPRESENTATION_H
#define KIFUPRESENTATION_H

#include <QString>
#include "shogimove.h"

// Presentation only. Never use these strings in a file writer or a protocol.
namespace KifuPresentation {
enum class Notation { Japanese, Western };
struct Options {
    Notation notation = Notation::Japanese;
    bool alwaysOrigin = false;
};

QString resolveLanguage(const QString& setting, const QString& systemLocale);
void configure(const QString& language, const QString& notation, bool alwaysOrigin);
Options options();
QString rankLabel(int rank);
QString usiMove(const ShogiMove& move);
QString move(const QString& beforeSfen, const QString& usi, Options style,
             bool includeSide = true);
QString pv(const QString& beforeSfen, const QString& usiMoves, const QString& japanese,
           bool fullOrigin = false);
QString label(const QString& canonical, const QString& beforeSfen = {},
              const QString& usi = {}, bool fullOrigin = false);
/// 1手の表記（手数・分岐の印を含まない）。日本語表記なら棋譜の表記（終局語は表示言語に合わせる）、
/// 英語表記なら指す前の局面と指し手から作る
QString moveLabel(const QString& canonical, const QString& beforeSfen, const QString& usi, bool fullOrigin = false);
/// 分岐がある手に付ける印（日本語表記は "+"、英語表記は " [+]"）
QString branchMark();

QString status(const QString& canonical);
QString infoKey(const QString& canonical);
QString infoValue(const QString& key, const QString& raw);
}

#endif
