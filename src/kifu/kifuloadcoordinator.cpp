#include "kifuloadcoordinator.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"
#include "recordpane.h"              // これまで通り（前方宣言側）
#include "kifurecordlistmodel.h"     // ← 実体定義（必須）
#include "kifubranchlistmodel.h"     // ← 実体定義（必須）
#include "branchdisplayplan.h"
#include "navigationpresenter.h"
#include "engineanalysistab.h"
#include "kifuvariationengine.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"

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

using BCDI = ::BranchCandidateDisplayItem;

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                                         QStringList& positionStrList,
                                         int& activeResolvedRow,
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
                                         QHash<int, QMap<int, ::BranchCandidateDisplay>>& branchDisplayPlan,
                                         QObject* parent)
    : QObject(parent)
    , m_gameInfoTable(gameInfoTable)
    , m_gameInfoDock(gameInfoDock)
    , m_tab(tab)
    , m_sfenRecord(sfenRecord)
    , m_gameMoves(gameMoves)                // ← 参照メンバに束縛（同一実体を共有）
    , m_positionStrList(positionStrList)    // ← 同上
    , m_recordPane(recordPane)
    , m_activeResolvedRow(activeResolvedRow)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
    , m_branchDisplayPlan(branchDisplayPlan)
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

    // ★ ロード中フラグ（applyResolvedRowAndSelect 等の分岐更新を抑止）
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
        m_loadingKifu = false;
        return;
    }
    if (!parseWarn.isEmpty()) {
        qWarning().noquote() << "[GM] parse warn:" << parseWarn;
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
    m_activeResolvedRow = 0;
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
    m_activeResolvedRow = 0;
    m_activePly         = 0;

    // apply 内では m_loadingKifu を見て分岐候補の更新を抑止
    applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);
    logStep("applyResolvedRowAndSelect");

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

    // 11) Plan 構築（Plan方式の基礎データ）
    buildBranchCandidateDisplayPlan();
    logStep("buildBranchCandidateDisplayPlan");

    // ★ 追加：読み込み直後に "+付与 & 行着色" をまとめて反映（Main 行が表示中）
    applyBranchMarksForCurrentLine();
    logStep("applyBranchMarksForCurrentLine");

    // 12) 分岐ツリーへ供給（黄色ハイライトは applyResolvedRowAndSelect 内で同期）
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

    // 13) VariationEngine への投入（他機能で必要な可能性があるため保持）
    if (!m_varEngine) {
        m_varEngine = std::make_unique<KifuVariationEngine>();
    }
    {
        QVector<UsiMove> usiMain;
        usiMain.reserve(m_kifuUsiMoves.size());
        for (const auto& u : std::as_const(m_kifuUsiMoves)) {
            usiMain.push_back(UsiMove(u));
        }
        m_varEngine->ingest(res, m_sfenMain, usiMain, m_dispMain);
    }
    logStep("varEngine->ingest");

    // 14) （Plan方式化に伴い）WL 構築や従来の候補再計算は廃止
    // 15) ブランチ候補ワイヤリング（planActivated -> applyResolvedRowAndSelect）
    emit setupBranchCandidatesWiring(); // 内部で planActivated の connect を済ませる

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

    // 16) ツリーは読み込み後ロック（ユーザ操作で枝を生やさない）
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked after load";

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

// 現在表示用の棋譜列（disp）を使ってモデルを再構成し、selectPly 行を選択・同期する
void KifuLoadCoordinator::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // いま表示中の棋譜列を保持（分岐⇄本譜の復帰で再利用）
    m_dispCurrent = disp;

    // （既存）モデルへ反映：ここで displayGameRecord(disp) が呼ばれ、
    // その過程で m_currentMoveIndex が 0 に戻る実装になっている
    displayGameRecord(disp);

    // ★ 追加：現在表示中の行に対する「分岐あり手」マーキングをモデルへ反映
    applyBranchMarksForCurrentLine();

    // ★ RecordPane 内のビューを使う
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view || !view->model()) return;

    // 行数（0 は「=== 開始局面 ===」、1..N が各手）
    const int rc  = view->model()->rowCount();
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    // 0列目インデックス（選択は行単位）
    const QModelIndex idx = view->model()->index(row, 0);
    if (auto* sel = view->selectionModel()) {
        sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->setCurrentIndex(idx);
    }
    view->scrollTo(idx, QAbstractItemView::PositionAtCenter);

    // ★ 追加：棋譜欄のハイライト行を更新
    if (m_kifuRecordModel) {
        m_kifuRecordModel->setCurrentHighlightRow(row);
    }

    // displayGameRecord() が 0 に戻した “現在の手数” を、選択行へ復元
    m_currentSelectedPly = row;
    m_currentMoveIndex   = row;

    // 盤面・ハイライトも現在手に同期
    qDebug() << "[KLC-DEBUG] emitting syncBoardAndHighlightsAtRow(" << row << ") from applyResolvedRowAndSelect";
    emit syncBoardAndHighlightsAtRow(row);
}

