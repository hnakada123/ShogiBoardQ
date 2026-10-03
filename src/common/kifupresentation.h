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
QString status(const QString& canonical);
QString infoKey(const QString& canonical);
QString infoValue(const QString& key, const QString& raw);
}

#endif
