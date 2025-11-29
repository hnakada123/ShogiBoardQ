#include "csatosfenconverter.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

#include <QStringDecoder>

// --- file-scope helper: CSAコメント1行を「表示用テキスト」へ正規化 ---
// 返り値：
//   null QString  …  この行は完全に無視（Kifu for Windowsバナーや '**' 評価系）
//   ""              …  段落区切り（空行）として扱う
//   その他文字列    …  先頭の"'"や装飾 '*' を取り除いた本文
static QString normalizeCsaCommentLine_(const QString& line)
{
    if (line.isEmpty() || line.at(0) != QLatin1Char('\'')) return QString();

    // 先頭の "'" を除去。続く半角スペースを1個だけ落とす（原文の字下げは尊重）
    QString t = line.mid(1);
    if (!t.isEmpty() && t.at(0).isSpace()) t.remove(0, 1);

    // 既知の生成ツールのバナーは捨てる
    if (t.startsWith(QLatin1String("---- Kifu for Windows"))) return QString();

    // Floodgate/CSA v3系の評価・読み筋（"** ..."）は本文コメントには載せない
    const QString trimmed = t.trimmed();
    if (trimmed.startsWith(QLatin1String("**"))) return QString();

    // 「'*' だけ」の行は空行（段落区切り）として残す
    if (trimmed == QLatin1String("*")) return QString("");

    // 先頭に '*' がある場合は装飾とみなし1個だけ除去し、続くスペースも1個除去
    if (!t.isEmpty() && t.at(0) == QLatin1Char('*')) {
        t.remove(0, 1);
        if (!t.isEmpty() && t.at(0).isSpace()) t.remove(0, 1);
    }

    // CRLF→LF 正規化（既に読み込み側で置換済みなら実害なし）
    t.replace("\r\n", "\n");
    return t;
}

// ---- Board: 平手初期配置 ----
void CsaToSfenConverter::Board::setHirate()
{
    // クリア
    for (int x = 1; x <= 9; ++x) {
        for (int y = 1; y <= 9; ++y) {
            sq[x][y] = Cell{};
        }
    }
    // 白（後手）陣（y=1: L N S G K G S N L）
    Piece back[9] = { KY, KE, GI, KI, OU, KI, GI, KE, KY };
    for (int x = 1; x <= 9; ++x) { sq[x][1].p = back[x-1]; sq[x][1].c = White; }
    // 白：y=2 角(2,2), 飛(8,2)
    sq[2][2] = { KA, White }; sq[8][2] = { HI, White };
    // 白：歩 y=3
    for (int x = 1; x <= 9; ++x) { sq[x][3] = { FU, White }; }

    // 黒：歩 y=7
    for (int x = 1; x <= 9; ++x) { sq[x][7] = { FU, Black }; }
    // 黒：y=8 飛(2,8), 角(8,8)
    sq[2][8] = { HI, Black }; sq[8][8] = { KA, Black };
    // 黒（先手）陣（y=9: L N S G K G S N L）
    for (int x = 1; x <= 9; ++x) { sq[x][9].p = back[x-1]; sq[x][9].c = Black; }
}

// ---- ファイル読込 ----
bool CsaToSfenConverter::readAllLinesDetectEncoding_(const QString& path, QStringList& outLines, QString* warn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("Failed to open: %1\n").arg(path);
        return false;
    }

    const QByteArray raw = f.readAll();
    const bool utf8Header = raw.left(128).contains("CSA encoding=UTF-8");

    QString text;

    if (utf8Header) {
        QStringDecoder dec(QStringDecoder::Utf8);
        text = dec(raw);
    } else {
        QStringDecoder decUtf8(QStringDecoder::Utf8);
        text = decUtf8(raw);
        if (decUtf8.hasError()) {
            QStringDecoder decSjis("Shift-JIS");
            if (!decSjis.isValid())
                decSjis = QStringDecoder("CP932");
            if (decSjis.isValid()) {
                text = decSjis(raw);
            } else {
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    text.replace("\r\n", "\n");
    text.replace("\r", "\n");
    outLines = text.split('\n', Qt::KeepEmptyParts);
    return true;
}

bool CsaToSfenConverter::isMoveLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('+') || c == QLatin1Char('-'));
}
bool CsaToSfenConverter::isResultLine_(const QString& s)
{
    return s.startsWith(QLatin1Char('%'));
}
bool CsaToSfenConverter::isMetaLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('V') || c == QLatin1Char('$') || c == QLatin1Char('N'));
}
bool CsaToSfenConverter::isCommentLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    return (s.startsWith('\''));
}

