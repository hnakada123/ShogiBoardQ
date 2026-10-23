#ifndef SFENCSAPOSITIONCONVERTER_H
#define SFENCSAPOSITIONCONVERTER_H

#include <QString>
#include <QStringList>
#include <optional>

#include "shogiposition.h"

namespace SfenCsaPositionConverter {

/// CSA の局面の行（PI・P1〜P9・P+・P-・手番）を SFEN にする（読めなければ空）
[[nodiscard]] std::optional<QString> fromCsaPositionLines(const QStringList& csaLines,
                                                          QString* outError = nullptr);
/// 局面を CSA の局面の行（P1〜P9・持ち駒の P+・P-・手番）にする
[[nodiscard]] QStringList toCsaPositionLines(const Position& position);

} // namespace SfenCsaPositionConverter

#endif // SFENCSAPOSITIONCONVERTER_H
