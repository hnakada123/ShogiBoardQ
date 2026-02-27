/// @file kifusavecoordinator.cpp
/// @brief 棋譜保存コーディネータの実装

#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>

#include "kifuioservice.h"   // writeKifuFile / makeDefaultSaveFileName
#include "playmode.h"
#include "gamesettings.h"

namespace KifuSaveCoordinator {

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

    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("名前を付けて保存"), defaultName, filter);
    if (path.isEmpty()) return QString();

    GameSettings::setLastKifuSaveDirectory(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断
    QString err;
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    // Shift_JIS で書き出す拡張子かどうか
    const bool shiftJis = (ext == QStringLiteral("kif") || ext == QStringLiteral("ki2"));

    if (ext == QStringLiteral("ki2") || ext == QStringLiteral("ki2u")) {
        // 消費時間がある場合は警告を表示
        if (hasTimeInfo) {
            const auto result = QMessageBox::warning(
                parent,
                QObject::tr("KI2形式で保存"),
                QObject::tr("KI2形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"),
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Ok);
            if (result == QMessageBox::Cancel) {
                return QString();
            }
        }
        // KI2形式で保存（Shift_JIS の場合はヘッダを差し替え）
        QStringList lines = ki2Lines;
        if (shiftJis && !lines.isEmpty()
            && lines.first().contains(QStringLiteral("encoding=UTF-8"))) {
            lines[0].replace(QStringLiteral("encoding=UTF-8"),
                             QStringLiteral("encoding=Shift_JIS"));
        }
        if (!KifuIoService::writeKifuFile(path, lines, &err, shiftJis)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("csa")) {
        // 分岐がある場合は警告を表示
        if (hasBranches) {
            const auto result = QMessageBox::warning(
                parent,
                QObject::tr("CSA形式で保存"),
                QObject::tr("CSA形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"),
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Ok);
            if (result == QMessageBox::Cancel) {
                return QString();
            }
        }
        // CSA形式で保存
        if (!KifuIoService::writeKifuFile(path, csaLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("jkf")) {
        // JKF形式で保存
        if (!KifuIoService::writeKifuFile(path, jkfLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("usen")) {
        // USEN形式で保存
        if (!KifuIoService::writeKifuFile(path, usenLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("usi")) {
        // 分岐がある場合は警告を表示
        if (hasBranches) {
            const auto result = QMessageBox::warning(
                parent,
                QObject::tr("USI形式で保存"),
                QObject::tr("USI形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"),
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Ok);
            if (result == QMessageBox::Cancel) {
                return QString();
            }
        }
        // 消費時間がある場合は警告を表示
        if (hasTimeInfo) {
            const auto result = QMessageBox::warning(
                parent,
                QObject::tr("USI形式で保存"),
                QObject::tr("USI形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"),
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Ok);
            if (result == QMessageBox::Cancel) {
                return QString();
            }
        }
        // USI形式で保存
        if (!KifuIoService::writeKifuFile(path, usiLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else {
        // KIF形式で保存（デフォルト / Shift_JIS の場合はヘッダを差し替え）
        QStringList lines = kifLines;
        if (shiftJis && !lines.isEmpty()
            && lines.first().contains(QStringLiteral("encoding=UTF-8"))) {
            lines[0].replace(QStringLiteral("encoding=UTF-8"),
                             QStringLiteral("encoding=Shift_JIS"));
        }
        if (!KifuIoService::writeKifuFile(path, lines, &err, shiftJis)) {
            if (outError) *outError = err;
            return QString();
        }
    }
    return path;
}

bool overwriteExisting(const QString& path,
                       const QStringList& kifuLines,
                       QString* outError)
{
    // 拡張子から Shift_JIS かどうかを判断
    const QString ext = QFileInfo(path).suffix().toLower();
    const bool shiftJis = (ext == QStringLiteral("kif") || ext == QStringLiteral("ki2"));

    QStringList lines = kifuLines;
    if (shiftJis && !lines.isEmpty()
        && lines.first().contains(QStringLiteral("encoding=UTF-8"))) {
        lines[0].replace(QStringLiteral("encoding=UTF-8"),
                         QStringLiteral("encoding=Shift_JIS"));
    }

    QString err;
    const bool ok = KifuIoService::writeKifuFile(path, lines, &err, shiftJis);
    if (!ok && outError) *outError = err;
    return ok;
}

} // namespace KifuSaveCoordinator
