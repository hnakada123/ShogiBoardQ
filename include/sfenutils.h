#ifndef SFENUTILS_H
#define SFENUTILS_H

#include <QString>

namespace SfenUtils {

// "startpos" を完全SFENへ正規化。既に完全SFENならそのまま返す。
inline QString normalizeStart(const QString& startPositionStr)
{
    if (startPositionStr == QStringLiteral("startpos")) {
        return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    return startPositionStr;
}

} // namespace SfenUtils

#endif // SFENUTILS_H