void KifuLoadCoordinator::showBranchCandidatesFromPlan(int row, int ply1)
{
    Q_UNUSED(row)
    Q_UNUSED(ply1)
    // 分岐候補の表示は KifuDisplayCoordinator が管理する
    // この関数は互換性のために残すが、何もしない
    qDebug().noquote() << "[KLC-DEBUG] showBranchCandidatesFromPlan: skipped"
                       << "(KifuDisplayCoordinator manages branch candidates)";
}

void KifuLoadCoordinator::onBackToMainButtonClicked()
{
    // 本譜は常に行0
    const int mainRow = 0;
    const int safePly = (m_branchPlyContext < 0) ? 0 : m_branchPlyContext;

    // 「直前の行」を本譜(=0行)として扱わせる
    m_activeResolvedRow = mainRow;

    applyResolvedRowAndSelect(mainRow, safePly);
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
        const int active = (nLines == 0) ? 0 : qBound(0, m_activeResolvedRow, nLines - 1);

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

void KifuLoadCoordinator::buildBranchCandidateDisplayPlan()
{
    m_branchDisplayPlan.clear();

    // 分岐候補の表示は KifuDisplayCoordinator が管理する
    // KifuBranchTree を使用しない旧システムは削除済み
    qDebug().noquote() << "[KLC] buildBranchCandidateDisplayPlan: skipped (KifuDisplayCoordinator manages branch candidates)";
}

void KifuLoadCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    // 既存の接続を解除（重複接続/古いタブへのぶら下がり防止）
    if (m_analysisTab) {
        QObject::disconnect(
            m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this,         &KifuLoadCoordinator::applyResolvedRowAndSelect
            );
    }

    m_analysisTab = tab;

    // 新しいタブに配線（「分岐ツリーのクリック → 行適用＆選択」）
    if (m_analysisTab) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this,          &KifuLoadCoordinator::applyResolvedRowAndSelect,
            Qt::UniqueConnection
            );
    }

    // 既存の Presenter 依存も更新（従来どおり）
    if (m_navPresenter) {
        NavigationPresenter::Deps d;
        d.coordinator = this;
        d.analysisTab = m_analysisTab;
        m_navPresenter->setDeps(d);
    }
}

void KifuLoadCoordinator::ensureNavigationPresenter()
{
    if (m_navPresenter) return;

    NavigationPresenter::Deps d;
    d.coordinator = this;
    d.analysisTab = m_analysisTab;

    m_navPresenter = new NavigationPresenter(d, this);

    // 必要なら通知を拾ってログや別UIに伝播
    // connect(m_navPresenter, &NavigationPresenter::branchUiUpdated,
    //         this, &KifuLoadCoordinator::onBranchUiUpdated); // ※スロット用意時
}

void KifuLoadCoordinator::applyResolvedRowAndSelect(int row, int selPly)
{
    // ★★★ 新システム: KifuBranchTree を優先使用 ★★★
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        const int nLines = m_branchTree->lineCount();
        qDebug().noquote()
            << "[KLC] applyResolvedRowAndSelect enter (new system)"
            << "row=" << row << "selPly=" << selPly
            << " lineCount=" << nLines
            << " recPtr=" << static_cast<const void*>(m_sfenRecord)
            << " recSize(before)=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

        // 引数の安全化
        int safeRow = (row < 0) ? 0 : ((row >= nLines) ? nLines - 1 : row);
        int safePly = (selPly < 0) ? 0 : selPly;

        // Sticky 分岐ロジック（新システム版）
        // 直前に分岐ライン（lineIndex > 0）を表示していた場合、
        // 分岐点より前の手数を要求しても分岐ラインを維持する
        QVector<BranchLine> lines = m_branchTree->allLines();
        const int prevRow = (m_activeResolvedRow < 0)
                                ? 0
                                : ((m_activeResolvedRow >= nLines) ? nLines - 1 : m_activeResolvedRow);

        if (safeRow == 0 && prevRow > 0 && prevRow < lines.size()) {
            const BranchLine& prevLine = lines.at(prevRow);
            const int branchPly = prevLine.branchPly;
            if (branchPly > 0 && safePly < branchPly) {
                qDebug() << "[KLC][sticky-new] keep variation line instead of main:"
                         << "prevRow=" << prevRow << "branchPly=" << branchPly
                         << "requested ply=" << safePly;
                safeRow = prevRow;
            }
        }

        // 状態更新
        m_activeResolvedRow = safeRow;
        m_activePly = safePly;

        // KifuBranchTree からデータ取得
        QList<KifDisplayItem> treeDisp = m_branchTree->getDisplayItemsForLine(safeRow);
        QStringList treeSfen = m_branchTree->getSfenListForLine(safeRow);
        QVector<ShogiMove> treeMoves = m_branchTree->getGameMovesForLine(safeRow);

        if (!treeDisp.isEmpty()) {
            m_dispCurrent = treeDisp;
            qDebug().noquote() << "[KLC] display items from BranchTree  size=" << m_dispCurrent.size();
        }

        if (m_sfenRecord && !treeSfen.isEmpty()) {
            *m_sfenRecord = treeSfen;
            qDebug().noquote() << "[KLC] SFEN from BranchTree  size=" << m_sfenRecord->size();
        }

        if (!treeMoves.isEmpty()) {
            m_gameMoves = treeMoves;
            qDebug().noquote() << "[KLC] game moves from BranchTree  size=" << m_gameMoves.size();
        }

        // 棋譜テーブルへ反映
        showRecordAtPly(m_dispCurrent, safePly);
        updateKifuBranchMarkersForActiveRow();
        m_loadingKifu = false;

        ensureNavigationPresenter();
        m_navPresenter->refreshAll(safeRow, safePly);

        qDebug().noquote()
            << "[KLC] applyResolvedRowAndSelect leave (new system)"
            << " recSize(after)=" << (m_sfenRecord ? m_sfenRecord->size() : -1);
        return;
    }

    // ★ KifuBranchTree が存在しない場合は早期リターン
    qDebug().noquote() << "[KLC] applyResolvedRowAndSelect: no BranchTree available, returning";
}

