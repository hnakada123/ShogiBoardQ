/// @file dialogcoordinator.cpp
/// @brief ダイアログコーディネータクラスの実装

#include "dialogcoordinator.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"

#include <QWidget>
#include <QMessageBox>
#include "logcategories.h"

#include "aboutcoordinator.h"
#include "engineregistrationdialog.h"
#include "promotionflow.h"
#include "considerationflowcontroller.h"
#include "tsumesearchflowcontroller.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "kifuanalysislistmodel.h"
#include "considerationtabmanager.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuloadcoordinator.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"
#include "considerationpositionresolver.h"
#include "shogiutils.h"
#include "sfenutils.h"

namespace {
QString extractUsiMoveFromKanjiLabel(const QString& moveLabel, int fallbackFileTo, int fallbackRankTo)
{
    if (moveLabel.isEmpty()) {
        return QString();
    }

    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark  = QStringLiteral("△");
    qsizetype markPos = moveLabel.indexOf(senteMark);
    if (markPos < 0) {
        markPos = moveLabel.indexOf(goteMark);
    }
    if (markPos < 0 || moveLabel.length() <= markPos + 1) {
        return QString();
    }

    const QString afterMark = moveLabel.mid(markPos + 1);
    const bool isDrop = afterMark.contains(QStringLiteral("打"));
    const bool isPromotion = afterMark.contains(QStringLiteral("成")) && !afterMark.contains(QStringLiteral("不成"));

    int fileTo = 0;
    int rankTo = 0;
    if (afterMark.startsWith(QStringLiteral("同"))) {
        fileTo = fallbackFileTo;
        rankTo = fallbackRankTo;
    } else if (afterMark.size() >= 2) {
        fileTo = ShogiUtils::parseFullwidthFile(afterMark.at(0));
        rankTo = ShogiUtils::parseKanjiRank(afterMark.at(1));
    }
    if (fileTo < 1 || fileTo > 9 || rankTo < 1 || rankTo > 9) {
        return QString();
    }
    const QChar toRankAlpha = QChar('a' + rankTo - 1);

    if (isDrop) {
        static const QString pieceChars = QStringLiteral("歩香桂銀金角飛");
        static const QString usiPieces  = QStringLiteral("PLNSGBR");
        QChar pieceUsi;
        for (qsizetype i = 0; i < pieceChars.size(); ++i) {
            if (afterMark.contains(pieceChars.at(i))) {
                pieceUsi = usiPieces.at(i);
                break;
            }
        }
        if (pieceUsi.isNull()) {
            return QString();
        }
        return QStringLiteral("%1*%2%3").arg(pieceUsi).arg(fileTo).arg(toRankAlpha);
    }

    const qsizetype parenStart = afterMark.indexOf(QLatin1Char('('));
    const qsizetype parenEnd   = afterMark.indexOf(QLatin1Char(')'));
    if (parenStart < 0 || parenEnd <= parenStart + 1) {
        return QString();
    }
    const QString srcStr = afterMark.mid(parenStart + 1, parenEnd - parenStart - 1);
    if (srcStr.size() != 2) {
        return QString();
    }
    const int fileFrom = srcStr.at(0).digitValue();
    const int rankFrom = srcStr.at(1).digitValue();
    if (fileFrom < 1 || fileFrom > 9 || rankFrom < 1 || rankFrom > 9) {
        return QString();
    }
    const QChar fromRankAlpha = QChar('a' + rankFrom - 1);

    QString usiMove = QStringLiteral("%1%2%3%4")
        .arg(fileFrom).arg(fromRankAlpha).arg(fileTo).arg(toRankAlpha);
    if (isPromotion) {
        usiMove += QLatin1Char('+');
    }
    return usiMove;
}
}  // namespace

