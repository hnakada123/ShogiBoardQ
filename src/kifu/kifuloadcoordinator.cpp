#include "kifuloadcoordinator.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "engineanalysistab.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifunavigationstate.h"

#include <QDebug>
#include <QStyledItemDelegate>
#include <QAbstractItemView>         // view->model() を使うなら
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTableWidget>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <functional>

// デバッグのオン/オフ（必要に応じて true に）
static bool kGM_VERBOSE = false;

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                                         QStringList& positionStrList,
                                         int& activePly,
                                         int& currentSelectedPly,
                                         int& currentMoveIndex,
                                         QStringList* sfenRecord,
                                         QTableWidget* gameInfoTable,
                                         QDockWidget* gameInfoDock,
                                         QTabWidget* tab,
                                         RecordPane* recordPane,
                                         KifuRecordListModel* kifuRecordModel,
                                         KifuBranchListModel* kifuBranchModel,
                                         QObject* parent)
    : QObject(parent)
    , m_gameInfoTable(gameInfoTable)
    , m_gameInfoDock(gameInfoDock)
    , m_tab(tab)
    , m_sfenRecord(sfenRecord)
    , m_gameMoves(gameMoves)                // ← 参照メンバに束縛（同一実体を共有）
    , m_positionStrList(positionStrList)    // ← 同上
    , m_recordPane(recordPane)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
{
    // 必要ならデバッグ時にチェック
    // Q_ASSERT(m_sfenRecord && "sfenRecord must not be null");
    // m_analysisTab は setAnalysisTab() 経由で後から設定される
    // m_shogiView は setShogiView() 経由で後から設定される
}

// ============================================================
// デバッグ出力ヘルパー関数（ファイルスコープ内でのみ使用）
// ============================================================

// 本譜（メインライン）のデバッグダンプ
static void dumpMainline(const KifParseResult& res, const QString& parseWarn)
{
    if (!kGM_VERBOSE) return;

    qDebug().noquote() << "[GM] KifParseResult dump:";
    if (!parseWarn.isEmpty()) {
        qDebug().noquote() << "  [parseWarn]" << parseWarn;
    }

    qDebug().noquote() << "  Mainline:";
    qDebug().noquote() << "    baseSfen: " << res.mainline.baseSfen;
    qDebug().noquote() << "    usiMoves: " << res.mainline.usiMoves;
    qDebug().noquote() << "    disp:";
    int mainIdx = 0;
    for (const auto& d : std::as_const(res.mainline.disp)) {
        qDebug().noquote() << "      [" << mainIdx << "] prettyMove: " << d.prettyMove;
        qDebug().noquote() << "           comment: " << (d.comment.isEmpty() ? "<none>" : d.comment);
        qDebug().noquote() << "           timeText: " << d.timeText;
        ++mainIdx;
    }
}

// 変化（バリエーション）のデバッグダンプ
static void dumpVariationsDebug(const KifParseResult& res)
{
    if (!kGM_VERBOSE) return;

    qDebug().noquote() << "  Variations:";
    int varNo = 0;
    for (const KifVariation& var : std::as_const(res.variations)) {
        qDebug().noquote() << "  [Var " << varNo << "]";
        qDebug().noquote() << "    startPly: " << var.startPly;
        qDebug().noquote() << "    baseSfen: " << var.line.baseSfen;
        qDebug().noquote() << "    usiMoves: " << var.line.usiMoves;
        qDebug().noquote() << "    disp:";
        int dispIdx = 0;
        for (const auto& d : std::as_const(var.line.disp)) {
            qDebug().noquote() << "      [" << dispIdx << "] prettyMove: " << d.prettyMove;
            qDebug().noquote() << "           comment: " << (d.comment.isEmpty() ? "<none>" : d.comment);
            qDebug().noquote() << "           timeText: " << d.timeText;
            ++dispIdx;
        }
        ++varNo;
    }
}

// ============================================================
// 棋譜読み込み共通処理
// ============================================================

