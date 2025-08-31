#ifndef KIFREADER_H
#define KIFREADER_H

#include <QString>
#include <QStringList>

class KifReader
{
public:
    // Read entire file as text with BOM/declared-encoding auto-detection (Shift_JIS/CP932 supported)
    static bool readTextAuto(const QString& path,
                             QString& outText,
                             QString* usedEncoding = nullptr,
                             QString* error = nullptr);

    // Read file as normalized lines (handles CR/LF variants), using the same auto-detection
    static bool readLinesAuto(const QString& path,
                              QStringList& outLines,
                              QString* usedEncoding = nullptr,
                              QString* error = nullptr);
};

#endif // KIFREADER_H