// ---- 開始局面解析 ----
bool CsaToSfenConverter::parseStartPos_(const QStringList& lines, int& idx,
                                        QString& baseSfen, Color& stm, Board& board)
{
    board.setHirate();
    stm = Black;

    auto sfenOfSide = [](Color c)->QString {
        return c == Black ? QStringLiteral(" b - 1")
                          : QStringLiteral(" w - 1");
    };
    const QString boardSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    bool sawPI = false;
    for (int i = idx; i < lines.size(); ++i) {
        const QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) continue;
        if (isMetaLine_(raw) || isCommentLine_(raw)) continue;

        if (raw == QLatin1String("PI")) { // 平手
            sawPI = true;
            ++i;
            if (i < lines.size()) {
                const QString nxt = lines.at(i).trimmed();
                if (nxt == QLatin1String("+")) { stm = Black; }
                else if (nxt == QLatin1String("-")) { stm = White; }
                else { --i; }
            }
            idx = i + 1;
            break;
        } else if (raw == QLatin1String("+")) {
            stm = Black;
            idx = i + 1;
            break;
        } else if (raw == QLatin1String("-")) {
            stm = White;
            idx = i + 1;
            break;
        } else if (isMoveLine_(raw) || isResultLine_(raw)) {
            idx = i;
            break;
        } else {
            continue;
        }
    }

    baseSfen = boardSfen + sfenOfSide(stm);
    Q_UNUSED(sawPI);
    return true;
}

// ---- CSAトークンを USI 1手 にして盤も更新 ----
bool CsaToSfenConverter::parseMoveLine_(const QString& line, Color mover, Board& b,
                                        int& prevTx, int& prevTy,
                                        QString& usiMoveOut, QString& prettyOut, QString* warn)
{
    const int comma = line.indexOf(QLatin1Char(','));
    const QString token = (comma >= 0) ? line.left(comma) : line;

    int fx=0, fy=0, tx=0, ty=0;
    Piece after = NO_P;
    if (!parseCsaMoveToken_(token, fx, fy, tx, ty, after)) {
        if (warn) *warn += QStringLiteral("Malformed move token: %1\n").arg(token);
        return false;
    }

    const bool isDrop = (fx == 0 && fy == 0);

    if (!inside_(tx) || !inside_(ty)) {
        if (warn) *warn += QStringLiteral("Destination out of range: %1\n").arg(token);
        return false;
    }

    // 盤上の元の駒
    Piece beforePiece = NO_P;
    bool  beforeProm  = false;
    if (!isDrop) {
        if (!inside_(fx) || !inside_(fy)) {
            if (warn) *warn += QStringLiteral("Source out of range: %1\n").arg(token);
            return false;
        }
        const Cell from = b.sq[fx][fy];
        beforePiece = from.p;
        beforeProm  = isPromotedPiece_(from.p);
    }

    // 成り判定
    bool promote = false;
    if (!isDrop) {
        if (isPromotedPiece_(after)) {
            const Piece base = basePieceOf_(after);
            if (!beforeProm && base == basePieceOf_(beforePiece)) {
                promote = true;
            }
        }
    }

    // USI生成
    const QString toSq = toUsiSquare_(tx, ty);
    QString usi;
    if (isDrop) {
        QString usiPiece;
        switch (after) {
        case FU: usiPiece = QStringLiteral("P"); break;
        case KY: usiPiece = QStringLiteral("L"); break;
        case KE: usiPiece = QStringLiteral("N"); break;
        case GI: usiPiece = QStringLiteral("S"); break;
        case KI: usiPiece = QStringLiteral("G"); break;
        case KA: usiPiece = QStringLiteral("B"); break;
        case HI: usiPiece = QStringLiteral("R"); break;
        default: usiPiece = QStringLiteral("P"); break; // 保険
        }
        usi = usiPiece + QStringLiteral("*") + toSq;
    } else {
        const QString fromSq = toUsiSquare_(fx, fy);
        usi = fromSq + toSq + (promote ? QStringLiteral("+") : QString());
    }
    usiMoveOut = usi;

    // pretty生成
    const QString sideMark = (mover == Black) ? QStringLiteral("▲") : QStringLiteral("△");

    if (isDrop) {
        const QString pj = pieceKanji_(after);
        const QString dest = zenkakuDigit_(tx) + kanjiRank_(ty);
        prettyOut = sideMark + dest + pj + QStringLiteral("打");
    } else {
        const QString pj = pieceKanji_(basePieceOf_(beforePiece));

        // 同一座標チェック
        QString dest;
        if (tx == prevTx && ty == prevTy) {
            dest = QStringLiteral("同　");
        } else {
            dest = zenkakuDigit_(tx) + kanjiRank_(ty);
        }

        prettyOut = sideMark + dest + pj;

        // 成りの場合に「成」を付与
        if (promote) {
            prettyOut += QStringLiteral("成");
        }

        prettyOut += QStringLiteral("(")
                    + QString::number(fx) + QString::number(fy) + QStringLiteral(")");
    }

    // 盤更新
    if (!isDrop) {
        b.sq[tx][ty] = { after, mover };
        b.sq[fx][fy] = Cell{};
    } else {
        b.sq[tx][ty] = { after, mover };
    }

    // 直前の着手位置を更新
    prevTx = tx;
    prevTy = ty;

    return true;
}