// 棋譜読み込みの共通ロジック
// parseFunc: 解析関数（必須）
// detectSfenFunc: 初期SFEN検出関数（空の場合はprepareInitialSfenを使用）
// extractGameInfoFunc: ゲーム情報抽出関数（空の場合はスキップ）
// dumpVariations: 変化のデバッグ出力を行うかどうか
void KifuLoadCoordinator::loadKifuCommon(
    const QString& filePath,
    const char* funcName,
    const KifuParseFunc& parseFunc,
    const KifuDetectSfenFunc& detectSfenFunc,
    const KifuExtractGameInfoFunc& extractGameInfoFunc,
    bool dumpVariations)
{
    // ★ パフォーマンス計測用
    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer stepTimer;
    auto logStep = [&](const char* stepName) {
        qDebug().noquote() << QStringLiteral("[PERF] %1: %2 ms").arg(stepName).arg(stepTimer.elapsed());
        stepTimer.restart();
    };
    stepTimer.start();

    // --- IN ログ ---
    qDebug().noquote() << "[MAIN]" << funcName << "IN file=" << filePath;

    // ★ ロード中フラグ（分岐更新を抑止）
    m_loadingKifu = true;

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    QString initialSfen;
    if (detectSfenFunc) {
        initialSfen = detectSfenFunc(filePath, &teaiLabel);
        if (initialSfen.isEmpty()) {
            initialSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
            teaiLabel = QStringLiteral("平手(既定)");
        }
    } else {
        initialSfen = prepareInitialSfen(filePath, teaiLabel);
    }
    logStep("detectInitialSfen");

    // 2) 解析（本譜＋分岐＋コメント）を一括取得
    KifParseResult res;
    QString parseWarn;
    if (!parseFunc(filePath, res, &parseWarn)) {
        qWarning().noquote() << "[GM] parse failed:" << filePath << parseWarn;
        emit errorOccurred(tr("棋譜ファイルの読み込みに失敗しました: %1").arg(QFileInfo(filePath).fileName()));
        m_loadingKifu = false;
        return;
    }
    if (!parseWarn.isEmpty()) {
        qWarning().noquote() << "[GM] parse warn:" << parseWarn;
        emit errorOccurred(tr("棋譜の読み込みで警告があります:\n%1").arg(parseWarn));
    }
    logStep("parseFunc");

    // 3) デバッグ出力
    dumpMainline(res, parseWarn);
    if (dumpVariations) {
        dumpVariationsDebug(res);
    }
    logStep("dumpMainline/Variations");

    // 4) 先手/後手名などヘッダ反映
    if (extractGameInfoFunc) {
        const QList<KifGameInfoItem> infoItems = extractGameInfoFunc(filePath);
        populateGameInfo(infoItems);
        applyPlayersFromGameInfo(infoItems);
    }
    logStep("extractGameInfo");

    // 5) 共通の後処理に委譲
    applyParsedResultCommon(filePath, initialSfen, teaiLabel, res, parseWarn, funcName);
    qDebug().noquote() << QStringLiteral("[PERF] loadKifuCommon TOTAL: %1 ms").arg(totalTimer.elapsed());
}

// ============================================================
// 各フォーマット用の公開関数
// ============================================================

void KifuLoadCoordinator::loadKi2FromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadKi2FromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            return Ki2ToSfenConverter::parseWithVariations(path, res, warn);
        },
        // 初期SFEN検出（デフォルト使用）
        KifuDetectSfenFunc(),
        // ゲーム情報抽出
        [](const QString& path) {
            return CsaToSfenConverter::extractGameInfo(path);
        },
        // 変化のデバッグ出力なし
        false
    );
}

void KifuLoadCoordinator::loadCsaFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadCsaFromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            return CsaToSfenConverter::parse(path, res, warn);
        },
        // 初期SFEN検出（デフォルト使用）
        KifuDetectSfenFunc(),
        // ゲーム情報抽出
        [](const QString& path) {
            return CsaToSfenConverter::extractGameInfo(path);
        },
        // 変化のデバッグ出力なし
        false
    );
}

void KifuLoadCoordinator::loadJkfFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadJkfFromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            JkfToSfenConverter::parseWithVariations(path, res, warn);
            return true;  // JKFは戻り値がvoidなので常にtrue
        },
        // 初期SFEN検出（JKF専用）
        [](const QString& path, QString* label) {
            return JkfToSfenConverter::detectInitialSfenFromFile(path, label);
        },
        // ゲーム情報抽出
        [](const QString& path) {
            return JkfToSfenConverter::extractGameInfo(path);
        },
        // 変化のデバッグ出力あり
        true
    );
}

void KifuLoadCoordinator::loadKifuFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadKifuFromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            KifToSfenConverter::parseWithVariations(path, res, warn);
            return true;  // KIFは戻り値がvoidなので常にtrue
        },
        // 初期SFEN検出（デフォルト使用）
        KifuDetectSfenFunc(),
        // ゲーム情報抽出
        [](const QString& path) {
            return KifToSfenConverter::extractGameInfo(path);
        },
        // 変化のデバッグ出力あり
        true
    );
}

void KifuLoadCoordinator::loadUsenFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadUsenFromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            UsenToSfenConverter::parseWithVariations(path, res, warn);
            return true;  // USENは戻り値がvoidなので常にtrue
        },
        // 初期SFEN検出（デフォルト使用）
        KifuDetectSfenFunc(),
        // ゲーム情報抽出（USENはゲーム情報が限定的だが、専用メソッドを使用）
        [](const QString& path) {
            return UsenToSfenConverter::extractGameInfo(path);
        },
        // 変化のデバッグ出力あり
        true
    );
}

void KifuLoadCoordinator::loadUsiFromFile(const QString& filePath)
{
    loadKifuCommon(
        filePath,
        "loadUsiFromFile",
        // 解析関数
        [](const QString& path, KifParseResult& res, QString* warn) {
            UsiToSfenConverter::parseWithVariations(path, res, warn);
            return true;  // USIは戻り値がvoidなので常にtrue
        },
        // 初期SFEN検出（デフォルト使用）
        KifuDetectSfenFunc(),
        // ゲーム情報抽出（USIはゲーム情報を持たないため空）
        KifuExtractGameInfoFunc(),
        // 変化のデバッグ出力なし
        false
    );
}

