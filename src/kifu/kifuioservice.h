#ifndef KIFUIOSERVICE_H
#define KIFUIOSERVICE_H

#include <QString>
#include <QStringList>
#include <QDateTime>

#include "playmode.h"

namespace KifuIoService {

QString makeDefaultSaveFileName(PlayMode mode,
                                const QString& human1,
                                const QString& human2,
                                const QString& engine1,
                                const QString& engine2,
                                const QDateTime& now,
                                const QString& extension = QStringLiteral("kifu"));

bool writeKifuFile(const QString& filePath,
                   const QStringList& kifuLines,
                   QString* errorText);

} // namespace

#endif // KIFUIOSERVICE_H