DialogCoordinator::DialogCoordinator(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

DialogCoordinator::~DialogCoordinator() = default;

void DialogCoordinator::updateDeps(const Deps& deps)
{
    m_match = deps.matchCoordinator;
    m_usi = deps.usiEngine;
    m_logModel = deps.logModel;
    m_thinkingModel = deps.thinkingModel;
    m_analysisModel = deps.analysisModel;
    m_considerationTabManager = deps.considerationTabManager;
}

void DialogCoordinator::showVersionInformation()
{
    AboutCoordinator::showVersionDialog(m_parentWidget);
}

void DialogCoordinator::openProjectWebsite()
{
    AboutCoordinator::openProjectWebsite();
}

void DialogCoordinator::showEngineSettingsDialog()
{
    EngineRegistrationDialog dlg(m_parentWidget);
    dlg.exec();
}

bool DialogCoordinator::showPromotionDialog()
{
    return PromotionFlow::askPromote(m_parentWidget);
}

void DialogCoordinator::showGameOverMessage(const QString& title, const QString& message)
{
    QMessageBox::information(m_parentWidget, title, message);
}

bool DialogCoordinator::startConsiderationDirect(const ConsiderationDirectParams& params)
{
    qCDebug(lcUi).noquote() << "startConsiderationDirect: position=" << params.position
                       << "engineIndex=" << params.engineIndex
                       << "unlimitedTime=" << params.unlimitedTime
                       << "byoyomiSec=" << params.byoyomiSec
                       << "multiPV=" << params.multiPV;

    // Flow に一任
    ConsiderationFlowController flow(this);
    ConsiderationFlowController::Deps d;
    d.match = m_match;
    d.onStarted = [this]() {
        Q_EMIT considerationModeStarted();
    };
    d.onError = [this](const QString& msg) { showFlowError(msg); };
    d.considerationModel = params.considerationModel;
    d.onTimeSettingsReady = [this](bool unlimited, int byoyomiSec) {
        Q_EMIT considerationTimeSettingsReady(unlimited, byoyomiSec);
    };
    d.onMultiPVReady = [this](int multiPV) {
        Q_EMIT considerationMultiPVReady(multiPV);
    };

    ConsiderationFlowController::DirectParams directParams;
    directParams.engineIndex = params.engineIndex;
    directParams.engineName = params.engineName;
    directParams.unlimitedTime = params.unlimitedTime;
    directParams.byoyomiSec = params.byoyomiSec;
    directParams.multiPV = params.multiPV;
    directParams.previousFileTo = params.previousFileTo;
    directParams.previousRankTo = params.previousRankTo;
    directParams.lastUsiMove = params.lastUsiMove;

    return flow.runDirect(d, directParams, params.position);
}

void DialogCoordinator::showTsumeSearchDialog(const TsumeSearchParams& params)
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialog: currentMoveIndex=" << params.currentMoveIndex;

    Q_EMIT tsumeSearchModeStarted();

    // Flow に一任
    TsumeSearchFlowController flow(this);

    TsumeSearchFlowController::Deps d;
    d.match = m_match;
    d.sfenRecord = params.sfenRecord;
    d.startSfenStr = params.startSfenStr;
    d.positionStrList = params.positionStrList;
    d.currentMoveIndex = qMax(0, params.currentMoveIndex);
    d.usiMoves = params.usiMoves;
    d.startPositionCmd = params.startPositionCmd;
    d.onError = [this](const QString& msg) { showFlowError(msg); };

    const bool started = flow.runWithDialog(d, m_parentWidget);
    if (!started) {
        Q_EMIT tsumeSearchModeEnded();
    }
}

void DialogCoordinator::setConsiderationContext(const ConsiderationContext& ctx)
{
    m_considerationCtx = ctx;
}