// 既存：分岐候補モデルの構築・表示更新を担う関数
void KifuLoadCoordinator::showBranchCandidates(int row, int ply)
{
    // 新システムが有効な場合は完全にスキップ
    // m_branchTree が存在する場合も新システムを使用する（フラグが設定されていなくても）
    qDebug().noquote() << "[KLC] showBranchCandidates: m_useNewBranchSystem=" << m_useNewBranchSystem
                       << "branchTree=" << (m_branchTree ? "exists" : "null");
    if (m_useNewBranchSystem || m_branchTree != nullptr) {
        qDebug().noquote() << "[KLC] showBranchCandidates: skipped (new system active)";
        return;
    }

    // 再入防止：更新中に再度呼ばれた場合はスキップ
    if (m_updatingBranchCandidates) {
        return;
    }
    m_updatingBranchCandidates = true;

    // Plan ベースの表示へ一本化
    showBranchCandidatesFromPlan(row, ply);

    // 表示更新後にツリーハイライトだけ通知（逆呼びになるため refreshAll は呼ばない）
    ensureNavigationPresenter();
    m_navPresenter->updateAfterBranchListChanged(row, ply);

    m_updatingBranchCandidates = false;
}

void KifuLoadCoordinator::onMainMoveRowChanged(int selPly)
{
    // selPly を安全化
    const int safePly = qMax(0, selPly);

    // 読み込み中はスキップ（再帰的な更新抑止）
    if (m_loadingKifu) return;

    // KifuBranchTree が必須
    if (m_branchTree == nullptr || m_branchTree->isEmpty()) {
        qDebug().noquote() << "[KLC] onMainMoveRowChanged: no BranchTree available";
        return;
    }

    // KifuBranchTree からライン数を取得
    const int nLines = m_branchTree->lineCount();
    const int row = (nLines == 0) ? 0 : qBound(0, m_activeResolvedRow, nLines - 1);

    // 盤/棋譜/ハイライトは applyResolvedRowAndSelect に一任
    applyResolvedRowAndSelect(row, safePly);
}

// ===== KifuLoadCoordinator.cpp: ライブ対局から分岐ツリーを更新 =====
QList<KifDisplayItem> KifuLoadCoordinator::collectDispFromRecordModel() const
{
    QList<KifDisplayItem> disp;

    if (!m_kifuRecordModel) {
        qDebug().noquote() << "[KLC-DEBUG] collectDispFromRecordModel_: m_kifuRecordModel is null";
        return disp;
    }

    const int rows = m_kifuRecordModel->rowCount();
    qDebug().noquote() << "[KLC-DEBUG] collectDispFromRecordModel_: rows=" << rows;

    if (rows <= 1) {
        qDebug().noquote() << "[KLC-DEBUG] collectDispFromRecordModel_: rows<=1, returning empty";
        return disp; // 0 行目は「=== 開始局面 ===」
    }

    // ★ 修正: 開始局面エントリを先頭に追加（rebuildBranchTree が disp[0] をスキップするため）
    // disp[0] = 開始局面（prettyMoveが空）、disp[1] = 1手目、disp[2] = 2手目...
    disp.push_back(KifDisplayItem(QString(), QString(), QString(), 0));

    // 「ASCII数字(1..,23..)+空白任意+ (▲|△)」の時だけ、その数字部分を剥がす。
    // 例: "1 ▲７六歩" / "1▲７六歩" / "23▲..." / "23 ▲..." は除去対象。
    // 例: "▲７六歩" / "△７六歩" / "７六歩"（先頭が全角数字）は対象外。
    static const QRegularExpression kDupMoveNoPattern(
        QStringLiteral("^\\s*([0-9]+)\\s*(?=[▲△])")
        );

    for (int r = 1; r < rows; ++r) {
        const QModelIndex idxMove = m_kifuRecordModel->index(r, 0);
        const QModelIndex idxTime = m_kifuRecordModel->index(r, 1);

        QString move = m_kifuRecordModel->data(idxMove, Qt::DisplayRole).toString();
        const QString time = m_kifuRecordModel->data(idxTime, Qt::DisplayRole).toString();

        qDebug().noquote() << "[KLC-DEBUG] collectDispFromRecordModel_: r=" << r << "move=" << move;

        // ★ 重複付与検知時のみ、先頭の ASCII 手数を除去
        move.remove(kDupMoveNoPattern);

        const int ply = r; // ヘッダを除いた 1 始まりの絶対手数
        disp.push_back(KifDisplayItem(move, time, QString(), ply));
    }

    qDebug().noquote() << "[KLC-DEBUG] collectDispFromRecordModel_: final disp.size=" << disp.size();
    return disp;
}