// ★ 追加: 文字列から棋譜を読み込む（棋譜貼り付け機能用）
bool KifuLoadCoordinator::loadKifuFromString(const QString& content)
{
    if (content.trimmed().isEmpty()) {
        emit errorOccurred(tr("貼り付けるテキストが空です。"));
        return false;
    }

    qDebug().noquote() << "[PASTE] loadKifuFromString: content length =" << content.size();

    // 形式を自動判定
    // 判定基準:
    // - SFEN: 盤面/手番/持駒/手数 の形式 (例: "lnsgkgsnl/1r5b1/... b - 1")
    // - BOD: "後手の持駒" や "+---" で始まる局面図
    // - KIF: "手数----指手" や "   1 ７六歩" のような行がある
    // - KI2: "▲７六歩" や "△３四歩" で始まる行がある（手数行なし）
    // - CSA: "+" や "-" で始まる指し手行、または "V2" ヘッダ
    // - USI: "position" で始まる
    // - JKF: "{" で始まるJSON
    // - USEN: "~" を含む（USENエンコード）

    enum KifuFormat { FMT_UNKNOWN, FMT_KIF, FMT_KI2, FMT_CSA, FMT_USI, FMT_JKF, FMT_USEN, FMT_SFEN, FMT_BOD };
    KifuFormat fmt = FMT_UNKNOWN;

    const QString trimmed = content.trimmed();

    // SFEN判定（盤面パターン: 小文字/大文字の駒文字と数字、スラッシュ区切り）
    // 例: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    // または "sfen lnsgkgsnl/..." の形式
    static const QRegularExpression sfenPattern(
        QStringLiteral("^(sfen\\s+)?[lnsgkrpbLNSGKRPB1-9+]+(/[lnsgkrpbLNSGKRPB1-9+]+){8}\\s+[bw]\\s+[-\\w]+\\s+\\d+")
    );
    if (sfenPattern.match(trimmed).hasMatch()) {
        fmt = FMT_SFEN;
        qDebug().noquote() << "[PASTE] detected format: SFEN";
    }
    // BOD判定（局面図: "後手の持駒" や "+--" で始まる罫線）
    else if (trimmed.contains(QStringLiteral("後手の持駒")) ||
             trimmed.contains(QStringLiteral("先手の持駒")) ||
             trimmed.contains(QRegularExpression(QStringLiteral("^\\+[-─]+\\+"), QRegularExpression::MultilineOption)) ||
             trimmed.contains(QStringLiteral("|v")) ||
             trimmed.contains(QStringLiteral("| ・"))) {
        fmt = FMT_BOD;
        qDebug().noquote() << "[PASTE] detected format: BOD";
    }
    // JSON判定（JKF）
    else if (trimmed.startsWith(QLatin1Char('{'))) {
        fmt = FMT_JKF;
        qDebug().noquote() << "[PASTE] detected format: JKF (JSON)";
    }
    // USI判定
    else if (trimmed.startsWith(QLatin1String("position"))) {
        fmt = FMT_USI;
        qDebug().noquote() << "[PASTE] detected format: USI";
    }
    // USEN判定（チルダを含む）
    else if (trimmed.contains(QLatin1Char('~'))) {
        fmt = FMT_USEN;
        qDebug().noquote() << "[PASTE] detected format: USEN";
    }
    // CSA判定（V2ヘッダまたは +/- で始まる指し手行）
    else if (trimmed.startsWith(QLatin1String("V2")) ||
             trimmed.startsWith(QLatin1String("'")) ||
             QRegularExpression(QStringLiteral("^[+-][0-9]")).match(trimmed).hasMatch() ||
             content.contains(QRegularExpression(QStringLiteral("\\n[+-][0-9]")))) {
        fmt = FMT_CSA;
        qDebug().noquote() << "[PASTE] detected format: CSA";
    }
    // KIF判定（"手数----" ヘッダまたは数字で始まる行）
    else if (content.contains(QStringLiteral("手数----")) ||
             content.contains(QRegularExpression(QStringLiteral("^\\s*\\d+\\s+[０-９一二三四五六七八九同]")))) {
        fmt = FMT_KIF;
        qDebug().noquote() << "[PASTE] detected format: KIF";
    }
    // KI2判定（▲△で始まる指し手）
    else if (content.contains(QRegularExpression(QStringLiteral("[▲△][０-９一二三四五六七八九同]")))) {
        fmt = FMT_KI2;
        qDebug().noquote() << "[PASTE] detected format: KI2";
    }

    if (fmt == FMT_UNKNOWN) {
        // 最後にKIFとして試す
        fmt = FMT_KIF;
        qDebug().noquote() << "[PASTE] format unknown, trying KIF";
    }

    // SFEN形式の場合は直接処理
    if (fmt == FMT_SFEN) {
        return loadPositionFromSfen(trimmed);
    }

    // BOD形式の場合は直接処理
    if (fmt == FMT_BOD) {
        return loadPositionFromBod(content);
    }

    // 一時ファイルを作成して読み込み
    QString tempFilePath = QDir::tempPath() + QStringLiteral("/shogi_paste_temp");
    switch (fmt) {
        case FMT_KIF:  tempFilePath += QStringLiteral(".kif"); break;
        case FMT_KI2:  tempFilePath += QStringLiteral(".ki2"); break;
        case FMT_CSA:  tempFilePath += QStringLiteral(".csa"); break;
        case FMT_USI:  tempFilePath += QStringLiteral(".usi"); break;
        case FMT_JKF:  tempFilePath += QStringLiteral(".jkf"); break;
        case FMT_USEN: tempFilePath += QStringLiteral(".usen"); break;
        default:       tempFilePath += QStringLiteral(".kif"); break;
    }

    // 一時ファイルに書き込み
    QFile tempFile(tempFilePath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(tr("一時ファイルの作成に失敗しました。"));
        return false;
    }
    QTextStream out(&tempFile);
    out.setEncoding(QStringConverter::Utf8);
    out << content;

    // 書き込みステータスを確認
    out.flush();
    if (out.status() != QTextStream::Ok) {
        emit errorOccurred(tr("一時ファイルへの書き込みに失敗しました。"));
        tempFile.close();
        return false;
    }
    tempFile.close();

    qDebug().noquote() << "[PASTE] created temp file:" << tempFilePath;

    // 形式に応じた読み込み関数を呼び出し
    switch (fmt) {
        case FMT_KIF:
            loadKifuFromFile(tempFilePath);
            break;
        case FMT_KI2:
            loadKi2FromFile(tempFilePath);
            break;
        case FMT_CSA:
            loadCsaFromFile(tempFilePath);
            break;
        case FMT_USI:
            loadUsiFromFile(tempFilePath);
            break;
        case FMT_JKF:
            loadJkfFromFile(tempFilePath);
            break;
        case FMT_USEN:
            loadUsenFromFile(tempFilePath);
            break;
        default:
            loadKifuFromFile(tempFilePath);
            break;
    }

    // 一時ファイルを削除
    QFile::remove(tempFilePath);
    qDebug().noquote() << "[PASTE] removed temp file:" << tempFilePath;

    return true;
}

