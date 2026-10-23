#ifndef KIFUFILECONTROLLER_H
#define KIFUFILECONTROLLER_H

/// @file kifufilecontroller.h
/// @brief 棋譜の文書とファイル（開く・保存・上書き・自動保存・貼り付け・クリップボードへのコピー）

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <optional>

#include "kifuformat.h"

class AppActivityState;
class KifuDocument;
class KifuLoadCoordinator;
class KifuNavigationState;
class KifuPasteDialog;
class QProgressDialog;
class QWidget;
class ShogiGameController;

/**
 * @brief 開いている棋譜の文書（KifuDocument）とファイルをつなぐ
 *
 * 読み込み（ファイル・貼り付け・局面）と保存（名前を付けて・上書き・自動）と、棋譜・局面のクリップボードへのコピー。
 * 画面からの操作（ダイアログ・非同期の読み込み）と自動化 API の操作（ダイアログなし・同期）は、
 * 読み込み・保存の後始末（上書き保存先と未保存の状態）を同じ関数で行う。
 */
class KifuFileController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                ///< ダイアログの親（必須）
        KifuDocument* document = nullptr;               ///< 開いている棋譜（必須）
        KifuLoadCoordinator* loadCoordinator = nullptr; ///< 読み込み（全部の読み込みで1つを使い回す。必須）
        ShogiGameController* gameController = nullptr;  ///< 盤（局面のコピー。必須）
        const KifuNavigationState* navState = nullptr;  ///< 表示中の手（局面のコピー・現在の手までのコピー。必須）
        /// 動作状態（棋譜を置き換えてよいかの判定。必須）
        const AppActivityState* activity = nullptr;
    };

    explicit KifuFileController(const Deps& deps, QObject* parent = nullptr);

    /// 未保存変更の確認。保存失敗・キャンセル時は呼び出し元の操作を中止する
    [[nodiscard]] bool confirmDiscardUnsaved();

public slots:
    /// 棋譜ファイルを選択して開く
    void chooseAndLoadKifuFile();
    /// 名前を付けて保存
    void saveKifuToFile();
    /// 上書き保存
    void overwriteKifuFile();
    /// クリップボードから棋譜を貼り付け
    void pasteKifuFromClipboard();
    /// 棋譜貼り付けダイアログからの取り込み
    void onKifuPasteImportRequested(const QString& content);
    /// SFEN局面集から選択された局面を反映
    void onSfenCollectionPositionSelected(const QString& sfen);

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

public:
    /// 棋譜を saveDir へ自動保存する（対局の終局時。形式は拡張子 extension）。保存したファイルを上書き保存先にする
    void autoSaveKifu(const QString& saveDir, const QString& extension);

    // --- 自動化 API 用の非対話 API（確認ダイアログを出さない。未保存の確認は呼び出し側が行う）---

    /// 未保存の変更があるか
    bool hasUnsavedChanges() const;
    /// 棋譜を読み込み・局面の反映で置き換えてよいか（待機中だけ。対局・検討・解析・局面編集の間は置き換えない）
    bool canReplaceRecord() const;
    /// 棋譜ファイルを読み込む（拡張子で形式を判定）。成功時は上書き保存先を更新する
    [[nodiscard]] bool loadKifuFile(const QString& filePath);
    /// 棋譜テキストを読み込む（形式は内容から判定）。上書き保存先はクリアする
    [[nodiscard]] bool loadKifuText(const QString& content);
    /// SFEN 局面を反映する。上書き保存先はクリアする
    [[nodiscard]] bool applySfenPosition(const QString& sfen);
    /// 指定パスへ保存する（形式は拡張子で決定）。ダイアログは出さず、失敗理由を errorText に返す。
    /// 成功時は上書き保存先を更新する
    [[nodiscard]] bool saveKifuToPath(const QString& filePath, QString* errorText = nullptr);

signals:
    /// ステータスバーに出す知らせ
    void statusMessage(const QString& message, int timeout);

private slots:
    void onAsyncLoadFinished(bool success);

private:
    /// 棋譜を置き換えられない状態なら知らせて true
    bool refuseReplacingRecord();

    /// 読み込んだものの出どころ（読み込みの後始末が違う）
    enum class Source {
        File,         ///< 棋譜ファイル（上書き保存先にし、変更なしとする）
        Text,         ///< 貼り付けた棋譜（ファイルにない内容として未保存にする）
        Position,     ///< 自動化 API で反映した局面（未保存にする）
        Collection,   ///< 局面集から反映した局面（局面集に残っているので変更なしとし、上書き保存先は持たない）
    };

    /// 開いている棋譜の format の行
    QStringList exportLines(KifuFormat format) const;
    void startAsyncLoad(const QString& input, Source source);
    /// 読み込みの後始末（画面・自動化 API のどちらの読み込みでも同じ）
    void finishLoad(bool success, Source source, const QString& path);
    /// path に保存する（形式は拡張子）。dialogParent があれば Shift_JIS で表せない文字を確認する
    bool writeToPath(const QString& filePath, QWidget* dialogParent, QString* errorText);
    void markSaved(const QString& filePath, const QString& message);
    bool copyRecord(KifuFormat format);
    bool setClipboardText(const QString& text, const QString& successMsg);
    /// 盤の局面の SFEN（手数は表示中の手の次の手）
    QString boardSfen() const;

    Deps m_deps;
    QPointer<KifuPasteDialog> m_kifuPasteDialog;
    QPointer<QProgressDialog> m_loadProgress;
    QString m_pendingLoadPath;
    Source m_pendingSource = Source::File;
    bool m_asyncLoadPending = false;   ///< このクラスが始めた非同期の読み込みの終わりを待っている
};

#endif // KIFUFILECONTROLLER_H
