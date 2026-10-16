#ifndef KIFUEXPORTCONTROLLER_H
#define KIFUEXPORTCONTROLLER_H

/// @file kifuexportcontroller.h
/// @brief 棋譜の書き出し（ファイル保存・上書き・自動保存・クリップボードへのコピー）

#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

#include "gamerecordmodel.h"
#include "kifusavefile.h"

struct AppState;
class GameInfoPane;
class KifuNavigationState;
class MatchTimeRecord;
class QWidget;
class ShogiGameController;

/**
 * @brief 棋譜を形式ごとの行にして、ファイル・クリップボードへ書き出す
 *
 * 書き出しの文脈（対局情報・開始局面・対局者・持ち時間・開始と終了の日時）はここで1回だけ組み立てる。
 */
class KifuExportController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                    ///< 保存ダイアログの親
        GameRecordModel* gameRecord = nullptr;              ///< 棋譜（必須）
        GameInfoPane* gameInfoPane = nullptr; ///< 対局情報
        const MatchTimeRecord* timeRecord = nullptr;        ///< 持ち時間と開始・終了の日時
        ShogiGameController* gameController = nullptr;      ///< 盤（局面のコピー）
        const AppState* state = nullptr;                    ///< 対局者
        const KifuNavigationState* navState = nullptr;      ///< 表示中の手（局面のコピー・現在の手までのコピー）
    };

    explicit KifuExportController(const Deps& deps, QObject* parent = nullptr);

    // --- ファイル保存 ---
    /// 棋譜をファイルに保存する（ダイアログ表示）。保存したファイルのパス（キャンセル時は空）
    QString saveToFile();
    /// 棋譜を上書き保存する（Shift_JIS で表せない文字の確認・失敗の通知にダイアログを使う）
    [[nodiscard]] bool overwriteFile(const QString& filePath);
    /// ダイアログを出さずに指定パスへ保存する（自動化 API 用）。失敗の理由を errorText に返す
    [[nodiscard]] bool saveToPathWithoutDialogs(const QString& filePath, QString* errorText);
    /**
     * @brief 指定フォルダへ自動保存する（ダイアログなし）。保存したパスを返す
     * @param extension 保存形式の拡張子（kifu・kif・ki2・csa・jkf・usen・usi）。名前は対局者と日時から作る
     */
    [[nodiscard]] std::optional<QString> autoSaveToDir(const QString& saveDir,
                                                       const QString& extension = QStringLiteral("kifu"));
    /// 指定形式の出力行を作る（ファイルに書かない。自動化 API 用）
    QStringList exportLines(KifuFormat format) const;

public slots:
    // --- クリップボードへのコピー ---
    bool copyKifToClipboard() { return copyRecord(KifuFormat::Kif); }
    bool copyKi2ToClipboard() { return copyRecord(KifuFormat::Ki2); }
    bool copyCsaToClipboard() { return copyRecord(KifuFormat::Csa); }
    bool copyUsiToClipboard() { return copyRecord(KifuFormat::Usi); }
    bool copyJkfToClipboard() { return copyRecord(KifuFormat::Jkf); }
    bool copyUsenToClipboard() { return copyRecord(KifuFormat::Usen); }
    /// USI 形式で、表示中の手順の表示中の手までをコピーする
    bool copyUsiCurrentToClipboard();
    /// 盤の局面を SFEN でコピーする
    bool copySfenToClipboard();
    /// 盤の局面を BOD（局面図）でコピーする
    bool copyBodToClipboard();

signals:
    /// ステータスバーに出す知らせ
    void statusMessage(const QString& message, int timeout);

private:
    GameRecordModel::ExportContext buildExportContext() const;
    bool writeToPath(const QString& filePath, QWidget* dialogParent, QString* errorText);
    bool copyRecord(KifuFormat format);
    bool setClipboardText(const QString& text, const QString& successMsg);
    /// 盤の局面の SFEN（手数は表示中の手の次の手）
    QString boardSfen() const;
    void markSaved(const QString& message);

    Deps m_deps;
};

#endif // KIFUEXPORTCONTROLLER_H