// ライブ（HvH/HvE）対局の 1手追加ごとに分岐ツリーを更新するエントリポイント
void KifuLoadCoordinator::updateBranchTreeFromLive(int currentPly)
{
    qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive ENTER: currentPly=" << currentPly
                       << "liveGameState.isActive=" << m_liveGameState.isActive;

    if (!m_analysisTab) {
        qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: m_analysisTab is null, returning";
        return;
    }

    // ★★★ m_liveGameState を使用してUI表示のみを更新
    // KifuBranchTree は変更しない（対局終了時に commitLiveGameToResolvedRows で確定する）

    if (m_liveGameState.isActive) {
        // ライブ対局中の場合
        qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: using liveGameState"
                           << "anchorPly=" << m_liveGameState.anchorPly
                           << "parentRowIndex=" << m_liveGameState.parentRowIndex
                           << "moves.size=" << m_liveGameState.moves.size();

        QVector<EngineAnalysisTab::ResolvedRowLite> rowsLite;

        // ★ 新システム: KifuBranchTree から既存ラインを取得
        if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
            QVector<BranchLine> lines = m_branchTree->allLines();
            rowsLite.reserve(lines.size() + 1);

            // 既存のラインを追加
            for (int i = 0; i < lines.size(); ++i) {
                const BranchLine& line = lines.at(i);
                EngineAnalysisTab::ResolvedRowLite x;
                x.startPly = line.branchPly;
                x.parent = (line.branchPoint != nullptr)
                               ? m_branchTree->findLineIndexForNode(line.branchPoint)
                               : -1;

                for (KifuBranchNode* node : std::as_const(line.nodes)) {
                    KifDisplayItem item;
                    item.prettyMove = node->displayText();
                    item.comment = node->comment();
                    item.timeText = node->timeText();
                    item.ply = node->ply();
                    x.disp.append(item);
                    x.sfen.append(node->sfen());
                }
                rowsLite.push_back(std::move(x));
            }

            // ライブゲーム行を追加
            EngineAnalysisTab::ResolvedRowLite liveRow;
            liveRow.startPly = m_liveGameState.anchorPly + 1;
            liveRow.parent = m_liveGameState.parentRowIndex;

            // 親行のデータを anchorPly まで コピー（KifuBranchTree から）
            int parentIdx = m_liveGameState.parentRowIndex;
            if (parentIdx < 0) parentIdx = 0;  // デフォルトは本譜
            if (parentIdx < lines.size()) {
                const BranchLine& parentLine = lines.at(parentIdx);
                for (int i = 0; i <= m_liveGameState.anchorPly && i < parentLine.nodes.size(); ++i) {
                    KifuBranchNode* node = parentLine.nodes.at(i);
                    KifDisplayItem item;
                    item.prettyMove = node->displayText();
                    item.comment = node->comment();
                    item.timeText = node->timeText();
                    item.ply = node->ply();
                    liveRow.disp.append(item);
                    liveRow.sfen.append(node->sfen());
                }
            }

            // ライブ対局の手を追加
            liveRow.disp.append(m_liveGameState.moves);
            liveRow.sfen.append(m_liveGameState.sfens);

            rowsLite.push_back(std::move(liveRow));
        } else {
            // KifuBranchTree がない場合は早期リターン
            qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: no BranchTree available for live game";
            return;
        }

        const int liveRowIdx = static_cast<int>(rowsLite.size() - 1);
        const int highlightAbsPly = m_liveGameState.anchorPly + static_cast<int>(m_liveGameState.moves.size());

        qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: rows=" << rowsLite.size()
                           << "liveRowIdx=" << liveRowIdx
                           << "highlightAbsPly=" << highlightAbsPly;

        m_analysisTab->setBranchTreeRows(rowsLite);
        m_analysisTab->highlightBranchTreeAt(liveRowIdx, highlightAbsPly, /*centerOn=*/true);
        return;
    }

    // 以下は m_liveGameState.isActive == false の場合（通常の新規対局など）

    // ★ 新システム: KifuBranchTree から分岐ツリーを構築
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: using KifuBranchTree"
                           << "lineCount=" << m_branchTree->lineCount();

        QVector<BranchLine> lines = m_branchTree->allLines();
        QVector<EngineAnalysisTab::ResolvedRowLite> rowsLite;
        rowsLite.reserve(lines.size());

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

            rowsLite.push_back(std::move(x));
        }

        m_analysisTab->setBranchTreeRows(rowsLite);

        // ハイライト位置を更新
        const int nLines = static_cast<int>(lines.size());
        const int highlightRow = (nLines == 0) ? 0 : qBound(0, m_activeResolvedRow, nLines - 1);
        int highlightAbsPly = 0;
        if (highlightRow >= 0 && highlightRow < nLines) {
            const int lineLen = static_cast<int>(lines.at(highlightRow).nodes.size());
            highlightAbsPly = qBound(0, currentPly, lineLen - 1);
        }
        m_analysisTab->highlightBranchTreeAt(highlightRow, highlightAbsPly, /*centerOn=*/true);
        return;
    }

    // KifuBranchTree がない場合は何もしない
    qDebug().noquote() << "[KLC-DEBUG] updateBranchTreeFromLive: no BranchTree available (non-live)";
}

