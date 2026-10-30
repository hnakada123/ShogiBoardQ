/// @file kifusavedialogs.cpp
/// @brief 棋譜の保存ダイアログ・未保存の確認の実装

#include "kifusavedialogs.h"
#include "kifufilefilter.h"
#include "unsavedchangesprompt.h"

#include <QFileDialog>
#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>

#include "gamesettings.h"

bool KifuSaveDialogs::confirmUtf8Fallback(QWidget* parent, const QString& characters)
{
    QMessageBox box(
        QMessageBox::Warning, tr("Shift_JISで保存できない文字"),
        tr("次の文字はShift_JISで保存できません：%1\n\n"
                    "UTF-8で保存しますか？ファイルの先頭行に「encoding=UTF-8」を記録します。").arg(characters),
        QMessageBox::Save | QMessageBox::Cancel,
        parent);
    box.button(QMessageBox::Save)->setText(tr("UTF-8で保存"));
    box.button(QMessageBox::Cancel)->setText(tr("キャンセル"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Save;
}

bool KifuSaveDialogs::confirmLossySave(QWidget* parent, const QString& title, const QString& message)
{
    const auto result = QMessageBox::warning(
        parent, title, message,
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok);
    return result == QMessageBox::Ok;
}

QString KifuSaveDialogs::saveViaDialog(QWidget* parent,
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

    // 絞り込みは保存できる拡張子の表から作る
    QString selectedFilter;
    const QString chosenPath = QFileDialog::getSaveFileName(
        parent, tr("名前を付けて保存"), defaultName, KifuFileFilter::forSaving(), &selectedFilter);
    const QString path = KifuFileFilter::pathForSaving(chosenPath, selectedFilter);
    if (path.isEmpty()) return QString();

    GameSettings::lastKifuSaveDirectory.set(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断
    const KifuFormat format = KifuSaveFile::saveFormatForPath(path);
    switch (format) {
    case KifuFormat::Ki2:
        if (hasTimeInfo && !confirmLossySave(
                parent,
                tr("KI2形式で保存"),
                tr("KI2形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Csa:
        if (hasBranches && !confirmLossySave(
                parent,
                tr("CSA形式で保存"),
                tr("CSA形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Jkf:
        break;
    case KifuFormat::Usen:
        if (hasTimeInfo && !confirmLossySave(
                parent,
                tr("USEN形式で保存"),
                tr("USEN形式は消費時間・コメント・しおりに対応していないため、これらの情報は保存されません。\n"
                            "詰み・反則勝ちなど一部の終局理由も保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Usi:
        if (hasBranches && !confirmLossySave(
                parent,
                tr("USI形式で保存"),
                tr("USI形式は分岐に対応していないため、分岐の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        if (hasTimeInfo && !confirmLossySave(
                parent,
                tr("USI形式で保存"),
                tr("USI形式は消費時間に対応していないため、消費時間の情報は保存されません。\n保存を続けますか？"))) {
            return QString();
        }
        break;
    case KifuFormat::Kif:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }

    if (!overwriteExisting(parent, path, generateLines, outError)) return QString();
    return path;
}

bool KifuSaveDialogs::confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save)
{
    if (!isDirty) return true;
    switch (UnsavedChangesPrompt::ask(parent, tr("未保存の棋譜"), tr("棋譜に未保存の変更があります。保存しますか？"))) {
    case UnsavedChangesPrompt::Choice::Save:
        return save();
    case UnsavedChangesPrompt::Choice::Discard:
        return true;
    case UnsavedChangesPrompt::Choice::Cancel:
        break;
    }
    return false;
}

bool KifuSaveDialogs::overwriteExisting(QWidget* parent, const QString& path,
                                        const KifuSaveFile::LineGenerator& generateLines, QString* outError)
{
    // 親ウィンドウが無い（自動化 API から保存する）ときは確認を出さずエラーにする
    if (!parent) return KifuSaveFile::write(path, generateLines, outError);
    return KifuSaveFile::write(path, generateLines, outError, [parent](const QString& missing) {
        return confirmUtf8Fallback(parent, missing);
    });
}

