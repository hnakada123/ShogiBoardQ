#include "ki2tosfenconverter.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QtGlobal>

// ---------------- 盤表現（最小限） ----------------
namespace {

// USI用の段（1..9）→ 'a'..'i'
static inline QChar rankToLetter(int rank1to9) { return QChar('a' + (rank1to9 - 1)); }

// 先後の区別：先手=大文字、後手=小文字
static inline bool isBlackPiece(QChar pc) { return pc.isUpper(); }
static inline bool isWhitePiece(QChar pc) { return pc.isLower(); }

// 成駒表現（USI相当）：TO, NY, NK, NG, UM, RY を 2 文字で表現
struct Piece2 { QChar a, b; }; // a='T',b='O' 等。1文字駒は a に駒字、b='\0'

static inline bool isPromotedPair(const Piece2& p){ return p.b != QChar('\0'); }

static inline Piece2 toPromoted(QChar base){
    // base は大文字。返り値も大文字（先手）で返す
    if (base == QChar('P')) return { QChar('T'), QChar('O') };
    if (base == QChar('L')) return { QChar('N'), QChar('Y') };
    if (base == QChar('N')) return { QChar('N'), QChar('K') };
    if (base == QChar('S')) return { QChar('N'), QChar('G') };
    if (base == QChar('B')) return { QChar('U'), QChar('M') };
    if (base == QChar('R')) return { QChar('R'), QChar('Y') };
    return { base, QChar('\0') };
}

static inline QChar demoteFromTaken(QChar pc){
    // 相手駒を取って持ち駒化する際の駒種へ（常に大文字に）
    QChar u = pc.toUpper();
    if (u == QChar('T')) return QChar('P');
    if (u == QChar('N')) return QChar('L'); // NY は L, NK は N だが、ここは曖昧性排除のため後で判定
    if (u == QChar('G')) return QChar('S');
    if (u == QChar('U')) return QChar('B');
    if (u == QChar('R')) return QChar('R');
    // 'Y','K','M' などは直前の 'N','U','R' と組で扱うため上の分岐で吸収
    // 単独 'K' は玉→持ち駒にならない
    if (u == QChar('L') || u == QChar('N') || u == QChar('S') || u == QChar('B') || u == QChar('R') || u == QChar('P'))
        return u;
    return u;
}

// 盤（1..9 の行列）。cell[r][f] に駒字（先手: 大文字, 後手: 小文字, 空: '\0'）
struct Board {
    QChar cell[10][10];

    // 持ち駒（先手/後手） P,L,N,S,G,B,R の順
    int handB[7] = {0};
    int handW[7] = {0};

    static int idxOf(QChar base){
        if (base == QChar('P')) return 0;
        if (base == QChar('L')) return 1;
        if (base == QChar('N')) return 2;
        if (base == QChar('S')) return 3;
        if (base == QChar('G')) return 4;
        if (base == QChar('B')) return 5;
        if (base == QChar('R')) return 6;
        return 0;
    }

    void clear(){
        for (int r=1;r<=9;++r){
            for (int f=1;f<=9;++f) cell[r][f] = QChar('\0');
        }
        for (int i=0;i<7;++i){ handB[i]=0; handW[i]=0; }
    }

    void setHirate(){
        clear();
        // 上段（先後の向き：r=1 が上（後手側）、r=9 が下（先手側））
        // SFEN: lnsgkgsnl/1r5b1/p1ppppppp/9/9/9/P1PPPPPPP/1B5R1/LNSGKGSNL
        const char* rows[9] = {
            "lnsgkgsnl", "1r5b1", "p1ppppppp", "9", "9", "9", "P1PPPPPPP", "1B5R1", "LNSGKGSNL"
        };
        loadFromSfenRows(rows);
    }

    void loadFromSfenString(const QString& sfen){
        clear();
        const QString board = sfen.section(' ', 0, 0); // 盤面部のみ抽出
        const QStringList rows = board.split('/');
        if (rows.size() != 9){ setHirate(); return; }
        for (int ir=0; ir<9; ++ir){
            const QString& row = rows.at(ir);
            int r = ir + 1; // 1..9
            int f = 9;      // SFEN 左→右 が f=9..1
            for (int i=0; i<row.size(); ++i){
                const QChar ch = row.at(i);
                if (ch.isDigit()){
                    const int n = ch.unicode() - '0';
                    f -= n;
                } else {
                    cell[r][f] = ch;
                    --f;
                }
            }
        }
    }