// ★ 追加: SFEN形式の局面を読み込む
bool KifuLoadCoordinator::loadPositionFromSfen(const QString& sfenStr)
{
    qDebug().noquote() << "[PASTE] loadPositionFromSfen:" << sfenStr;

    QString sfen = sfenStr.trimmed();
    
    // "sfen " プレフィックスがあれば除去
    if (sfen.startsWith(QStringLiteral("sfen "), Qt::CaseInsensitive)) {
        sfen = sfen.mid(5).trimmed();
    }

    // SFEN文字列の検証（最低限4つの部分が必要）
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() < 4) {
        emit errorOccurred(tr("無効なSFEN形式です。"));
        return false;
    }

    // 局面をセットアップ
    m_loadingKifu = true;

    // sfenRecordをクリアして初期局面をセット
    if (m_sfenRecord) {
        m_sfenRecord->clear();
        m_sfenRecord->append(sfen);
    }

    // 表示用データを作成
    QList<KifDisplayItem> disp;
    KifDisplayItem startItem;
    startItem.ply = 0;
    startItem.prettyMove = QStringLiteral("=== 開始局面 ===");
    startItem.comment = QString();
    disp.append(startItem);

    // ★ KifuBranchTree をセットアップ
    if (m_branchTree == nullptr) {
        m_branchTree = new KifuBranchTree(this);
    } else {
        m_branchTree->clear();
    }
    m_branchTree->setRootSfen(sfen);

    // 各種インデックスをリセット
    if (m_navState != nullptr) {
        m_navState->goToRoot();
    }
    m_activePly = 0;
    m_currentSelectedPly = 0;
    m_currentMoveIndex = 0;

    // ゲーム情報をクリア
    if (m_gameInfoTable) {
        m_gameInfoTable->clearContents();
        m_gameInfoTable->setRowCount(0);
    }

    // シグナルを発行（displayGameRecordでモデルが更新される）
    emit setReplayMode(true);
    emit displayGameRecord(disp);
    qDebug() << "[KLC-DEBUG] emitting syncBoardAndHighlightsAtRow(0) from loadPositionFromSfen";
    emit syncBoardAndHighlightsAtRow(0);
    emit enableArrowButtons();

    m_loadingKifu = false;

    qDebug().noquote() << "[PASTE] loadPositionFromSfen: completed";
    return true;
}

// ★ 追加: BOD形式の局面を読み込む
bool KifuLoadCoordinator::loadPositionFromBod(const QString& bodStr)
{
    qDebug().noquote() << "[PASTE] loadPositionFromBod: length =" << bodStr.size();

    // BOD形式をSFEN形式に変換
    QString sfen;
    QString detectedLabel;
    QString warn;

    const QStringList lines = bodStr.split(QLatin1Char('\n'));
    
    if (!KifToSfenConverter::buildInitialSfenFromBod(lines, sfen, &detectedLabel, &warn)) {
        emit errorOccurred(tr("BOD形式の解析に失敗しました。%1").arg(warn));
        return false;
    }

    if (sfen.isEmpty()) {
        emit errorOccurred(tr("BOD形式から局面を取得できませんでした。"));
        return false;
    }

    qDebug().noquote() << "[PASTE] loadPositionFromBod: converted SFEN =" << sfen;

    // 手番情報を追加（BODから取得できなかった場合のデフォルト）
    // buildInitialSfenFromBodは盤面部分のみを返すので、手番と手数を補完
    if (!sfen.contains(QLatin1Char(' '))) {
        // 手番がない場合、BODテキストから判定
        QString turn = QStringLiteral("b");  // デフォルトは先手
        if (bodStr.contains(QStringLiteral("後手番"))) {
            turn = QStringLiteral("w");
        }
        sfen = QStringLiteral("%1 %2 - 1").arg(sfen, turn);
    }

    // SFEN読み込み処理に委譲
    return loadPositionFromSfen(sfen);
}