void KifuLoadCoordinator::applyBranchMarksForCurrentLine()
{
    if (!m_kifuRecordModel) return;

    QSet<int> marks; // ply1=1..N（モデルの行番号と一致。0は開始局面なので除外）

    qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: m_activeResolvedRow=" << m_activeResolvedRow
                       << "liveGameState.isActive=" << m_liveGameState.isActive;

    // KifuBranchTree から分岐点を取得
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<BranchLine> lines = m_branchTree->allLines();
        const int nLines = static_cast<int>(lines.size());
        const int active = (nLines == 0) ? 0 : qBound(0, m_activeResolvedRow, nLines - 1);
        if (active >= 0 && active < nLines) {
            const BranchLine& line = lines.at(active);
            marks = m_branchTree->branchablePlysOnLine(line);
        }
        qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: marks=" << marks;
    } else {
        // KifuBranchTree がない場合は空のマークを設定して終了
        qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: no BranchTree available";
        m_kifuRecordModel->setBranchPlyMarks(marks);
        return;
    }

    // ライブ対局中は、親行の分岐プランからもマークを取得
    if (m_liveGameState.isActive && m_liveGameState.parentRowIndex >= 0) {
        // KifuBranchTree から親行のマークを取得
        if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
            QVector<BranchLine> lines = m_branchTree->allLines();
            const int nLines = static_cast<int>(lines.size());
            const int parentLine = qBound(0, m_liveGameState.parentRowIndex, nLines - 1);
            if (parentLine >= 0 && parentLine < nLines) {
                const BranchLine& line = lines.at(parentLine);
                QSet<int> parentMarks = m_branchTree->branchablePlysOnLine(line);
                // anchorPly以下の手のみ追加
                for (int ply1 : std::as_const(parentMarks)) {
                    if (ply1 > 0 && ply1 <= m_liveGameState.anchorPly) {
                        marks.insert(ply1);
                    }
                }
            }
        }

        // ★ ライブ対局の最初の手（anchorPly + 1）に分岐マークを追加
        // ライブ対局で指した手が親行と異なる場合、そこが分岐点になる
        const int firstLivePly = m_liveGameState.anchorPly + 1;
        qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: checking live branch"
                           << "anchorPly=" << m_liveGameState.anchorPly
                           << "firstLivePly=" << firstLivePly
                           << "moves.size=" << m_liveGameState.moves.size()
                           << "parentRowIndex=" << m_liveGameState.parentRowIndex;

        // ライブ対局の分岐判定: 新システムと旧システムで共通のロジック
        if (!m_liveGameState.moves.isEmpty() && m_liveGameState.parentRowIndex >= 0) {
            QString parentMove;
            bool hasParentMove = false;

            // 親行の firstLivePly 手目を取得（KifuBranchTree から）
            if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
                QVector<BranchLine> lines = m_branchTree->allLines();
                const int nLines = static_cast<int>(lines.size());
                const int parentLineIdx = qBound(0, m_liveGameState.parentRowIndex, nLines - 1);
                if (parentLineIdx >= 0 && parentLineIdx < nLines) {
                    const BranchLine& parentLine = lines.at(parentLineIdx);
                    if (firstLivePly < static_cast<int>(parentLine.nodes.size())) {
                        parentMove = parentLine.nodes.at(firstLivePly)->displayText();
                        hasParentMove = true;
                    }
                }
            }

            const QString liveMove = m_liveGameState.moves[0].prettyMove;

            if (hasParentMove) {
                qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: comparing"
                                   << "parent=" << parentMove << "live=" << liveMove;
                if (parentMove != liveMove) {
                    marks.insert(firstLivePly);
                    qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: added live branch mark at ply"
                                       << firstLivePly;
                } else {
                    qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: moves are same, no mark added";
                }
            } else {
                // 親行に firstLivePly 手目がない → ライブ対局の手が新規の手なので分岐マーク追加
                marks.insert(firstLivePly);
                qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: parent has no move at"
                                   << firstLivePly << "adding mark for new move";
            }
        }
    }

    qDebug().noquote() << "[KLC] applyBranchMarksForCurrentLine: final marks=" << marks;
    m_kifuRecordModel->setBranchPlyMarks(marks);
}

