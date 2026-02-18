#ifndef SFENCSAPOSITIONCONVERTER_H
#define SFENCSAPOSITIONCONVERTER_H

#include <QString>
#include <QStringList>

namespace SfenCsaPositionConverter {

bool fromCsaPositionLines(const QStringList& csaLines, QString* outSfen, QString* outError = nullptr);
QStringList toCsaPositionLines(const QString& sfen, bool* ok = nullptr, QString* outError = nullptr);

} // namespace SfenCsaPositionConverter

#endif // SFENCSAPOSITIONCONVERTER_H