void KifuLoadCoordinator::applyParsedResultCommon(
    const QString& filePath,
    const QString& initialSfen,
    const QString& teaiLabel,
    const KifParseResult& res,
    const QString& parseWarn,
    const char* callerTag)
{
    // ★ パフォーマンス計測用
    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer stepTimer;
    auto logStep = [&](const char* stepName) {
        qDebug().noquote() << QStringLiteral("[PERF] %1: %2 ms").arg(stepName).arg(stepTimer.elapsed());
        stepTimer.restart();
    };
    stepTimer.start();

    // 本譜（表示／USI）
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    m_kifuUsiMoves = res.mainline.usiMoves;

    // 終局/中断判定（見た目文字列で簡易判定）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) {
            if (s.contains(kw)) return true;
        }
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    // disp[0]は開始局面エントリなので、指し手が1つもない場合は disp.size() <= 1
    if (m_kifuUsiMoves.isEmpty() && !hasTerminal && disp.size() <= 1) {
        const QString errorMessage =
            tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(filePath);
        emit errorOccurred(errorMessage);
        qDebug().noquote() << "[MAIN]" << callerTag << "OUT (no moves)";
        m_loadingKifu = false; // 早期return時も必ず解除
        return;
    }

    // 3) 本譜の SFEN 列と m_gameMoves を再構築
    qDebug().noquote() << "[KLC] applyParsedResultCommon: calling rebuildSfenRecord"
                       << "initialSfen=" << initialSfen.left(60)
                       << "usiMoves.size=" << m_kifuUsiMoves.size()
                       << "hasTerminal=" << hasTerminal;
    rebuildSfenRecord(initialSfen, m_kifuUsiMoves, hasTerminal);
    qDebug().noquote() << "[KLC] applyParsedResultCommon: after rebuildSfenRecord"
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                       << "m_sfenRecord.size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        qDebug().noquote() << "[KLC] m_sfenRecord[0]=" << m_sfenRecord->first().left(60);
        if (m_sfenRecord->size() > 1) {
            qDebug().noquote() << "[KLC] m_sfenRecord[1]=" << m_sfenRecord->at(1).left(60);
        }
        if (m_sfenRecord->size() > 2) {
            qDebug().noquote() << "[KLC] m_sfenRecord[last]=" << m_sfenRecord->last().left(60);
        }
    }
    logStep("rebuildSfenRecord");
    rebuildGameMoves(initialSfen, m_kifuUsiMoves);
    logStep("rebuildGameMoves");

    // 3.5) USI position コマンド列を構築（0..N）
    //     initialSfen は prepareInitialSfen() が返す手合い込みの SFEN
    //     m_kifuUsiMoves は 1..N の USI 文字列（"7g7f" 等）
    m_positionStrList.clear();
    m_positionStrList.reserve(m_kifuUsiMoves.size() + 1);

    const QString base = QStringLiteral("position sfen %1").arg(initialSfen);
    m_positionStrList.push_back(base);  // 0手目：moves なし

    QStringList acc; // 先頭からの累積
    acc.reserve(m_kifuUsiMoves.size());
    for (qsizetype i = 0; i < m_kifuUsiMoves.size(); ++i) {
        acc.push_back(m_kifuUsiMoves.at(i));
        // i+1 手目：先頭から i+1 個の moves を連結
        m_positionStrList.push_back(base + QStringLiteral(" moves ") + acc.join(' '));
    }

    // （任意）ログで確認
    qDebug().noquote() << "[USI] position list built. count=" << m_positionStrList.size();
    if (!m_positionStrList.isEmpty()) {
        qDebug().noquote() << "[USI] pos[0]=" << m_positionStrList.first();
        if (m_positionStrList.size() > 1) {
            qDebug().noquote() << "[USI] pos[1]=" << m_positionStrList.at(1);
        }
    }

    logStep("buildPositionStrList");

    // 4) 棋譜表示へ反映（本譜）
    emit displayGameRecord(disp);
    logStep("displayGameRecord");

    // 5) 本譜スナップショットを保持（以降の解決・描画に使用）
    m_dispMain = disp;          // 表示列（1..N）
    m_sfenMain = *m_sfenRecord; // 0..N の局面列
    m_gmMain   = m_gameMoves;   // 1..N のUSIムーブ

    // 6) 変化を取りまとめ（KifuBranchTreeBuilder でツリー構築に使用）
    m_variationsByPly.clear();
    m_variationsSeq.clear();
    for (const KifVariation& kv : std::as_const(res.variations)) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;         // “その変化が始まる絶対手数（1-origin）”
        if (L.disp.isEmpty()) continue;
        m_variationsByPly[L.startPly].push_back(L);
        m_variationsSeq.push_back(L);     // 入力順（KIF/CSA出現順）を保持
    }

    // 7) 棋譜テーブルの初期選択（開始局面を選択）
    if (m_recordPane && m_recordPane->kifuView()) {
        QTableView* view = m_recordPane->kifuView();
        if (view->model() && view->model()->rowCount() > 0) {
            const QModelIndex idx0 = view->model()->index(0, 0);
            if (view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollTo(idx0, QAbstractItemView::PositionAtTop);
        }
    }

    // 8) 初期状態を適用
    if (m_navState != nullptr) {
        m_navState->goToRoot();
    }
    m_activePly         = 0;

    // 9) UIの整合
    emit enableArrowButtons();
    logImportSummary(filePath, m_kifuUsiMoves, disp, teaiLabel, parseWarn, QString());

    // 10) KifuBranchTree を構築
    if (m_branchTree != nullptr) {
        // KifuBranchTreeBuilder を使用してツリーを構築
        KifuBranchTreeBuilder::buildFromKifParseResult(m_branchTree, res, initialSfen);
        qDebug().noquote() << "[KLC] KifuBranchTree built: nodeCount=" << m_branchTree->nodeCount()
                           << "lineCount=" << m_branchTree->lineCount();
        emit branchTreeBuilt();
    }
    logStep("buildKifuBranchTree");

    // ★ 追加：読み込み直後に "+付与 & 行着色" をまとめて反映（Main 行が表示中）
    applyBranchMarksForCurrentLine();
    logStep("applyBranchMarksForCurrentLine");

    // 12) 分岐ツリーへ供給
    if (m_analysisTab && m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        QVector<BranchLine> lines = m_branchTree->allLines();
        rows.reserve(lines.size());

        for (int i = 0; i < lines.size(); ++i) {
            const BranchLine& line = lines.at(i);
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = line.branchPly;
            x.parent = (line.branchPoint != nullptr)
                           ? m_branchTree->findLineIndexForNode(line.branchPoint)
                           : -1;

            // disp, sfen を BranchLine の nodes から構築
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                item.prettyMove = node->displayText();
                item.comment = node->comment();
                item.timeText = node->timeText();
                item.ply = node->ply();
                x.disp.append(item);
                x.sfen.append(node->sfen());
            }

            rows.push_back(std::move(x));
        }

        m_analysisTab->setBranchTreeRows(rows);
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }
    logStep("setBranchTreeRows");

    if (m_kifuBranchModel) {
        // 起動直後は候補を出さない（0手目）：モデルクリア＆ビュー非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view =
            m_recordPane ? m_recordPane->branchView() : nullptr) {
            view->setVisible(true);
            view->setEnabled(false);
        }
    }

    // ★ ロード完了 → 抑止解除
    m_loadingKifu = false;

    qDebug().noquote() << QStringLiteral("[PERF] applyParsedResultCommon TOTAL: %1 ms").arg(totalTimer.elapsed());
    qDebug().noquote() << "[MAIN]" << callerTag << "OUT";
}