void KifuLoadCoordinator::rebuildBranchPlanAndMarksForLive(int /*currentPly*/)
{
    // KifuBranchTree が必須
    if (m_branchTree == nullptr || m_branchTree->isEmpty()) {
        if (m_kifuRecordModel) {
            const QSet<int> empty;
            m_kifuRecordModel->setBranchPlyMarks(empty);
        }
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        return;
    }

    // 1) 分岐候補Planの再構築（新システムではスキップされる）
    buildBranchCandidateDisplayPlan();

    // 2) 「現在の局面から開始」のアンカー手（= 1局目の途中で選択した手）を固定取得
    const int anchorPly = (m_branchPlyContext >= 0)
                              ? m_branchPlyContext
                              : qMax(0, m_currentSelectedPly);

    // 3) アンカー行を求める
    int anchorRow = 0;
    const int nLines = m_branchTree->lineCount();
    if (m_activeResolvedRow >= 0 && m_activeResolvedRow < nLines) {
        anchorRow = m_activeResolvedRow;
    }

    // 4) デリゲート用マーカー／モデルの '+' は「一時的に」anchorRow を
    //    アクティブとして再計算してから元に戻す（文脈は動かさない）
    const int oldActive = m_activeResolvedRow;
    m_activeResolvedRow = anchorRow;
    updateKifuBranchMarkersForActiveRow();   // 行背景（オレンジ）の再計算
    applyBranchMarksForCurrentLine();       // 行末の '+' 再付与
    m_activeResolvedRow = oldActive;

    // 5) 分岐候補欄は UI のみ更新（★ m_branchPlyContext を変更しない）
    refreshBranchCandidatesUIOnly(anchorRow, anchorPly);
}

void KifuLoadCoordinator::refreshBranchCandidatesUIOnly(int row, int ply1)
{
    Q_UNUSED(row)
    Q_UNUSED(ply1)
    // 分岐候補の表示は KifuDisplayCoordinator が管理する
    // この関数は互換性のために残すが、何もしない
    qDebug().noquote() << "[KLC] refreshBranchCandidatesUIOnly: skipped (KifuDisplayCoordinator manages branch candidates)";
}

// ★ 追加：分岐コンテキストをリセット（対局終了時に使用）
void KifuLoadCoordinator::resetBranchContext()
{
    m_branchPlyContext = -1;
    // ★ m_liveBranchAnchorPly はリセットしない（対局終了後も分岐構造を維持するため）
    // 新規対局開始時に resetBranchTreeForNewGame でリセットされる
    m_activeResolvedRow = 0;  // 本譜行に戻す
    // ★ 注意: m_liveGameState は commitLiveGameToResolvedRows() でクリアするので
    //   ここではクリアしない（対局終了時の順序問題を回避）
}

// ★ 追加：分岐ツリーを完全リセット（新規対局開始時に使用）
void KifuLoadCoordinator::resetBranchTreeForNewGame()
{
    qDebug().noquote() << "[KLC] resetBranchTreeForNewGame: clearing all branch data";

    // 1) 分岐コンテキストを完全リセット
    m_branchPlyContext = -1;
    m_liveBranchAnchorPly = -1;  // ★ これをリセットすることで、新規対局で古い分岐が残らない
    m_activeResolvedRow = 0;
    m_liveGameState.clear();  // ★ 追加: ライブ対局状態をクリア

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

    // 2) 分岐表示計画をクリア
    m_branchDisplayPlan.clear();

    // 4) 分岐候補インデックスをクリア
    m_branchIndex.clear();
    m_branchablePlySet.clear();

    // 5) 分岐ツリーUIをクリア
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> emptyRows;
        m_analysisTab->setBranchTreeRows(emptyRows);
    }

    qDebug().noquote() << "[KLC] resetBranchTreeForNewGame: done";
}

