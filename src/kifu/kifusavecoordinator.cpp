/// @file kifusavecoordinator.cpp
/// @brief 棋譜保存コーディネータの実装

#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <optional>

#include "kifuioservice.h"   // writeKifuFile / makeDefaultSaveFileName
#include "playmode.h"
#include "gamesettings.h"

namespace KifuSaveCoordinator {

namespace {

/// 拡張子に対応する保存形式。認識できない拡張子は nullopt
std::optional<SaveFormat> formatForExtension(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("kif") || ext == QStringLiteral("kifu")) return SaveFormat::Kif;
    if (ext == QStringLiteral("ki2") || ext == QStringLiteral("ki2u")) return SaveFormat::Ki2;
    if (ext == QStringLiteral("csa"))  return SaveFormat::Csa;
    if (ext == QStringLiteral("jkf"))  return SaveFormat::Jkf;
    if (ext == QStringLiteral("usen")) return SaveFormat::Usen;
    if (ext == QStringLiteral("usi"))  return SaveFormat::Usi;
    return std::nullopt;
}

/// Shift_JIS で書き出す場合、先頭行の encoding 宣言を差し替えた行リストを返す
QStringList withShiftJisHeader(const QStringList& lines)
{
    QStringList result = lines;
    if (!result.isEmpty()
        && result.first().contains(QStringLiteral("encoding=UTF-8"))) {
        result[0].replace(QStringLiteral("encoding=UTF-8"),
                          QStringLiteral("encoding=Shift_JIS"));
    }
    return result;
}

/// 拡張子に応じたエンコーディングで行リストを書き込む
bool writeLinesForPath(const QString& path, const QStringList& lines, QString* outError)
{
    const bool shiftJis = usesShiftJisForPath(path);
    QString err;
    const bool ok = KifuIoService::writeKifuFile(
        path, shiftJis ? withShiftJisHeader(lines) : lines, &err, shiftJis);
    if (!ok && outError) *outError = err;
    return ok;
}

/// 情報が失われる形式への保存を確認する。続行なら true
bool confirmLossySave(QWidget* parent, const QString& title, const QString& message)
{
    const auto result = QMessageBox::warning(
        parent, title, message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok);
    return result != QMessageBox::Cancel;
}

} // namespace

SaveFormat saveFormatForPath(const QString& path)
{
    return formatForExtension(path).value_or(SaveFormat::Kif);
}

QString pathForSelectedFilter(const QString& path, const QString& selectedFilter)
{
    if (path.isEmpty() || !QFileInfo(path).suffix().isEmpty()) return path;
    static const QRegularExpression extensionRe(QStringLiteral("\\(\\*\\.([a-z0-9]+)\\)"));
    const auto match = extensionRe.match(selectedFilter);
    const QString extension = match.hasMatch() ? match.captured(1) : QStringLiteral("kifu");
    return path + QLatin1Char('.') + extension;
}

bool hasKnownSaveExtension(const QString& path)
{
    return formatForExtension(path).has_value();
}

bool usesShiftJisForPath(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QStringLiteral("kif") || ext == QStringLiteral("ki2");
}

QString saveViaDialogWithUsi(QWidget* parent,
                              const QStringList& kifLines,
                              const QStringList& ki2Lines,
                              const QStringList& csaLines,
                              const QStringList& jkfLines,
                              const QStringList& usenLines,
                              const QStringList& usiLines,
                              PlayMode mode,
                              const QString& human1,
                              const QString& human2,
                              const QString& engine1,
                              const QString& engine2,
                              bool hasBranches,
                              bool hasTimeInfo,
                              QString* outError)
{
    // 既定ファイル名を生成
    if (outError) outError->clear();
    QString defaultName = KifuIoService::makeDefaultSaveFileName(
        mode, human1, human2, engine1, engine2, QDateTime::currentDateTime());
    if (defaultName.isEmpty() || defaultName.startsWith(QStringLiteral("_")))
        defaultName = "untitled.kifu";

    // 前回保存したディレクトリを復元
    const QString lastDir = GameSettings::lastKifuSaveDirectory();
    if (!lastDir.isEmpty())
        defaultName = QDir(lastDir).filePath(defaultName);

    // フィルタを作成（KIF/KI2/CSA/JKF/USEN/USI形式が選択可能）
    const QString filter = QObject::tr(
        "KIF形式 UTF-8 (*.kifu);;"
        "KIF形式 Shift_JIS (*.kif);;"
        "KI2形式 UTF-8 (*.ki2u);;"
        "KI2形式 Shift_JIS (*.ki2);;"
        "CSA形式 (*.csa);;"
        "JKF形式 (*.jkf);;"
        "USEN形式 (*.usen);;"
        "USI形式 (*.usi);;"
        "すべてのファイル (*)");

    QString selectedFilter;
    const QString chosenPath = QFileDialog::getSaveFileName(
        parent, QObject::tr("名前を付けて保存"), defaultName, filter, &selectedFilter);
    const QString path = pathForSelectedFilter(chosenPath, selectedFilter);
    if (path.isEmpty()) return QString();

    GameSettings::setLastKifuSaveDirectory(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断
    const QStringList* lines = &kifLines;
    switch (saveFormatForPath(path)) {
    case SaveFormat::Ki2:
        if (hasTimeInfo && !confirmLossySave(
                parent,
                QObject::tr("KI2形式で保存"),
                QObject::tr("KI2形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        lines = &ki2Lines;
        break;
    case SaveFormat::Csa:
        if (hasBranches && !confirmLossySave(
                parent,
                QObject::tr("CSA形式で保存"),
                QObject::tr("CSA形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        lines = &csaLines;
        break;
    case SaveFormat::Jkf:
        lines = &jkfLines;
        break;
    case SaveFormat::Usen:
        lines = &usenLines;
        break;
    case SaveFormat::Usi:
        if (hasBranches && !confirmLossySave(
                parent,
                QObject::tr("USI形式で保存"),
                QObject::tr("USI形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        if (hasTimeInfo && !confirmLossySave(
                parent,
                QObject::tr("USI形式で保存"),
                QObject::tr("USI形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        lines = &usiLines;
        break;
    case SaveFormat::Kif:
        lines = &kifLines;
        break;
    }

    if (!writeLinesForPath(path, *lines, outError)) return QString();
    return path;
}

bool overwriteExisting(const QString& path,
                       const QStringList& lines,
                       QString* outError)
{
    return writeLinesForPath(path, lines, outError);
}

} // namespace KifuSaveCoordinator