QString KifuLoadCoordinator::prepareInitialSfen(const QString& filePath, QString& teaiLabel) const
{
    const QString sfen = KifToSfenConverter::detectInitialSfenFromFile(filePath, &teaiLabel);
    return sfen.isEmpty()
               ? QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
               : sfen;
}

void KifuLoadCoordinator::populateGameInfo(const QList<KifGameInfoItem>& items)
{
    // ★ nullチェック: m_gameInfoTableがnullptrの場合は処理をスキップ
    if (!m_gameInfoTable) {
        qWarning().noquote() << "[KifuLoadCoordinator] populateGameInfo: m_gameInfoTable is null, skipping table update";
        // ★ 追加: 元の対局情報を保存するためのシグナルは発行する
        emit gameInfoPopulated(items);
        return;
    }

    // ★ セル変更シグナルを一時的にブロック
    m_gameInfoTable->blockSignals(true);
    
    m_gameInfoTable->clearContents();
    m_gameInfoTable->setRowCount(static_cast<int>(items.size()));

    for (int row = 0; row < static_cast<int>(items.size()); ++row) {
        const auto& it = items.at(row);
        auto *keyItem   = new QTableWidgetItem(it.key);
        auto *valueItem = new QTableWidgetItem(it.value);
        // ★ 修正: 項目名は編集不可、内容は編集可能
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        // valueItemはデフォルトで編集可能（フラグを変更しない）
        m_gameInfoTable->setItem(row, 0, keyItem);
        m_gameInfoTable->setItem(row, 1, valueItem);
    }

    m_gameInfoTable->resizeColumnToContents(0);
    
    // ★ シグナルを再開
    m_gameInfoTable->blockSignals(false);

    // まだタブに載ってなければ、このタイミングで追加しておくと確実
    addGameInfoTabIfMissing();
    
    // ★ 追加: 元の対局情報を保存するためのシグナルを発行
    emit gameInfoPopulated(items);
}

void KifuLoadCoordinator::addGameInfoTabIfMissing()
{
    if (!m_tab) return;
    
    // ★ 追加: m_gameInfoTableがnullの場合は処理をスキップ
    if (!m_gameInfoTable) {
        qDebug().noquote() << "[KifuLoadCoordinator] addGameInfoTabIfMissing: m_gameInfoTable is null, skipping";
        return;
    }

    // Dock で表示していたら解除
    if (m_gameInfoDock && m_gameInfoDock->widget() == m_gameInfoTable) {
        m_gameInfoDock->setWidget(nullptr);
        m_gameInfoDock->deleteLater();
        m_gameInfoDock = nullptr;
    }

    // ★ 修正: テーブルの親ウィジェット（コンテナ）を取得
    // MainWindowで m_gameInfoContainer に m_gameInfoTable が配置されている
    QWidget* widgetToAdd = m_gameInfoTable;
    if (m_gameInfoTable && m_gameInfoTable->parentWidget()) {
        QWidget* parent = m_gameInfoTable->parentWidget();
        // 親がQTabWidgetでなければ、それがコンテナ
        if (!qobject_cast<QTabWidget*>(parent)) {
            widgetToAdd = parent;
        }
    }

    // ★ 修正: 既に「対局情報」タブがあるか、同じウィジェットがタブに含まれているか確認
    // ウィジェットで検索
    if (m_tab->indexOf(widgetToAdd) != -1) {
        // 既にタブに含まれている場合は何もしない
        return;
    }

    // タブタイトルで「対局情報」を検索
    for (int i = 0; i < m_tab->count(); ++i) {
        if (m_tab->tabText(i) == tr("対局情報")) {
            // 既存タブを削除してwidgetToAddで置き換え
            m_tab->removeTab(i);
            m_tab->insertTab(i, widgetToAdd, tr("対局情報"));
            return;
        }
    }

    // タブがない場合のみ追加
    int anchorIdx = -1;

    // 1) EngineAnalysisTab（検討タブ）の直後に入れる
    if (m_analysisTab)
        anchorIdx = m_tab->indexOf(m_analysisTab);

    // 2) 念のため、タブタイトルで「コメント/Comments」を探してその直後に入れるフォールバック
    if (anchorIdx < 0) {
        for (int i = 0; i < m_tab->count(); ++i) {
            const QString t = m_tab->tabText(i);
            if (t.contains(tr("コメント")) || t.contains("Comments", Qt::CaseInsensitive)) {
                anchorIdx = i;  // NOLINT(clang-analyzer-deadcode.DeadStores) - 将来の拡張用
                break;
            }
        }
    }

    // ★ 対局情報タブは最初（インデックス0）に挿入する
    m_tab->insertTab(0, widgetToAdd, tr("対局情報"));
}