bool CsaToSfenConverter::parseCsaMoveToken_(const QString& token,
                                            int& fx, int& fy, int& tx, int& ty,
                                            Piece& afterPiece)
{
    if (token.size() < 7) return false;

    const QChar sign = token.at(0);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    auto d = [](QChar ch)->int { const int v = ch.digitValue(); return (v >= 0 && v <= 9) ? v : -1; };

    fx = d(token.at(1));
    fy = d(token.at(2));
    tx = d(token.at(3));
    ty = d(token.at(4));

    if (tx < 1 || tx > 9 || ty < 1 || ty > 9) return false;

    const bool isDrop = (fx == 0 && fy == 0);
    if (!isDrop) {
        if (fx < 1 || fx > 9 || fy < 1 || fy > 9) return false;
    }

    const QStringView p = QStringView{token}.mid(5, 2);

    if      (p == u"FU") afterPiece = FU;
    else if (p == u"KY") afterPiece = KY;
    else if (p == u"KE") afterPiece = KE;
    else if (p == u"GI") afterPiece = GI;
    else if (p == u"KI") afterPiece = KI;
    else if (p == u"KA") afterPiece = KA;
    else if (p == u"HI") afterPiece = HI;
    else if (p == u"OU") afterPiece = OU;
    else if (p == u"TO") afterPiece = TO;
    else if (p == u"NY") afterPiece = NY;
    else if (p == u"NK") afterPiece = NK;
    else if (p == u"NG") afterPiece = NG;
    else if (p == u"UM") afterPiece = UM;
    else if (p == u"RY") afterPiece = RY;
    else return false;

    return true;
}

