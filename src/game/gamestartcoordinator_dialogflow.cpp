/// @file gamestartcoordinator_dialogflow.cpp
/// @brief GameStartCoordinator::initializeGame の実装（ダイアログ対話→対局開始フロー）

#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "logcategories.h"
#include "kifurecordlistmodel.h"
#include "kifuloadcoordinator.h"
#include "matchcoordinator.h"
#include "shogiclock.h"

#include <QDebug>
#include <QLoggingCategory>

// ============================================================
// 対局開始メインフロー（ダイアログ対話は app/ 層で完了済み）
// ============================================================

void GameStartCoordinator::initializeGame(const Ctx& c)
{
    qCDebug(lcGame).noquote() << "initializeGame: ENTER"
                       << " c.currentSfenStr=" << (c.currentSfenStr ? c.currentSfenStr->left(50) : "null")
                       << " c.startSfenStr=" << (c.startSfenStr ? c.startSfenStr->left(50) : "null")
                       << " c.selectedPly=" << c.selectedPly
                       << " c.sfenRecord size=" << (c.sfenRecord ? c.sfenRecord->size() : -1);

    // --- 1) hasEditedStart 判定（ダイアログ表示・承認は呼び出し元 app/ 層で実施済み） ---
    static const QString kHirateSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const auto detectEditedStartSfen = [&]() -> QString {
        const auto isEditedStart = [&](const QString& raw) -> bool {
            const QString s = raw.trimmed();
            return !s.isEmpty()
                   && s != kHirateSfen
                   && s != QLatin1String("startpos");
        };

        if (c.startSfenStr && isEditedStart(*c.startSfenStr)) {
            return c.startSfenStr->trimmed();
        }

        // startSfenStr が途中で平手に戻ってしまうケースに備え、
        // 局面履歴の先頭（開始局面）をフォールバックとして使用する。
        if (c.sfenRecord && !c.sfenRecord->isEmpty() && isEditedStart(c.sfenRecord->first())) {
            return c.sfenRecord->first().trimmed();
        }

        return QString();
    };
    const QString editedStartSfen = detectEditedStartSfen();
    const bool hasEditedStart = !editedStartSfen.isEmpty();

    // --- 2) ダイアログ結果（app/ 層で抽出済み）から必要情報を取得 ---
    int  initPosNo = c.dialogData.startingPositionNumber;
    const bool p1Human   = c.dialogData.isHuman1;
    const bool p2Human   = c.dialogData.isHuman2;

    qCDebug(lcGame).noquote() << "initializeGame: after dialog, initPosNo=" << initPosNo;

    // 局面編集後の場合、ダイアログの選択に関わらず「現在の局面」を強制
    // （app/ 層での設定バックアップ: dialogData.startingPositionNumber が
    //   正しく 0 に設定されていない場合への対策）
    if (hasEditedStart) {
        if (initPosNo != 0) {
            qCDebug(lcGame).noquote() << "initializeGame: overriding initPosNo from"
                               << initPosNo << "to 0 (edited position detected)";
            initPosNo = 0;
        }
    }

    // --- 3) 開始SFENの決定 ---
    const int startingPosNumber = initPosNo;

    // 対局開始後に選択すべき棋譜行（現在局面から開始時に使用）
    int startingRow = -1;

    QString startSfen = editedStartSfen;
    if (startSfen.isEmpty() && c.startSfenStr && !c.startSfenStr->isEmpty()) {
        startSfen = *(c.startSfenStr);
    }
    qCDebug(lcGame).noquote() << "initializeGame: BEFORE prepareDataCurrentPosition"
                       << " startSfen=" << startSfen.left(50)
                       << " c.currentSfenStr=" << (c.currentSfenStr ? c.currentSfenStr->left(50) : "null");

    if (startingPosNumber == 0) {
        // 現在局面から開始
        prepareDataCurrentPosition(c);

        qCDebug(lcGame).noquote() << "initializeGame: AFTER prepareDataCurrentPosition"
                           << " c.currentSfenStr=" << (c.currentSfenStr ? c.currentSfenStr->left(50) : "null");

        if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
            startSfen = *(c.currentSfenStr);
            qCDebug(lcGame).noquote() << "initializeGame: SET startSfen from currentSfenStr=" << startSfen.left(50);
        } else if (startSfen.isEmpty()) {
            startSfen = QStringLiteral("startpos");
            qCDebug(lcGame).noquote() << "initializeGame: FALLBACK to startpos";
        }
    } else {
        // 平手/駒落ちプリセット（GameStartOptionsBuilder のテーブルを使用）
        startSfen = GameStartOptionsBuilder::startingPositionSfen(startingPosNumber);

        // プリセット局面は新規対局なので、全UIコンポーネントをクリアする。
        // shouldStartFromCurrentPosition() が false を返すよう
        // currentSfenStr を "startpos" に設定してフルクリアを強制する。
        if (c.currentSfenStr) {
            *c.currentSfenStr = QStringLiteral("startpos");
        }
        emit requestPreStartCleanup();

        prepareInitialPosition(c);

        // 平手/駒落ちから新規対局を開始する場合、分岐ツリーを完全リセット
        if (c.kifuLoadCoordinator) {
            c.kifuLoadCoordinator->resetBranchTreeForNewGame();
        } else {
            emit requestBranchTreeResetForNewGame();
        }
    }

    // --- 3.5) 開始SFENを正規化してsfenRecordにseed ---
    const QString seedSfen = GameStartOptionsBuilder::canonicalizeSfen(startSfen);

    if (c.sfenRecord) {
        // 現在局面から開始（startingPosNumber==0）の場合は
        // 0..selectedPly を保全し、末尾を seedSfen に置換
        if (startingPosNumber == 0 && !c.sfenRecord->isEmpty() && c.selectedPly >= 0) {
            // c.selectedPly を常に基準として使用する。
            // 分岐ツリーから特定の手を選択して対局開始する場合、
            // 棋譜モデルの行数は前の対局の値を反映している可能性があるため。
            int keepIdx = static_cast<int>(qBound(qsizetype(0), qsizetype(c.selectedPly), c.sfenRecord->size() - 1));
            const int takeLen = keepIdx + 1;

            QStringList preserved;
            preserved.reserve(takeLen);
            for (int i = 0; i < takeLen; ++i) {
                preserved.append(c.sfenRecord->at(i));
            }
            if (!preserved.isEmpty()) {
                preserved[preserved.size() - 1] = seedSfen;
            }

            c.sfenRecord->clear();
            c.sfenRecord->append(preserved);

            startingRow = keepIdx;

            qCDebug(lcGame).noquote()
                << "seed-resume: kept(0.." << keepIdx << ") size=" << c.sfenRecord->size()
                << " head=" << (c.sfenRecord->isEmpty() ? QString("<empty>") : c.sfenRecord->first());
        } else {
            c.sfenRecord->clear();
            c.sfenRecord->append(seedSfen);

            qCDebug(lcGame).noquote()
                << "seed: sfenRecord*=" << static_cast<const void*>(c.sfenRecord)
                << " size=" << c.sfenRecord->size()
                << " head=" << (c.sfenRecord->isEmpty() ? QString("<empty>") : c.sfenRecord->first());
        }
    } else {
        qCWarning(lcGame) << "seed: sfenRecord is null (cannot seed)";
    }

    // --- 4) PlayMode を SFEN手番と整合させて最終決定 ---
    PlayMode mode = determinePlayModeAlignedWithTurn(initPosNo, p1Human, p2Human, seedSfen);
    qCDebug(lcGame) << "Final PlayMode =" << static_cast<int>(mode) << "  startSfen=" << seedSfen;

    // --- 5) StartOptions 構築 ---
    if (!m_match) {
        return;
    }
    MatchCoordinator::StartOptions opt =
        m_match->buildStartOptions(mode, seedSfen, c.sfenRecord, &c.dialogData);

    m_match->ensureHumanAtBottomIfApplicable(&c.dialogData, c.bottomIsP1);

    // --- 6) TimeControl を構築（GameStartOptionsBuilder に委譲） ---
    const TimeControl tc = GameStartOptionsBuilder::buildTimeControl(c.dialogData);

    // --- 7) 時計の準備と配線・起動は prepare() に委譲 ---
    Request req;
    req.dialogData  = c.dialogData;
    req.startSfen   = seedSfen;
    req.clock       = c.clock ? c.clock : m_match->clock();
    // 現在局面からの開始時は prepareDataCurrentPosition() で既にクリーンアップ済み
    req.skipCleanup = (startingPosNumber == 0);

    prepare(req);

    // --- 8) 対局者名をMainWindowに通知（startの前に。EvE初手で評価値グラフが動くため） ---
    const QString human1 = c.dialogData.humanName1;
    const QString human2 = c.dialogData.humanName2;
    const QString engine1 = opt.engineName1;
    const QString engine2 = opt.engineName2;
    qCDebug(lcGame).noquote() << "startGameAfterDialog: BEFORE playerNamesResolved";
    qCDebug(lcGame).noquote() << "human1=" << human1 << " human2=" << human2 << " engine1=" << engine1 << " engine2=" << engine2;
    emit playerNamesResolved(human1, human2, engine1, engine2, static_cast<int>(mode));

    // --- 8.5) 連続対局設定を通知（EvE対局時のみ有効） ---
    const int consecutiveGames = c.dialogData.consecutiveGames;
    const bool switchTurn = c.dialogData.isSwitchTurnEachGame;
    emit consecutiveGamesConfigured(consecutiveGames, switchTurn);
    qCDebug(lcGame).noquote() << "consecutiveGames=" << consecutiveGames << " switchTurn=" << switchTurn;

    qCDebug(lcGame).noquote() << "startGameAfterDialog: AFTER playerNamesResolved, BEFORE start()";

    // --- 9) 対局開始（時計設定のみ、初手goはまだ呼ばない） ---
    StartParams params;
    params.opt  = opt;
    params.tc   = tc;
    params.autoStartEngineMove = false;

    start(params);
    qCDebug(lcGame).noquote() << "startGameAfterDialog: AFTER start()";

    // --- 9.5) 現在局面から開始の場合、開始行を選択するよう通知 ---
    if (startingRow >= 0) {
        qCDebug(lcGame).noquote() << "emit requestSelectKifuRow(" << startingRow << ")";
        emit requestSelectKifuRow(startingRow);
    }

    // --- 10) 時計の関連付けと開始、その後エンジン初手 ---
    if (m_match) {
        if (c.clock && m_match->clock() != c.clock) {
            m_match->setClock(c.clock);
        }
        if (ShogiClock* clk = m_match->clock()) {
            clk->startClock();
        }
        // 初手がエンジン手番なら go を起動（1回だけ）
        m_match->startInitialEngineMoveIfNeeded();
    }
}