bool KifuLoadCoordinator::setupBranchForResumeFromCurrent(int anchorPly, const QString& terminalLabel)
{
    Q_UNUSED(terminalLabel);

    qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: anchorPly=" << anchorPly
                       << "terminalLabel=" << terminalLabel;

    // anchorPly は 0-based（開始局面=0, 1手目=1, ...）
    if (anchorPly < 0) {
        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: invalid anchorPly";
        return false;
    }

    // KifuBranchTree が必要
    if (m_branchTree == nullptr || m_branchTree->isEmpty()) {
        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: KifuBranchTree is empty";
        return false;
    }

    QVector<BranchLine> lines = m_branchTree->allLines();
    const int nLines = static_cast<int>(lines.size());
    if (nLines == 0) {
        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: no lines in BranchTree";
        return false;
    }

    // 親行を決定（m_activeResolvedRow が有効な場合はそれを使用、そうでなければ本譜）
    int parentRow = 0;  // デフォルトは本譜
    if (m_activeResolvedRow >= 0 && m_activeResolvedRow < nLines) {
        const BranchLine& activeLine = lines.at(m_activeResolvedRow);
        if (activeLine.nodes.size() > anchorPly) {
            parentRow = m_activeResolvedRow;
            qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: using activeResolvedRow as parent:"
                               << parentRow;
        }
    }

    // 親ラインの情報を取得
    const BranchLine& parentLine = lines.at(parentRow);

    // 棋譜モデルを分岐ラインのデータで更新（本譜以外から開始する場合）
    if (parentRow != 0 && m_kifuRecordModel) {
        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: updating kifuRecordModel with branch line data"
                           << "parentRow=" << parentRow << "anchorPly=" << anchorPly;

        m_kifuRecordModel->clearAllItems();
        m_kifuRecordModel->appendItem(
            new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                            QStringLiteral("（１手 / 合計）")));

        for (int i = 1; i <= anchorPly && i < parentLine.nodes.size(); ++i) {
            KifuBranchNode* node = parentLine.nodes.at(i);
            QString moveText = node->displayText();
            static const QRegularExpression kStartsWithNumber(QStringLiteral("^\\s*\\d"));
            if (!kStartsWithNumber.match(moveText).hasMatch() && !moveText.isEmpty()) {
                const QString moveNumberStr = QString::number(i);
                const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
                moveText = spaces + moveNumberStr + QLatin1Char(' ') + moveText;
            }
            m_kifuRecordModel->appendItem(
                new KifuDisplay(moveText, node->timeText(), node->comment()));
        }

        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: kifuRecordModel updated with"
                           << m_kifuRecordModel->rowCount() << "rows";
    }

    // ライブゲーム状態を初期化
    m_liveGameState.clear();
    m_liveGameState.anchorPly = anchorPly;
    m_liveGameState.parentRowIndex = parentRow;
    m_liveGameState.isActive = true;

    qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: initialized liveGameState"
                       << "anchorPly=" << m_liveGameState.anchorPly
                       << "parentRowIndex=" << m_liveGameState.parentRowIndex;

    // 分岐コンテキストを設定
    m_branchPlyContext = anchorPly;
    m_liveBranchAnchorPly = anchorPly;
    m_branchTreeLocked = false;

    // 分岐表示プランを再構築
    buildBranchCandidateDisplayPlan();
    applyBranchMarksForCurrentLine();

    // 分岐ツリータブの表示を更新
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rowsLite;
        rowsLite.reserve(nLines + 1);

        // 既存のラインを追加
        for (int i = 0; i < nLines; ++i) {
            const BranchLine& line = lines.at(i);
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = line.branchPly;
            x.parent = (line.branchPoint != nullptr)
                           ? m_branchTree->findLineIndexForNode(line.branchPoint)
                           : -1;

            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                item.prettyMove = node->displayText();
                item.comment = node->comment();
                item.timeText = node->timeText();
                item.ply = node->ply();
                x.disp.append(item);
                x.sfen.append(node->sfen());
            }
            rowsLite.push_back(std::move(x));
        }

        // ライブゲーム行を追加（親行のプレフィックスのみ）
        EngineAnalysisTab::ResolvedRowLite liveRow;
        liveRow.startPly = anchorPly + 1;
        liveRow.parent = parentRow;

        for (int i = 0; i <= anchorPly && i < parentLine.nodes.size(); ++i) {
            KifuBranchNode* node = parentLine.nodes.at(i);
            KifDisplayItem item;
            item.prettyMove = node->displayText();
            item.comment = node->comment();
            item.timeText = node->timeText();
            item.ply = node->ply();
            liveRow.disp.append(item);
            liveRow.sfen.append(node->sfen());
        }
        rowsLite.push_back(std::move(liveRow));

        const int liveRowIdx = static_cast<int>(rowsLite.size() - 1);

        qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: updating branch tree UI"
                           << "rows=" << rowsLite.size();

        m_analysisTab->setBranchTreeRows(rowsLite);
        m_analysisTab->highlightBranchTreeAt(liveRowIdx, anchorPly, /*centerOn=*/true);
    }

    qDebug().noquote() << "[KLC] setupBranchForResumeFromCurrent: done"
                       << "m_branchPlyContext=" << m_branchPlyContext
                       << "liveGameState.isActive=" << m_liveGameState.isActive;

    return true;
}

// ★ 追加：ライブ対局の手を追加
void KifuLoadCoordinator::appendLiveMove(const KifDisplayItem& item, const QString& sfen, const ShogiMove& move)
{
    if (!m_liveGameState.isActive) {
        qDebug().noquote() << "[KLC] appendLiveMove: liveGameState is not active, ignoring";
        return;
    }

    m_liveGameState.moves.append(item);
    m_liveGameState.sfens.append(sfen);
    m_liveGameState.gameMoves.append(move);

    qDebug().noquote() << "[KLC] appendLiveMove: added move"
                       << "ply=" << m_liveGameState.totalPly()
                       << "move=" << item.prettyMove
                       << "moves.size=" << m_liveGameState.moves.size();

    // ★ 追加: 分岐マーク（'+'）を更新
    applyBranchMarksForCurrentLine();

    emit liveGameStateChanged();
}