QString KifuLoadCoordinator::findGameInfoValue(const QList<KifGameInfoItem>& items,
                                      const QStringList& keys) const
{
    for (const auto& it : items) {
        // KifGameInfoItem.key は「先手」「後手」等（末尾コロンは normalize 済み）
        if (keys.contains(it.key)) {
            const QString v = it.value.trimmed();
            if (!v.isEmpty()) return v;
        }
    }
    return QString();
}

void KifuLoadCoordinator::applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items)
{
    // 優先度：
    // 1) 先手／後手
    // 2) 下手／上手（→ 下手=Black, 上手=White）
    // 3) 先手省略名／後手省略名
    QString black = findGameInfoValue(items, { QStringLiteral("先手") });
    QString white = findGameInfoValue(items, { QStringLiteral("後手") });

    // 下手/上手にも対応（必要な側だけ補完）
    const QString shitate = findGameInfoValue(items, { QStringLiteral("下手") });
    const QString uwate   = findGameInfoValue(items, { QStringLiteral("上手") });

    if (black.isEmpty() && !shitate.isEmpty())
        black = shitate;                   // 下手 → Black
    if (white.isEmpty() && !uwate.isEmpty())
        white = uwate;                     // 上手 → White

    // 省略名でのフォールバック
    if (black.isEmpty())
        black = findGameInfoValue(items, { QStringLiteral("先手省略名") });
    if (white.isEmpty())
        white = findGameInfoValue(items, { QStringLiteral("後手省略名") });

    // 取得できた方だけ反映（既存表示を尊重）
    if (!black.isEmpty() && m_shogiView)
        m_shogiView->setBlackPlayerName(black);
    if (!white.isEmpty() && m_shogiView)
        m_shogiView->setWhitePlayerName(white);
}

void KifuLoadCoordinator::rebuildSfenRecord(const QString& initialSfen,
                                            const QStringList& usiMoves,
                                            bool hasTerminal)
{
    qDebug().noquote() << "[KLC] rebuildSfenRecord ENTER"
                       << "initialSfen=" << initialSfen.left(60)
                       << "usiMoves.size=" << usiMoves.size()
                       << "hasTerminal=" << hasTerminal
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord);

    const QStringList list = SfenPositionTracer::buildSfenRecord(initialSfen, usiMoves, hasTerminal);

    qDebug().noquote() << "[KLC] rebuildSfenRecord: built list.size=" << list.size();
    if (!list.isEmpty()) {
        qDebug().noquote() << "[KLC] rebuildSfenRecord: head[0]=" << list.first().left(60);
        if (list.size() > 1) {
            qDebug().noquote() << "[KLC] rebuildSfenRecord: tail[last]=" << list.last().left(60);
        }
    }

    if (!m_sfenRecord) {
        qWarning() << "[KLC] rebuildSfenRecord: m_sfenRecord was NULL! Creating new QStringList.";
        m_sfenRecord = new QStringList;
    }
    *m_sfenRecord = list; // COW

    qDebug().noquote() << "[KLC] rebuildSfenRecord LEAVE"
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                       << "m_sfenRecord->size=" << m_sfenRecord->size();
}

void KifuLoadCoordinator::rebuildGameMoves(const QString& initialSfen,
                                           const QStringList& usiMoves)
{
    m_gameMoves = SfenPositionTracer::buildGameMoves(initialSfen, usiMoves);
}

// ====== アクティブ行に対する「分岐あり手」の再計算 → ビュー再描画 ======
void KifuLoadCoordinator::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    // まずビュー参照を取得（nullでも安全に抜ける）
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    // KifuBranchTree から分岐点を取得
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        const int nLines = static_cast<int>(lines.size());
        const int currentLineIdx = (m_navState != nullptr) ? m_navState->currentLineIndex() : 0;
        const int active = (nLines == 0) ? 0 : qBound(0, currentLineIdx, nLines - 1);

        if (active >= 0 && active < nLines) {
            const BranchLine& line = lines.at(active);
            m_branchablePlySet = m_branchTree->branchablePlysOnLine(line);
        }
    }

    // デリゲート装着（未装着ならここで装着）
    ensureBranchRowDelegateInstalled();

    // 再描画
    if (view && view->viewport()) view->viewport()->update();
}

void KifuLoadCoordinator::ensureBranchRowDelegateInstalled()
{
    // RecordPane から棋譜ビューを取得
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view) return;

    if (!m_branchRowDelegate) {
        // デリゲートをビューの子として作成し、ビューに適用
        m_branchRowDelegate = new BranchRowDelegate(view);
        view->setItemDelegate(m_branchRowDelegate);
    } else {
        // 念のため親が違う場合は付け替え
        if (m_branchRowDelegate->parent() != view) {
            m_branchRowDelegate->setParent(view);
            view->setItemDelegate(m_branchRowDelegate);
        }
    }

    // 「分岐あり」マーカーの集合をデリゲートへ渡す
    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}