bool DialogCoordinator::startConsiderationFromContext()
{
    qCDebug(lcUi).noquote() << "startConsiderationFromContext";

    // エンジンが選択されているかチェック
    if (m_considerationTabManager && m_considerationTabManager->selectedEngineName().isEmpty()) {
        QMessageBox::critical(m_parentWidget, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return false;
    }

    // 手番表示用の設定
    if (m_considerationCtx.gameController && m_considerationCtx.gameMoves && m_considerationCtx.currentMoveIndex) {
        const int moveIdx = *m_considerationCtx.currentMoveIndex;
        const int movesSize = static_cast<int>(m_considerationCtx.gameMoves->size());
        if (movesSize > 0 && moveIdx >= 0 && moveIdx < movesSize) {
            if (isBlackPiece(m_considerationCtx.gameMoves->at(moveIdx).movingPiece))
                m_considerationCtx.gameController->setCurrentPlayer(ShogiGameController::Player1);
            else
                m_considerationCtx.gameController->setCurrentPlayer(ShogiGameController::Player2);
        }
    }

    const int currentMoveIdx = m_considerationCtx.currentMoveIndex ? *m_considerationCtx.currentMoveIndex : 0;

    // ConsiderationPositionResolver で局面・ハイライト情報を一括解決
    ConsiderationPositionResolver::Inputs resolverInputs;
    resolverInputs.currentSfenStr = m_considerationCtx.currentSfenStr;
    resolverInputs.gameUsiMoves = m_considerationCtx.gameUsiMoves;
    resolverInputs.gameMoves = m_considerationCtx.gameMoves;
    resolverInputs.startSfenStr = m_considerationCtx.startSfenStr;
    resolverInputs.sfenRecord = m_considerationCtx.sfenRecord;
    resolverInputs.kifuLoadCoordinator = m_considerationCtx.kifuLoadCoordinator;
    resolverInputs.kifuRecordModel = m_considerationCtx.kifuRecordModel;
    resolverInputs.branchTree = m_considerationCtx.branchTree;
    resolverInputs.navState = m_considerationCtx.navState;

    const ConsiderationPositionResolver resolver(resolverInputs);
    const auto resolved = resolver.resolveForRow(currentMoveIdx);

    qCDebug(lcUi).noquote() << "startConsiderationFromContext: position=" << resolved.position.left(50);

    // 検討タブ専用モデルを作成（なければ）
    if (m_considerationCtx.considerationModel) {
        if (!(*m_considerationCtx.considerationModel)) {
            *m_considerationCtx.considerationModel = new ShogiEngineThinkingModel(m_parentWidget);
        }
    }

    // 検討タブの設定を保存
    if (m_considerationTabManager) {
        m_considerationTabManager->saveConsiderationTabSettings();
    }

    // 検討タブの設定を使用して直接検討を開始
    ConsiderationDirectParams params;
    params.position = resolved.position;
    params.engineIndex = m_considerationTabManager ? m_considerationTabManager->selectedEngineIndex() : 0;
    params.engineName = m_considerationTabManager ? m_considerationTabManager->selectedEngineName() : QString();
    params.unlimitedTime = m_considerationTabManager ? m_considerationTabManager->isUnlimitedTime() : true;
    params.byoyomiSec = m_considerationTabManager ? m_considerationTabManager->byoyomiSec() : 20;
    params.multiPV = m_considerationTabManager ? m_considerationTabManager->considerationMultiPV() : 1;
    params.considerationModel = m_considerationCtx.considerationModel ? *m_considerationCtx.considerationModel : nullptr;
    params.previousFileTo = resolved.previousFileTo;
    params.previousRankTo = resolved.previousRankTo;
    params.lastUsiMove = resolved.lastUsiMove;

    // 漢字表記からUSI指し手を抽出（UI固有フォールバック: resolver では解決できない場合）
    if (params.lastUsiMove.isEmpty() && currentMoveIdx > 0 && m_considerationCtx.kifuRecordModel) {
        const int rowCount = m_considerationCtx.kifuRecordModel->rowCount();
        if (currentMoveIdx < rowCount) {
            const QString moveLabel =
                m_considerationCtx.kifuRecordModel->index(currentMoveIdx, 0).data(Qt::DisplayRole).toString();
            params.lastUsiMove = extractUsiMoveFromKanjiLabel(
                moveLabel, params.previousFileTo, params.previousRankTo);
            qCDebug(lcUi).noquote() << "lastUsiMove (from record label fallback):" << params.lastUsiMove
                               << " label=" << moveLabel;
        }
    }

    qCDebug(lcUi).noquote() << "startConsiderationFromContext calling startConsiderationDirect"
                      << "engineIndex=" << params.engineIndex
                      << "engineName=" << params.engineName
                      << "unlimitedTime=" << params.unlimitedTime
                      << "byoyomiSec=" << params.byoyomiSec
                      << "multiPV=" << params.multiPV;
    const bool started = startConsiderationDirect(params);
    qCDebug(lcUi).noquote() << "startConsiderationFromContext EXIT";
    return started;
}

void DialogCoordinator::setTsumeSearchContext(const TsumeSearchContext& ctx)
{
    m_tsumeSearchCtx = ctx;
}

void DialogCoordinator::showTsumeSearchDialogFromContext()
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext";

    TsumeSearchParams params;
    // m_tsumeSearchCtx.sfenRecord は起動時に bindContexts() で1度だけ
    // キャプチャされるため、その時点の MatchCoordinator が指していた
    // m_sharedSfenRecord のアドレスで固定される。MC が後から再生成されると
    // この固定ポインタはダングリングとなり、参照すると解放済みメモリへの
    // アクセスでクラッシュする（リリースビルドで観測）。
    // displayTsumeShogiSearchDialog() で m_match を最新に差し替えた後に
    // 呼ばれる前提で、現在の MC から sfenRecord を取得する。
    params.sfenRecord = m_match ? m_match->sfenRecordPtr() : m_tsumeSearchCtx.sfenRecord;
    params.startSfenStr = m_tsumeSearchCtx.startSfenStr ? *m_tsumeSearchCtx.startSfenStr : QString();
    params.positionStrList = m_tsumeSearchCtx.positionStrList ? *m_tsumeSearchCtx.positionStrList : QStringList();
    params.currentMoveIndex = m_tsumeSearchCtx.currentMoveIndex ? qMax(0, *m_tsumeSearchCtx.currentMoveIndex) : 0;

    // USI形式の指し手リストを取得（棋譜解析と同様のロジック）
    const qsizetype sfenSize = params.sfenRecord ? params.sfenRecord->size() : 0;
    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext: sfenSize=" << sfenSize
                       << "gameUsiMoves.size=" << (m_tsumeSearchCtx.gameUsiMoves ? m_tsumeSearchCtx.gameUsiMoves->size() : -1)
                       << "kifuLoadCoordinator=" << (m_tsumeSearchCtx.kifuLoadCoordinator ? "exists" : "null");

    if (m_tsumeSearchCtx.gameUsiMoves && !m_tsumeSearchCtx.gameUsiMoves->isEmpty()) {
        const qsizetype usiSize = m_tsumeSearchCtx.gameUsiMoves->size();
        qCDebug(lcUi).noquote() << "checking gameUsiMoves: sfenSize=" << sfenSize << " usiSize=" << usiSize;
        if (sfenSize == usiSize + 1) {
            params.usiMoves = m_tsumeSearchCtx.gameUsiMoves;
            qCDebug(lcUi).noquote() << "using gameUsiMoves, size=" << usiSize;
        } else {
            qCDebug(lcUi).noquote() << "gameUsiMoves size mismatch";
        }
    } else if (m_tsumeSearchCtx.kifuLoadCoordinator) {
        QStringList* kifuUsiMoves = m_tsumeSearchCtx.kifuLoadCoordinator->kifuUsiMovesPtr();
        const qsizetype usiSize = kifuUsiMoves ? kifuUsiMoves->size() : 0;
        qCDebug(lcUi).noquote() << "checking kifuUsiMoves: sfenSize=" << sfenSize << " usiSize=" << usiSize;
        if (kifuUsiMoves) {
            qCDebug(lcUi).noquote() << "kifuUsiMoves content:" << *kifuUsiMoves;
        }
        if (sfenSize == usiSize + 1) {
            params.usiMoves = kifuUsiMoves;
            qCDebug(lcUi).noquote() << "using kifuLoadCoordinator->kifuUsiMovesPtr(), size=" << usiSize;
        } else {
            qCDebug(lcUi).noquote() << "kifuUsiMoves size mismatch (expected sfenSize == usiSize + 1)";
        }
    } else {
        qCDebug(lcUi).noquote() << "no usiMoves source available";
    }

    // 開始局面コマンドを決定（平手初期局面の場合は "startpos"、それ以外は "sfen ..."）
    static const QString kHirateSfen = SfenUtils::hirateSfen();
    if (params.startSfenStr.isEmpty() || params.startSfenStr == kHirateSfen) {
        params.startPositionCmd = QStringLiteral("startpos");
    } else {
        params.startPositionCmd = QStringLiteral("sfen ") + params.startSfenStr;
    }

    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext:"
                       << "usiMoves=" << (params.usiMoves ? QString::number(params.usiMoves->size()) : "null")
                       << "startSfenStr=" << params.startSfenStr.left(50)
                       << "startPositionCmd=" << params.startPositionCmd
                       << "currentMoveIndex=" << params.currentMoveIndex;

    showTsumeSearchDialog(params);
}

void DialogCoordinator::showFlowError(const QString& message)
{
    qCWarning(lcUi).noquote() << "Flow error:" << message;
    QMessageBox::warning(m_parentWidget, tr("エラー"), message);
}