    void loadFromSfenRows(const char* rows[9]){
        clear();
        for (int ir=0; ir<9; ++ir){
            const char* row = rows[ir];
            int r = ir + 1;
            int f = 9;
            for (int i=0; row[i]; ++i){
                const char ch = row[i];
                if (ch >= '0' && ch <= '9'){
                    f -= (ch - '0');
                } else {
                    cell[r][f] = QChar(ch);
                    --f;
                }
            }
        }
    }

    QChar at(int r,int f) const { return cell[r][f]; }
    void set(int r,int f,QChar pc){ cell[r][f] = pc; }

    // 駒の利き（単純化した合法性チェック）。成金=金相当、馬/竜は角/飛＋王
    static bool knightCan(int r0,int f0,int r1,int f1,bool black){
        const int dr = r1 - r0;
        const int df = f1 - f0;
        if (black){ return (dr == -2 && (df == -1 || df == 1)); }
        else      { return (dr ==  2 && (df == -1 || df == 1)); }
    }

    static bool goldCan(int r0,int f0,int r1,int f1,bool black){
        const int dr = r1 - r0, df = f1 - f0;
        if (black){
            return ( (dr==-1 && (df==-1||df==0||df==1)) || (dr==0 && (df==-1||df==1)) || (dr==1 && df==0) );
        } else {
            return ( (dr== 1 && (df==-1||df==0||df==1)) || (dr==0 && (df==-1||df==1)) || (dr==-1 && df==0) );
        }
    }

    static bool kingCan(int r0,int f0,int r1,int f1){
        const int dr = qAbs(r1-r0), df = qAbs(f1-f0);
        return dr<=1 && df<=1 && (dr+df)>0;
    }

    static bool silverCan(int r0,int f0,int r1,int f1,bool black){
        const int dr = r1 - r0, df = f1 - f0;
        if (black){ return ( (dr==-1 && (df==-1||df==0||df==1)) || (dr==1 && qAbs(df)==1) ); }
        else      { return ( (dr== 1 && (df==-1||df==0||df==1)) || (dr==-1 && qAbs(df)==1) ); }
    }

    static bool pawnCan(int r0,int f0,int r1,int f1,bool black){
        const int dr = r1 - r0, df = f1 - f0;
        return (df==0) && (black ? (dr==-1) : (dr==1));
    }

    bool lineClear(int r0,int f0,int r1,int f1) const {
        const int dr = (r1>r0)?1:((r1<r0)?-1:0);
        const int df = (f1>f0)?1:((f1<f0)?-1:0);
        int r=r0+dr, f=f0+df;
        while (r!=r1 || f!=f1){ if (cell[r][f] != QChar('\0')) return false; r+=dr; f+=df; }
        return true;
    }

    static bool lanceCan(int r0,int f0,int r1,int f1,bool black){
        if (f0!=f1) return false;
        return black ? (r1<r0) : (r1>r0);
    }

    static bool bishopShape(int r0,int f0,int r1,int f1){ return qAbs(r1-r0)==qAbs(f1-f0); }
    static bool rookShape(int r0,int f0,int r1,int f1){ return (r0==r1)||(f0==f1); }