// ---- 文字変換 ----
QChar CsaToSfenConverter::usiRankLetter_(int y)
{
    static const char table[10] = {0,'a','b','c','d','e','f','g','h','i'};
    if (y < 1 || y > 9) return QLatin1Char('a');
    return QLatin1Char(table[y]);
}
QString CsaToSfenConverter::toUsiSquare_(int x, int y)
{
    return QString::number(x) + usiRankLetter_(y);
}
bool CsaToSfenConverter::isPromotedPiece_(Piece p)
{
    return (p == TO || p == NY || p == NK || p == NG || p == UM || p == RY);
}
CsaToSfenConverter::Piece CsaToSfenConverter::basePieceOf_(Piece p)
{
    switch (p) {
    case TO: return FU; case NY: return KY; case NK: return KE; case NG: return GI;
    case UM: return KA; case RY: return HI;
    default: return p;
    }
}
QString CsaToSfenConverter::pieceKanji_(Piece p)
{
    switch (p) {
    case FU: return QStringLiteral("歩");
    case KY: return QStringLiteral("香");
    case KE: return QStringLiteral("桂");
    case GI: return QStringLiteral("銀");
    case KI: return QStringLiteral("金");
    case KA: return QStringLiteral("角");
    case HI: return QStringLiteral("飛");
    case OU: return QStringLiteral("玉");
    case TO: return QStringLiteral("と");
    case NY: return QStringLiteral("成香");
    case NK: return QStringLiteral("成桂");
    case NG: return QStringLiteral("成銀");
    case UM: return QStringLiteral("馬");
    case RY: return QStringLiteral("龍");
    default: return QStringLiteral("歩");
    }
}
QString CsaToSfenConverter::zenkakuDigit_(int d)
{
    static const QChar map[] = {
        QChar(u'０'), // 0
        QChar(u'１'), // 1
        QChar(u'２'), // 2
        QChar(u'３'), // 3
        QChar(u'４'), // 4
        QChar(u'５'), // 5
        QChar(u'６'), // 6
        QChar(u'７'), // 7
        QChar(u'８'), // 8
        QChar(u'９')  // 9
    };

    if (d >= 1 && d <= 9) {
        return QString(map[d]);
    }
    return QString(QChar(u'？'));
}
QString CsaToSfenConverter::kanjiRank_(int y)
{
    static const QChar map[10] = { QChar(), QChar(u'一'), QChar(u'二'), QChar(u'三'), QChar(u'四'),
                                   QChar(u'五'), QChar(u'六'), QChar(u'七'), QChar(u'八'), QChar(u'九') };
    if (y >= 1 && y <= 9) return QString(map[y]);
    return QString(QChar(u'？'));
}
bool CsaToSfenConverter::inside_(int v) { return v >= 1 && v <= 9; }

// ---- エントリポイント ----
bool CsaToSfenConverter::parse(const QString& filePath, KifParseResult& out, QString* warn)
{
    QStringList lines;
    if (!readAllLinesDetectEncoding_(filePath, lines, warn)) return false;

    // out を毎回初期化
    out.mainline.baseSfen.clear();
    out.mainline.usiMoves.clear();
    out.mainline.disp.clear();
    out.variations.clear(); // CSAは分岐を持たない

    // 開始局面と手番
    int   idx  = 0;
    Color stm  = Black;
    Board board; board.setHirate();
    if (!parseStartPos_(lines, idx, out.mainline.baseSfen, stm, board)) {
        if (warn) *warn += QStringLiteral("Failed to parse start position.\n");
        return false;
    }

    // 直前の着手先座標（連続移動の"同"判定用などで既に使っている想定）
    int prevTx = -1, prevTy = -1;

    // コメントの一時バッファ（最初の指し手が出るまで蓄積）
    QStringList pendingComments;

    // 指し手を読む
    Color turn = stm;
    for (int i = idx; i < lines.size(); ++i) {
        QString s = lines.at(i);
        if (s.isEmpty()) continue;
        s.replace("\r\n", "\n");
        s = s.trimmed();
        if (s.isEmpty()) continue;

        // ---- コメント行（先頭が'）は「本文コメント」として扱う ----
        if (isCommentLine_(s)) {
            const QString norm = normalizeCsaCommentLine_(s);
            if (!norm.isNull()) { // null=完全無視
                if (out.mainline.disp.isEmpty()) {
                    // まだ1手も出ていない → 最初の指し手へ付与するため一旦貯める
                    pendingComments.append(norm);
                } else {
                    // すでに本譜が始まっている → 直前の指し手に追記
                    QString &dst = out.mainline.disp.last().comment;
                    if (!dst.isEmpty() && !norm.isEmpty())
                        dst += QLatin1Char('\n');
                    dst += norm; // normが空文字なら「空行（段落）」として改行効果のみ
                }
            }
            continue;
        }

        // メタ行（V/$/N）はスキップ
        if (isMetaLine_(s)) continue;

        // 単独の "+" / "-" は手番指示
        if (s.size() == 1 && (s[0] == QLatin1Char('+') || s[0] == QLatin1Char('-'))) {
            turn = (s[0] == QLatin1Char('+')) ? Black : White;
            continue;
        }

        // 終局コード
        if (isResultLine_(s)) {
            const QString sideMark = (turn == Black) ? QStringLiteral("▲") : QStringLiteral("△");
            QString label;
            if      (s.startsWith(QLatin1String("%TORYO")))           label = QStringLiteral("投了");
            else if (s.startsWith(QLatin1String("%CHUDAN")))          label = QStringLiteral("中断");
            else if (s.startsWith(QLatin1String("%SENNICHITE")))      label = QStringLiteral("千日手");
            else if (s.startsWith(QLatin1String("%JISHOGI")))         label = QStringLiteral("持将棋");
            else if (s.startsWith(QLatin1String("%TIME_UP")))         label = QStringLiteral("切れ負け");
            else if (s.startsWith(QLatin1String("%ILLEGAL_MOVE")))    label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%+ILLEGAL_ACTION"))) label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%-ILLEGAL_ACTION"))) label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%KACHI")))           label = QStringLiteral("入玉勝ち");
            else if (s.startsWith(QLatin1String("%TSUMI")))           label = QStringLiteral("詰み");
            else if (s.startsWith(QLatin1String("%FUZUMI")))          label = QStringLiteral("不詰");
            else if (s.startsWith(QLatin1String("%MATTA")))           label = QStringLiteral("待った");
            else                                                      label = s; // 不明コードはそのまま

            KifDisplayItem di;
            di.prettyMove = sideMark + label;
            if (!pendingComments.isEmpty()) {
                di.comment = pendingComments.join(QStringLiteral("\n"));
                pendingComments.clear();
            }
            out.mainline.disp.append(di);
            break; // 以降は読まない
        }

        // 通常の指し手
        if (isMoveLine_(s)) {
            const QChar head  = s.at(0);
            const Color mover = (head == QLatin1Char('+')) ? Black : White;

            QString usi, pretty;
            if (!parseMoveLine_(s, mover, board, prevTx, prevTy, usi, pretty, warn)) {
                if (warn) *warn += QStringLiteral("Failed to parse move line: %1\n").arg(s);
                return false;
            }
            out.mainline.usiMoves.append(usi);

            KifDisplayItem di;
            di.prettyMove = pretty;
            if (!pendingComments.isEmpty()) {
                di.comment = pendingComments.join(QStringLiteral("\n"));
                pendingComments.clear();
            }
            out.mainline.disp.append(di);

            // 次手番へ
            turn = (mover == Black) ? White : Black;
            continue;
        }

        // ここまでに該当しない行は読み飛ばし
    }

    return true;
}