// ★ 追加：ライブ対局をResolvedRowに確定
void KifuLoadCoordinator::commitLiveGameToResolvedRows()
{
    if (!m_liveGameState.isActive) {
        qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: liveGameState is not active";
        return;
    }

    if (m_liveGameState.moves.isEmpty()) {
        qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: no live moves to commit";
        m_liveGameState.clear();
        return;
    }

    qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows:"
                       << "anchorPly=" << m_liveGameState.anchorPly
                       << "parentRowIndex=" << m_liveGameState.parentRowIndex
                       << "moves.size=" << m_liveGameState.moves.size();

    // KifuBranchTree にライブデータを追加
    int newLineIndex = -1;
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        // 親ノードを見つける（anchorPlyの位置）
        KifuBranchNode* parentNode = nullptr;
        int parentLineIdx = m_liveGameState.parentRowIndex;
        if (parentLineIdx < 0) parentLineIdx = 0;  // デフォルトは本譜

        QVector<BranchLine> lines = m_branchTree->allLines();
        if (parentLineIdx < lines.size()) {
            const BranchLine& parentLine = lines.at(parentLineIdx);
            // anchorPly の位置にあるノードを取得
            if (m_liveGameState.anchorPly < parentLine.nodes.size()) {
                parentNode = parentLine.nodes.at(m_liveGameState.anchorPly);
            }
        }

        // 親ノードにライブの手を追加
        if (parentNode != nullptr) {
            qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: adding to KifuBranchTree"
                               << "parentNode ply=" << parentNode->ply()
                               << "moves to add=" << m_liveGameState.moves.size();

            KifuBranchNode* currentNode = parentNode;
            for (int i = 0; i < m_liveGameState.moves.size(); ++i) {
                const KifDisplayItem& item = m_liveGameState.moves.at(i);
                const QString& sfen = (i < m_liveGameState.sfens.size())
                                          ? m_liveGameState.sfens.at(i)
                                          : QString();
                const ShogiMove& move = (i < m_liveGameState.gameMoves.size())
                                            ? m_liveGameState.gameMoves.at(i)
                                            : ShogiMove();

                KifuBranchNode* newNode = m_branchTree->addMove(
                    currentNode, move, item.prettyMove, sfen, item.timeText);
                if (newNode != nullptr) {
                    currentNode = newNode;
                }
            }

            // 新しく作成したラインのインデックスを取得
            if (currentNode != nullptr && currentNode != parentNode) {
                newLineIndex = m_branchTree->findLineIndexForNode(currentNode);
            }
        }
    }

    qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: created line" << newLineIndex
                       << "moves.size=" << m_liveGameState.moves.size();

    // 新しく作成した行をアクティブにする
    if (newLineIndex >= 0) {
        m_activeResolvedRow = newLineIndex;
    }

    // 分岐表示プランを再構築
    buildBranchCandidateDisplayPlan();

    // デバッグ: 分岐プランの内容を出力
    qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: branch plan after rebuild:";
    for (auto it = m_branchDisplayPlan.constBegin(); it != m_branchDisplayPlan.constEnd(); ++it) {
        const int rowIdx = it.key();
        const QMap<int, BranchCandidateDisplay>& byPly = it.value();
        qDebug().noquote() << "  row[" << rowIdx << "]: plys with branches:";
        for (auto pit = byPly.constBegin(); pit != byPly.constEnd(); ++pit) {
            qDebug().noquote() << "    ply" << pit.key() << ": " << pit.value().items.size() << " items";
        }
    }
    qDebug().noquote() << "[KLC] commitLiveGameToResolvedRows: m_activeResolvedRow=" << m_activeResolvedRow;

    applyBranchMarksForCurrentLine();

    // ライブゲーム状態をクリア
    m_liveGameState.clear();
    m_liveBranchAnchorPly = -1;

    // 分岐ツリーUIを更新
    if (m_analysisTab && m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rowsLite;
        QVector<BranchLine> lines = m_branchTree->allLines();
        rowsLite.reserve(lines.size());

        for (int i = 0; i < lines.size(); ++i) {
            const BranchLine& line = lines.at(i);
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = line.branchPly;
            x.parent = (line.branchPoint != nullptr)
                           ? m_branchTree->findLineIndexForNode(line.branchPoint)
                           : -1;

            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                item.prettyMove = node->displayText();
                item.comment = node->comment();
                item.timeText = node->timeText();
                item.ply = node->ply();
                x.disp.append(item);
                x.sfen.append(node->sfen());
            }
            rowsLite.push_back(std::move(x));
        }

        const int highlightRow = (newLineIndex >= 0) ? newLineIndex : 0;
        const int lastPly = (highlightRow < lines.size() && !lines.at(highlightRow).nodes.isEmpty())
                                ? lines.at(highlightRow).nodes.last()->ply()
                                : 0;
        m_analysisTab->setBranchTreeRows(rowsLite);
        m_analysisTab->highlightBranchTreeAt(highlightRow, lastPly, /*centerOn=*/true);
    }

    emit liveGameCommitted(newLineIndex);
}