    // 指定の先手/後手・駒種で、to(r1,f1) へ合法に移動可能な元位置を 1 個だけ特定
    bool findUniqueOrigin(QChar base, bool promoted, bool black, int r1, int f1,
                          int& r0, int& f0) const
    {
        // 探索順は適当。候補が 1 つに特定できた時点で true
        for (int r=1; r<=9; ++r){
            for (int f=1; f<=9; ++f){
                const QChar pc = cell[r][f];
                if (pc == QChar('\0')) continue;
                if (black && !isBlackPiece(pc)) continue;
                if (!black && !isWhitePiece(pc)) continue;
                const QChar up = pc.toUpper();
                // base と一致するか（成駒は toUpper してから base と比較）
                const bool sameBase = (up==base) ||
                                      ( (promoted && ((base==QChar('P') && up==QChar('T')) ||
                                                     (base==QChar('L') && up==QChar('N')) ||
                                                     (base==QChar('N') && up==QChar('N')) ||
                                                     (base==QChar('S') && up==QChar('N')) ||
                                                     (base==QChar('B') && up==QChar('U')) ||
                                                     (base==QChar('R') && up==QChar('R')))) );
                if (!sameBase) continue;

                // 目的地が自駒で塞がれていないか
                const QChar dst = cell[r1][f1];
                if (dst != QChar('\0')){
                    if (black && isBlackPiece(dst)) continue;
                    if (!black && isWhitePiece(dst)) continue;
                }

                // 駒ごとの動き（成駒は金 or 角+王 / 飛+王）
                bool ok = false;
                if (!promoted){
                    if (base==QChar('P')) ok = pawnCan(r,f,r1,f1,black);
                    else if (base==QChar('L')) ok = lanceCan(r,f,r1,f1,black) && lineClear(r,f,r1,f1);
                    else if (base==QChar('N')) ok = knightCan(r,f,r1,f1,black);
                    else if (base==QChar('S')) ok = silverCan(r,f,r1,f1,black);
                    else if (base==QChar('G')) ok = goldCan(r,f,r1,f1,black);
                    else if (base==QChar('B')) ok = bishopShape(r,f,r1,f1) && lineClear(r,f,r1,f1);
                    else if (base==QChar('R')) ok = rookShape(r,f,r1,f1) && lineClear(r,f,r1,f1);
                    else if (base==QChar('K')) ok = kingCan(r,f,r1,f1);
                } else {
                    if (base==QChar('B')) ok = (bishopShape(r,f,r1,f1) && lineClear(r,f,r1,f1)) || kingCan(r,f,r1,f1); // 馬
                    else if (base==QChar('R')) ok = (rookShape(r,f,r1,f1) && lineClear(r,f,r1,f1)) || kingCan(r,f,r1,f1); // 竜
                    else ok = goldCan(r,f,r1,f1,black); // と・成香・成桂・成銀
                }
                if (!ok) continue;

                // 到達経路の遮り（飛角香）
                if (!promoted){
                    if (base==QChar('B') || base==QChar('R') || base==QChar('L')){
                        if (!lineClear(r,f,r1,f1)) continue;
                    }
                } else {
                    if (base==QChar('B') && bishopShape(r,f,r1,f1) && !lineClear(r,f,r1,f1)) continue;
                    if (base==QChar('R') && rookShape(r,f,r1,f1) && !lineClear(r,f,r1,f1)) continue;
                }

                // 候補を 1 つ確保。二重に見つかったら曖昧として失敗（KI2 では右/左/直…のヒントを未使用）
                if (r0==-1){ r0=r; f0=f; }
                else { return false; } // 複数候補→曖昧
            }
        }
        return (r0!=-1);
    }

    // 移動の適用（取り・成り・打ちに対応）
    void applyMove(bool black, QChar base, bool promote,
                   bool isDrop, int r0, int f0, int r1, int f1)
    {
        if (isDrop){
            // 手持ちを 1 減らして盤上に置く
            const int idx = idxOf(base);
            if (black){ if (handB[idx]>0) --handB[idx]; }
            else       { if (handW[idx]>0) --handW[idx]; }
            cell[r1][f1] = black ? base : base.toLower();
            return;
        }
        // 取り
        if (cell[r1][f1] != QChar('\0')){
            const QChar cap = cell[r1][f1];
            QChar dropBase = demoteFromTaken(cap);
            const int idx = idxOf(dropBase);
            if (black) ++handB[idx]; else ++handW[idx];
        }
        // 移動
        QChar mover = cell[r0][f0];
        cell[r0][f0] = QChar('\0');
        if (promote){
            // base → 成駒文字へ
            if (base==QChar('B'))      mover = black ? QChar('U') : QChar('u'); // 馬
            else if (base==QChar('R')) mover = black ? QChar('R') : QChar('r'); // 竜（Rは同字、見た目は後で+）
            else if (base==QChar('P')) mover = black ? QChar('T') : QChar('t');
            else if (base==QChar('L')) mover = black ? QChar('N') : QChar('n');
            else if (base==QChar('N')) mover = black ? QChar('N') : QChar('n');
            else if (base==QChar('S')) mover = black ? QChar('N') : QChar('n');
        }
        cell[r1][f1] = mover;
    }
};

// --- ユーティリティ ---
static inline int zenkakuDigitToInt(QChar z){
    static const QString Z = QStringLiteral("１２３４５６７８９");
    const int idx = Z.indexOf(z);
    return (idx>=0) ? (idx+1) : -1;
}
static inline int kanjiDigitToInt(QChar z){
    static const QString K = QStringLiteral("一二三四五六七八九");
    const int idx = K.indexOf(z);
    return (idx>=0) ? (idx+1) : -1;
}

} // namespace