KifuLoadCoordinator::BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

KifuLoadCoordinator::BranchRowDelegate::~BranchRowDelegate() = default;

void KifuLoadCoordinator::BranchRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QStyledItemDelegate::initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    // 選択時のハイライトと衝突しないように、未選択時のみオレンジ背景
    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, QColor(255, 220, 160));
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

void KifuLoadCoordinator::logImportSummary(const QString& filePath,
                                  const QStringList& usiMoves,
                                  const QList<KifDisplayItem>& disp,
                                  const QString& teaiLabel,
                                  const QString& warnParse,
                                  const QString& warnConvert) const
{
    if (!warnParse.isEmpty())
        qWarning().noquote() << "[KIF parse warnings]\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qWarning().noquote() << "[KIF convert warnings]\n" << warnConvert.trimmed();

    qDebug().noquote() << QStringLiteral("KIF読込: %1手（%2）")
                              .arg(usiMoves.size())
                              .arg(QFileInfo(filePath).fileName());
    for (qsizetype i = 0; i < qMin(qsizetype(5), usiMoves.size()); ++i) {
        qDebug().noquote() << QStringLiteral("USI[%1]: %2")
        .arg(i + 1)
            .arg(usiMoves.at(i));
    }

    qDebug().noquote() << QStringLiteral("手合割: %1")
                              .arg(teaiLabel.isEmpty()
                                       ? QStringLiteral("平手(既定)")
                                       : teaiLabel);

    // 本譜（表示用）。コメントがあれば直後に出力。
    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
        ? QStringLiteral("00:00/00:00:00")
        : it.timeText;
        qDebug().noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qDebug().noquote() << QStringLiteral("  └ コメント: %1")
                                      .arg(it.comment.trimmed());
        }
    }

    // SFEN（抜粋）
    if (m_sfenRecord) {
        for (int i = 0; i < qMin(12, m_sfenRecord->size()); ++i) {
            qDebug().noquote() << QStringLiteral("%1) %2")
            .arg(i)
                .arg(m_sfenRecord->at(i));
        }
    }

    // m_gameMoves（従来通り）
    qDebug() << "m_gameMoves size:" << m_gameMoves.size();
    for (qsizetype i = 0; i < m_gameMoves.size(); ++i) {
        qDebug().noquote() << QString("%1) ").arg(i + 1) << m_gameMoves[i];
    }
}

void KifuLoadCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}



void KifuLoadCoordinator::applyBranchMarksForCurrentLine()
{
    if (!m_kifuRecordModel) return;

    QSet<int> marks; // ply1=1..N（モデルの行番号と一致。0は開始局面なので除外）

    const int currentLineIdx = (m_navState != nullptr) ? m_navState->currentLineIndex() : 0;
    qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: currentLineIndex=" << currentLineIdx;

    // KifuBranchTree から分岐点を取得
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        const int nLines = static_cast<int>(lines.size());
        const int active = (nLines == 0) ? 0 : qBound(0, currentLineIdx, nLines - 1);
        if (active >= 0 && active < nLines) {
            const BranchLine& line = lines.at(active);
            marks = m_branchTree->branchablePlysOnLine(line);
        }
        qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: marks=" << marks;
    } else {
        // KifuBranchTree がない場合は空のマークを設定して終了
        qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: no BranchTree available";
    }

    m_kifuRecordModel->setBranchPlyMarks(marks);
}

// ★ 追加：分岐コンテキストをリセット（対局終了時に使用）
void KifuLoadCoordinator::resetBranchContext()
{
    m_branchPlyContext = -1;
    // 本譜行に戻す
    if (m_navState != nullptr) {
        m_navState->goToRoot();
        m_navState->resetPreferredLineIndex();
    }
}

// ★ 追加：分岐ツリーを完全リセット（新規対局開始時に使用）
void KifuLoadCoordinator::resetBranchTreeForNewGame()
{
    qDebug().noquote() << "[KLC] resetBranchTreeForNewGame: clearing all branch data";

    // 1) 分岐コンテキストを完全リセット
    m_branchPlyContext = -1;

    // ★ ツリーをクリアする前にcurrentNodeをnullに設定
    // setRootSfen()がtreeChangedシグナルを同期的に発火するため、
    // 旧ノード削除後にダングリングポインタで不正メモリアクセスが発生するのを防止
    if (m_navState != nullptr) {
        m_navState->setCurrentNode(nullptr);
        m_navState->resetPreferredLineIndex();
    }

    // ★ 新システム: KifuBranchTree を初期化（開始局面で再作成）
    if (m_branchTree == nullptr) {
        m_branchTree = new KifuBranchTree(this);
    } else {
        m_branchTree->clear();
    }
    // 平手初期局面でルートノードを作成
    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    m_branchTree->setRootSfen(kHirateStartSfen);

    // ★ ツリー再構築後にナビゲーション状態を新しいルートに設定
    if (m_navState != nullptr) {
        m_navState->goToRoot();
    }

    // 2) 分岐候補インデックスをクリア
    m_branchablePlySet.clear();

    // 3) 分岐ツリーUIをクリア
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> emptyRows;
        m_analysisTab->setBranchTreeRows(emptyRows);
    }

    qDebug().noquote() << "[KLC] resetBranchTreeForNewGame: done";
}