// CSAファイルからヘッダー情報を抽出する
QList<KifGameInfoItem> CsaToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> items;
    QStringList lines;
    QString warn;

    // 既存のエンコーディング自動判定付き読み込み関数を利用
    if (!readAllLinesDetectEncoding_(filePath, lines, &warn)) {
        return items;
    }

    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;

        // 指し手が始まったらヘッダー終了とみなして打ち切り
        // (isMoveLine_ は行頭が +, - かどうかを判定する既存関数)
        if (isMoveLine_(line)) break;

        // 盤面定義行 (PI, P1, P+ 等) が始まったらヘッダー終了
        if (line.startsWith(QLatin1Char('P'))) break;

        // --- CSAヘッダー解析 ---

        // "N+": 先手名
        if (line.startsWith(QLatin1String("N+"))) {
            items.append({ QStringLiteral("先手"), line.mid(2).trimmed() });
        }
        // "N-": 後手名
        else if (line.startsWith(QLatin1String("N-"))) {
            items.append({ QStringLiteral("後手"), line.mid(2).trimmed() });
        }
        // "$KEY:VALUE" 形式（棋戦名、場所、時間など）
        else if (line.startsWith(QLatin1Char('$'))) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                QString key = line.mid(1, colon - 1);
                const QString val = line.mid(colon + 1).trimmed();

                // 一般的なキーを日本語表記にマップ（KIF形式の表示名と合わせる）
                if (key == QLatin1String("EVENT"))      key = QStringLiteral("棋戦");
                else if (key == QLatin1String("SITE"))       key = QStringLiteral("場所");
                else if (key == QLatin1String("START_TIME")) key = QStringLiteral("開始日時");
                else if (key == QLatin1String("END_TIME"))   key = QStringLiteral("終了日時");
                else if (key == QLatin1String("TIME_LIMIT")) key = QStringLiteral("持ち時間");
                else if (key == QLatin1String("OPENING"))    key = QStringLiteral("戦型");

                items.append({ key, val });
            }
        }
        // "V": バージョン情報 (例: V2.2) - 必要であれば追加
        else if (line.startsWith(QLatin1Char('V'))) {
            items.append({ QStringLiteral("バージョン"), line });
        }
        // コメント行 "'" は無視して続行
    }

    return items;
}