// ------------------ Ki2ToSfenConverter 実装 ------------------

bool Ki2ToSfenConverter::readAllLinesDetectEncoding_(const QString& filePath,
                                                     QStringList& outLines,
                                                     QString* warn)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)){
        if (warn) *warn += QStringLiteral("Cannot open: ") + filePath + QLatin1Char('\n');
        return false;
    }

    QTextStream ts(&f);
    ts.setAutoDetectUnicode(true); // BOM のみ自動


    outLines.clear();
    while (!ts.atEnd()){
        const QString line = ts.readLine();
        outLines.push_back(line);
    }
    return true;
}

int Ki2ToSfenConverter::findMainlineStart_(const QStringList& lines)
{
    // BOD ブロックや "手合割" を過ぎた最初の非空行を本譜開始とみなす
    bool inBod = false;
    for (int i=0; i<lines.size(); ++i){
        const QString s = lines.at(i).trimmed();
        if (s.isEmpty()) continue;
        if (s == QStringLiteral("*")) continue; // コメント行単独
        if (s.startsWith(QStringLiteral("手合割"))) { continue; }
        if (s.startsWith(QStringLiteral("後手の持駒")) || s.startsWith(QStringLiteral("先手の持駒"))) { continue; }
        if (s == QStringLiteral("後手の持駒：なし") || s == QStringLiteral("先手の持駒：なし")) { continue; }
        if (s.startsWith(QStringLiteral("後手の持駒：")) || s.startsWith(QStringLiteral("先手の持駒："))) { continue; }
        if (s.startsWith(QStringLiteral("手数---"))) { continue; }
        if (s == QStringLiteral("後手：") || s == QStringLiteral("先手：")) { continue; }
        if (s == QStringLiteral("後手番") || s == QStringLiteral("先手番")) { continue; }
        if (s.startsWith(QStringLiteral("後手の手番")) || s.startsWith(QStringLiteral("先手の手番"))) { continue; }

        // BOD 構文のごく簡単な検出（行頭に "P" が続く）
        if (s.startsWith(QLatin1Char('P'))){ inBod = true; continue; }
        if (inBod){
            if (!s.startsWith(QLatin1Char('P'))) { inBod = false; }
            else continue; // BOD 継続
        }
        if (!inBod){ return i; }
    }
    return 0;
}

