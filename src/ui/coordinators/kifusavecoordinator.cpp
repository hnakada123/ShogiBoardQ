/// @file kifusavecoordinator.cpp
/// @brief 棋譜の保存ダイアログ・未保存の確認の実装

#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>

#include "gamesettings.h"

namespace KifuSaveCoordinator {

namespace {

/// Shift_JIS で表せない文字があるとき、UTF-8 で保存するか確認する。保存するなら true
bool confirmUtf8Fallback(QWidget* parent, const QString& characters)
{
    QMessageBox box(
        QMessageBox::Warning, QObject::tr("Shift_JISで保存できない文字"),
        QObject::tr("次の文字はShift_JISで保存できません：%1\n\n"
                    "UTF-8で保存しますか？ファイルの先頭行に「encoding=UTF-8」を記録します。").arg(characters),
        QMessageBox::Save | QMessageBox::Cancel,
        parent);
    box.button(QMessageBox::Save)->setText(QObject::tr("UTF-8で保存"));
    box.button(QMessageBox::Cancel)->setText(QObject::tr("キャンセル"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Save;
}

/// 情報が失われる形式への保存を確認する。続行なら true
bool confirmLossySave(QWidget* parent, const QString& title, const QString& message)
{
    const auto result = QMessageBox::warning(
        parent, title, message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok);
    return result == QMessageBox::Ok;
}

} // namespace

QString saveViaDialog(QWidget* parent,
                      const KifuSaveFile::LineGenerator& generateLines,
                      const QString& defaultFileName,
                      bool hasBranches,
                      bool hasTimeInfo,
                      QString* outError)
{
    // 既定ファイル名を生成
    if (outError) outError->clear();
    QString defaultName = defaultFileName;

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
    const QString path = KifuSaveFile::pathForSelectedFilter(chosenPath, selectedFilter);
    if (path.isEmpty()) return QString();

    GameSettings::lastKifuSaveDirectory.set(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断
    const KifuFormat format = KifuSaveFile::saveFormatForPath(path);
    switch (format) {
    case KifuFormat::Ki2:
        if (hasTimeInfo && !confirmLossySave(
                parent,
                QObject::tr("KI2形式で保存"),
                QObject::tr("KI2形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Csa:
        if (hasBranches && !confirmLossySave(
                parent,
                QObject::tr("CSA形式で保存"),
                QObject::tr("CSA形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Jkf:
        break;
    case KifuFormat::Usen:
        if (hasTimeInfo && !confirmLossySave(
                parent,
                QObject::tr("USEN形式で保存"),
                QObject::tr("USEN形式は消費時間・コメント・しおりに対応していないため、これらの情報は保存されません。\n"
                            "詰み・反則勝ちなど一部の終局理由も保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Usi:
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
        break;
    case KifuFormat::Kif:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }

    if (!overwriteExisting(parent, path, generateLines(format), outError)) return QString();
    return path;
}

bool confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save)
{
    if (!isDirty) return true;
    QMessageBox box(
        QMessageBox::Warning, QObject::tr("未保存の棋譜"),
        QObject::tr("棋譜に未保存の変更があります。保存しますか？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        parent);
    box.button(QMessageBox::Save)->setText(QObject::tr("保存"));
    box.button(QMessageBox::Discard)->setText(QObject::tr("破棄"));
    box.button(QMessageBox::Cancel)->setText(QObject::tr("キャンセル"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);
    const int choice = box.exec();
    if (choice == QMessageBox::Save) return save();
    return choice == QMessageBox::Discard;
}

bool overwriteExisting(QWidget* parent, const QString& path, const QStringList& lines, QString* outError)
{
    // 親ウィンドウが無い（自動化 API から保存する）ときは確認を出さずエラーにする
    if (!parent) return KifuSaveFile::write(path, lines, outError);
    return KifuSaveFile::write(path, lines, outError, [parent](const QString& missing) {
        return confirmUtf8Fallback(parent, missing);
    });
}

} // namespace KifuSaveCoordinator
