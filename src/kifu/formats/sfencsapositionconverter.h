#ifndef SFENCSAPOSITIONCONVERTER_H
#define SFENCSAPOSITIONCONVERTER_H

#include <QString>
#include <QStringList>
#include <optional>

namespace SfenCsaPositionConverter {

[[nodiscard]] std::optional<QString> fromCsaPositionLines(const QStringList& csaLines,
                                                          QString* outError = nullptr);
[[nodiscard]] std::optional<QStringList> toCsaPositionLines(const QString& sfen,
                                                             QString* outError = nullptr);

} // namespace SfenCsaPositionConverter

#endif // SFENCSAPOSITIONCONVERTER_H