QString Ki2ToSfenConverter::decideInitialSfen_(const QStringList& lines, int& startIndex, QString* warn)
{
    // 1) 手合割の簡易判定（見つかれば優先）
    QString teai;
    for (int i=0; i<lines.size(); ++i){
        const QString s = lines.at(i).trimmed();
        if (s.startsWith(QStringLiteral("手合割"))){
            const int pos = s.indexOf(QChar(u'：'));
            teai = (pos>=0) ? s.mid(pos+1).trimmed() : s.mid(QStringLiteral("手合割").size()).trimmed();
            break;
        }
    }

    // 代表的な SFEN を用意（必要に応じて今後追加）
    const QString SFEN_HIRATE = QStringLiteral("lnsgkgsnl/1r5b1/p1ppppppp/9/9/9/P1PPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString SFEN_KAKU   = QStringLiteral("lnsgkgsnl/1r7/p1ppppppp/9/9/9/P1PPPPPPP/8/LNSGKGSNL b B - 1"); // 角落ち（簡易。持駒に B としているが盤から外すだけでも可）
    const QString SFEN_HISHA  = QStringLiteral("lnsgkgsnl/7b/p1ppppppp/9/9/9/P1PPPPPPP/8/LNSGKGSNL b R - 1");
    const QString SFEN_NIMAI  = QStringLiteral("lnsgkgsnl/9/p1ppppppp/9/9/9/P1PPPPPPP/8/LNSGKGSNL b RB - 1");

    QString base = SFEN_HIRATE;
    if (!teai.isEmpty()){
        if (teai.contains(QStringLiteral("平手"))) base = SFEN_HIRATE;
        else if (teai.contains(QStringLiteral("角落"))) base = SFEN_KAKU;
        else if (teai.contains(QStringLiteral("飛車落"))) base = SFEN_HISHA;
        else if (teai.contains(QStringLiteral("二枚落"))) base = SFEN_NIMAI;
        else {
            if (warn) *warn += QStringLiteral("[KI2] 未対応の手合割: ") + teai + QLatin1Char('\n');
        }
    }

    startIndex = findMainlineStart_(lines);
    return base;
}

void Ki2ToSfenConverter::extractKi2TokensFromLine_(const QString& line, QStringList& outTokens)
{
    // 例："▲７六歩   △３四歩   ▲２六歩" → ["▲７六歩","△３四歩","▲２六歩"]
    // 例："同　角成" → ["同角成"]（全角スペースは除去）
    outTokens.clear();
    if (line.trimmed().isEmpty()) return;

    QString s = line;
    s.replace(QChar(0x3000), QLatin1Char(' ')); // 全角スペース→半角

    const QString heads = QStringLiteral("▲△☗☖同１２３４５６７８９一二三四五六七八九");

    QString cur;
    for (int i=0; i<s.size(); ++i){
        const QChar ch = s.at(i);
        const bool isHead = heads.contains(ch);
        if (isHead){
            if (!cur.trimmed().isEmpty()) outTokens.push_back(cur.trimmed());
            cur.clear();
        }
        cur.append(ch);
    }
    if (!cur.trimmed().isEmpty()) outTokens.push_back(cur.trimmed());

    // 末尾のゴミ（コメント記号など）を落とす簡易処理
    for (int i=0; i<outTokens.size(); ++i){
        outTokens[i] = normalizeToken_(outTokens.at(i));
    }
}

QString Ki2ToSfenConverter::normalizeToken_(QString t)
{
    t.replace(QChar(0x3000), QLatin1Char(' ')); // 全角スペース
    t.replace(QStringLiteral("　"), QString()); // 残存全角空白
    t.replace(QStringLiteral(" "), QString());  // 半角空白も基本除去（KI2は空白に意味が無い）
    return t;
}

bool Ki2ToSfenConverter::mapKanjiPiece_(const QString& s, QChar& base, bool& promoted)
{
    promoted = false;
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);

    // 成駒（と/杏/圭/全/馬/龍/竜）
    if (c == QChar(u'と')){ base = QChar('P'); promoted = true; return true; }
    if (c == QChar(u'杏')){ base = QChar('L'); promoted = true; return true; }
    if (c == QChar(u'圭')){ base = QChar('N'); promoted = true; return true; }
    if (c == QChar(u'全')){ base = QChar('S'); promoted = true; return true; }
    if (c == QChar(u'馬')){ base = QChar('B'); promoted = true; return true; }
    if (c == QChar(u'龍') || c == QChar(u'竜')){ base = QChar('R'); promoted = true; return true; }

    // 生駒（歩香桂銀金角飛玉）
    if (c == QChar(u'歩')){ base = QChar('P'); return true; }
    if (c == QChar(u'香')){ base = QChar('L'); return true; }
    if (c == QChar(u'桂')){ base = QChar('N'); return true; }
    if (c == QChar(u'銀')){ base = QChar('S'); return true; }
    if (c == QChar(u'金')){ base = QChar('G'); return true; }
    if (c == QChar(u'角')){ base = QChar('B'); return true; }
    if (c == QChar(u'飛')){ base = QChar('R'); return true; }
    if (c == QChar(u'玉')){ base = QChar('K'); return true; }

    return false;
}

void Ki2ToSfenConverter::parseDestSquare_(const QString& twoChars, int& file, int& rank, bool& isDou)
{
    file = -1; rank = -1; isDou = false;
    if (twoChars.isEmpty()) return;
    if (twoChars.startsWith(QStringLiteral("同"))){ isDou = true; return; }

    const QChar a = twoChars.size()>=1 ? twoChars.at(0) : QChar();
    const QChar b = twoChars.size()>=2 ? twoChars.at(1) : QChar();

    int f = zenkakuDigitToInt(a);
    if (f<1) f = kanjiDigitToInt(a);
    int r = kanjiDigitToInt(b);
    if (f>=1 && r>=1){ file = f; rank = r; }
}

bool Ki2ToSfenConverter::isTerminalWord_(const QString& t, QString& termLabel)
{
    static const QStringList words = {
        QStringLiteral("投了"), QStringLiteral("千日手"), QStringLiteral("中断"),
        QStringLiteral("持将棋"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け")
    };
    for (int i=0;i<words.size();++i){ if (t.contains(words.at(i))) { termLabel = words.at(i); return true; } }
    return false;
}

bool Ki2ToSfenConverter::parse(const QString& filePath,
                               KifParseResult& out,
                               QString* warn)
{
    QStringList lines;
    if (!readAllLinesDetectEncoding_(filePath, lines, warn)) return false;

    out.mainline.baseSfen.clear();
    out.mainline.usiMoves.clear();
    out.mainline.disp.clear();
    out.variations.clear(); // KI2 は分岐なし想定

    int startIdx = 0;
    const QString baseSfen = decideInitialSfen_(lines, startIdx, warn);
    out.mainline.baseSfen = baseSfen;

    // 盤を初期化
    Board bd; bd.loadFromSfenString(baseSfen);

    // コメント一時バッファ
    QString pendingComment; pendingComment.reserve(256);

    // 直前の着手先（「同」用）
    int lastToFile = -1, lastToRank = -1;

    // 手数と手番（先手始動想定。必要に応じてヘッダから変更）
    int ply = 0;

    for (int i = startIdx; i < lines.size(); ++i){
        const QString raw = lines.at(i);
        const QString s = raw.trimmed();
        if (s.isEmpty()) continue;

        if (s.startsWith(QLatin1Char('*'))) {
            // コメント行：次の指し手に付与（QStringViewでオーバーヘッド回避）
            if (!pendingComment.isEmpty()) pendingComment += QLatin1Char('\n');
            pendingComment += QStringView{s}.mid(1);  // ← s.mid(1) をやめる
            continue;
        }

        // 1 行に複数手
        QStringList toks; extractKi2TokensFromLine_(s, toks);
        if (toks.isEmpty()) continue;

        for (int ti=0; ti<toks.size(); ++ti){
            const QString tok = toks.at(ti);

            // 終局語
            QString term;
            if (isTerminalWord_(tok, term)){
                // 表示用アイテムだけ積む（USI は積まない）
                KifDisplayItem d;
                d.prettyMove = term; // 例："投了"
                d.comment = pendingComment; pendingComment.clear();
                out.mainline.disp.push_back(d);
                // 以降は読み飛ばす
                return true;
            }

            // 先後マーカー（▲/△/☗/☖）は表示に反映するだけで、手番は ply の偶奇で決定
            const bool black = ((ply % 2) == 0);

            // 例："▲７六歩" / "△３四歩" / "同角成" / "７七角成" / "４四歩打" など
            // マーカー除去
            QString t = tok;
            if (!t.isEmpty()){
                const QChar h = t.at(0);
                if (h==QChar(u'▲') || h==QChar(u'△') || h==QChar(u'☗') || h==QChar(u'☖')) t = t.mid(1);
            }

            // 目的地
            int toF=-1, toR=-1; bool isDou=false;
            if (t.size()>=2){ parseDestSquare_(t.left(2), toF, toR, isDou); }
            if (isDou){ toF = lastToFile; toR = lastToRank; }
            if (!(toF>=1 && toR>=1)){
                if (warn) *warn += QStringLiteral("[KI2] 目的地の解釈に失敗: ") + tok + QLatin1Char('\n');
                continue; // 無視して前進
            }

            // 駒種＋成り/打ち
            // 目的地字を落とした残りから駒と修飾を抽出
            QString rest = t.mid(isDou ? 1 : 2);
            bool forcedPromote = false;
            bool isDrop = false;

            // 成/不成/打 の語を除去しながらフラグ化
            if (rest.contains(QStringLiteral("成"))) { forcedPromote = true; rest.remove(QStringLiteral("成")); }
            if (rest.contains(QStringLiteral("不成"))) { forcedPromote = false; rest.remove(QStringLiteral("不成")); }
            if (rest.contains(QStringLiteral("打"))) { isDrop = true; rest.remove(QStringLiteral("打")); }
            // 右/左/直/寄/上/引 は曖昧解消ヒントだが、初版では未使用のため除去のみ
            static const QString hints = QStringLiteral("右左直寄上引");
            QString rest2; rest2.reserve(rest.size());
            for (int k=0;k<rest.size();++k){ if (!hints.contains(rest.at(k))) rest2.append(rest.at(k)); }
            rest = rest2;

            // 駒種
            QChar base('P'); bool promotedWord=false;
            if (!mapKanjiPiece_(rest, base, promotedWord)){
                if (warn) *warn += QStringLiteral("[KI2] 駒種の解釈に失敗: ") + tok + QLatin1Char('\n');
                continue;
            }
            bool doPromote = forcedPromote || promotedWord;

            // 打ち（持ち駒）
            int fromR=-1, fromF=-1;
            if (isDrop){
                // from は不要。USI は "P*7f" のように * 表記
            } else {
                // 盤から移動元を一意に特定
                fromR = -1; fromF = -1;
                if (!bd.findUniqueOrigin(base, promotedWord /*既に成駒として表記されていればtrue*/, black, toR, toF, fromR, fromF)){
                    if (warn) *warn += QStringLiteral("[KI2] 移動元が一意に特定できません: ") + tok + QLatin1Char('\n');
                    continue;
                }
            }

            // USI 文字列を構築
            QString usi;
            if (isDrop){
                // 例: P*7f
                const QChar toRank = rankToLetter(toR);
                usi = QString(base) + QLatin1Char('*') + QString::number(toF) + QString(toRank);
            } else {
                const QChar r0 = rankToLetter(fromR);
                const QChar r1 = rankToLetter(toR);
                usi = QString::number(fromF) + QString(r0) + QString::number(toF) + QString(r1);
                if (doPromote) usi += QLatin1Char('+');
            }

            // 盤に適用
            bd.applyMove(black, base, doPromote, isDrop, fromR, fromF, toR, toF);

            // 表示用アイテム
            KifDisplayItem d;
            d.prettyMove = tok;               // 元トークンをそのまま
            d.timeText   = QStringLiteral("00:00/00:00:00");
            // KI2 は時間表記ないケースが多い

            d.comment = pendingComment; pendingComment.clear();

            out.mainline.usiMoves.push_back(usi);
            out.mainline.disp.push_back(d);

            lastToFile = toF; lastToRank = toR;
            ++ply;
        }
    }

    return true;
}

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> ret;
    QStringList lines; QString warn;
    if (!readAllLinesDetectEncoding_(filePath, lines, &warn)) return ret;

    // ごく簡単に「開始日時」「場所」「棋戦」などを抽出（必要に応じて強化）
    for (int i=0;i<lines.size();++i){
        const QString s = lines.at(i).trimmed();
        if (s.startsWith(QStringLiteral("開始日時"))){ ret.push_back({QStringLiteral("開始日時"), s.section(QChar(u'：'),1).trimmed()}); }
        else if (s.startsWith(QStringLiteral("場所"))){ ret.push_back({QStringLiteral("場所"), s.section(QChar(u'：'),1).trimmed()}); }
        else if (s.startsWith(QStringLiteral("棋戦"))){ ret.push_back({QStringLiteral("棋戦"), s.section(QChar(u'：'),1).trimmed()}); }
    }
    return ret;
}

QMap<QString, QString> Ki2ToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    QMap<QString, QString> m;
    const QList<KifGameInfoItem> items = extractGameInfo(filePath);
    for (int i=0;i<items.size();++i){ m.insert(items.at(i).key, items.at(i).value); }
    return m;
}
