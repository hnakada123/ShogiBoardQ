#!/usr/bin/env python3
"""Generate docs/index.html and its en / zh-cn / zh-tw translations."""
import html
import json
import os
from urllib.parse import quote

# リポジトリの docs/（このファイルは scripts/docs/ にある）
DOCS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "docs")
SITE = "https://hnakada123.github.io/ShogiBoardQ/"
REPO = "https://github.com/hnakada123/ShogiBoardQ"
RELEASES = REPO + "/releases"
LATEST = RELEASES + "/latest/download/"

ORDER = ["ja", "en", "zh-cn", "zh-tw"]
META = {
    "ja": {"dir": "", "lang": "ja", "hreflang": "ja", "locale": "ja_JP", "name": "日本語", "shots": "images/"},
    "en": {"dir": "en/", "lang": "en", "hreflang": "en", "locale": "en_US", "name": "English", "shots": "images/en/"},
    "zh-cn": {"dir": "zh-cn/", "lang": "zh-Hans", "hreflang": "zh-Hans", "locale": "zh_CN", "name": "简体中文", "shots": "images/zh-cn/"},
    "zh-tw": {"dir": "zh-tw/", "lang": "zh-Hant", "hreflang": "zh-Hant", "locale": "zh_TW", "name": "繁體中文", "shots": "images/zh-tw/"},
}

# (icon, guide page) — 節へリンクするときは "page#id"。各言語の "cards" もこの文字列で引く
FEATURE_GROUPS = [
    ("play", [("game", "game-play"), ("engine", "game-play#engine-registration"), ("hayanagi", "hayanagi"), ("network", "csa-game"), ("nyugyoku", "nyugyoku")]),
    ("analysis", [("consideration", "consideration"), ("eval-graph", "kifu-analysis"), ("book", "joseki"),
                  ("position-viewer", "kyokumenshu-viewer")]),
    ("records", [("kifu-list", "kifu-display"), ("branch-tree", "branch-tree"), ("kifu-file", "kifu-management"), ("board-edit", "board-edit"),
                 ("image-export", "image-export")]),
    ("tsume", [("tsume-play", "tsume-play"), ("mate-search", "tsumi-search"), ("tsume-generator", "tsumeshogi-generator")]),
    ("setup", [("piece", "piece-style"), ("appearance", "board-colors"), ("font", "gui-font"), ("dock", "dock"),
               ("menu", "menu"), ("piece-sound", "piece-sound"), ("language", "multilanguage"), ("platform", "multi-os")]),
    ("ai", [("ai", "mcp-server")]),
]

PIECES = ["standard", "torafu_light", "wood_walnut", "tint_sakura", "deep_ebony", "deep_navy", "sengoku",
          "chess_facet_wood", "chess_atelier_paper", "chess_ribbon_slate", "alphabet_sei_wood", "alphabet_rin_slate"]

T = {}

T["ja"] = {
    "title": "ShogiBoardQ - 将棋の対局・検討・棋譜解析・詰将棋ソフト",
    "desc": "ShogiBoardQは、USIエンジンとの対局・検討・棋譜解析、6,000題の詰将棋、40種類の駒と自由な盤面デザインに対応した無料の将棋ソフトです。Windows・macOS・Linuxで動作します。",
    "tw_desc": "USIエンジンとの対局・検討・棋譜解析、詰将棋6,000題、40種類の駒。Windows・macOS・Linux対応の無料将棋ソフト。",
    "nav": ["特長", "機能", "ダウンロード", "利用ガイド"],
    "share": "共有", "menu": "メニュー", "language": "表示言語", "icon_alt": "ShogiBoardQ アイコン",
    "eyebrow": "無料・オープンソースの将棋ソフト",
    "lead": "対局・検討・棋譜解析から詰将棋まで。<br>将棋の研究に必要な機能を、ひとつの画面にまとめました。",
    "cta": ["ダウンロード", "利用ガイド"],
    "platforms": "Windows / macOS / Linux",
    "window": "将棋盤Q",
    "main_alt": "ShogiBoardQのメイン画面。検討モードでHayanagiが示した3つの候補手が盤上に番号付きの矢印で表示され、下の検討タブに読み筋が並んでいる",
    "main_cap": "検討モード：エンジンの候補手を盤上の矢印と読み筋で確認できます（Linuxで撮影）",
    "stats": [("40", "駒の種類"), ("6,000", "同梱の詰将棋（3〜13手詰）"), ("7", "読み込める棋譜形式"), ("4", "表示言語")],
    "hl_title": "主な特長",
    "hl_desc": "研究・外観・詰将棋の3つの場面から、ShogiBoardQの使い心地を紹介します。",
    "hl": [
        {
            "kicker": "研究・解析",
            "h3": "候補手は盤上の矢印で、<br>形勢は評価値グラフで",
            "p": "USIエンジンを登録すれば、任意の局面を検討モードで調べたり、棋譜全体を一括で解析したりできます。評価値グラフと解析結果の一覧から、形勢が動いた一手をすぐに見つけられます。",
            "items": [
                "候補手は番号付きの矢印で表示。持駒を打つ手は駒台から伸びます",
                "棋譜解析で各手の評価値・最善手との一致・評価値差を一覧表示",
                "対局中もエンジンの最善手を矢印で確認。エンジン同士の対局では双方の思考を並べて表示",
                "分岐ツリー・しおり・コメントで研究内容を整理",
            ],
            "links": [("consideration", "検討モード"), ("kifu-analysis", "棋譜解析"), ("game-play", "対局機能")],
            "alt": "棋譜解析の結果画面。評価値グラフで49手目から形勢が後手に傾いた様子と、各手の評価値・候補手の一覧が並んでいる",
            "cap": "棋譜解析：評価値グラフと解析結果を並べて表示",
        },
        {
            "kicker": "外観",
            "h3": "40種類の駒と、<br>自由に組み合わせる盤面",
            "p": "「表示」→「対局画面の外観…」で、駒・将棋盤・背景・駒台・対局者情報を見本から選べます。プレビューで全体を確かめながら、好みの対局画面に仕上げられます。",
            "items": [
                "駒は標準・虎斑・木肌・淡色・深色・戦国文字・チェス風・アルファベットの40種類",
                "将棋盤・背景・駒台は各26種類、対局者情報は20種類。おすすめの組み合わせも24種類",
                "選んだ外観は読み筋盤・詰将棋・画像出力にも反映",
            ],
            "links": [("piece-style", "駒の種類"), ("board-colors", "対局画面の外観")],
            "alt": "対局画面の外観ウィンドウ。駒の見本から黒檀を選び、月白の将棋盤と組み合わせたプレビューを表示している",
            "cap": "対局画面の外観：「黒檀・月白」の組み合わせ",
            "pieces_label": "駒の見本",
        },
        {
            "kicker": "詰将棋",
            "h3": "6,000題の詰将棋を、<br>盤上で解く",
            "p": "配布パッケージには3・5・7・9・11・13手詰を各1,000題収録。問題一覧から選び、内蔵エンジンHayanagiが務める玉方を相手に盤上で詰ませます。",
            "items": [
                "初期盤面のカードで問題を選び、挑戦状況で絞り込み",
                "残り手数以内に詰ませれば正解。成立する別解も正解として判定",
                "挑戦・正答の履歴と正解手順の再生で振り返り",
                "詰み探索や詰将棋局面生成（GUI・CLI）で問題づくりにも対応",
            ],
            "links": [("tsume-play", "詰将棋対局"), ("tsumeshogi-generator", "詰将棋局面生成"), ("tsumi-search", "詰み探索")],
            "alt": "詰将棋の問題一覧。5手詰の問題が初期盤面のカードで並んでいる",
            "alt2": "詰将棋対局の画面。攻方のあなたがHayanagiの玉を詰ませる",
            "cap": "問題一覧（奥）と詰将棋対局の画面（手前）",
        },
    ],
    "pieces": ["標準の駒", "淡虎斑", "胡桃", "薄桜", "黒檀", "鉄紺", "戦国文字", "Facet（木肌）", "Atelier（白）",
               "Ribbon（墨）", "端正（木肌）", "凛（墨）"],
    "more": "詳しく見る",
    "ft_title": "機能一覧",
    "ft_desc": "利用ガイドで、すべての機能の使い方を目的別に解説しています。",
    "engine_note": "対局・検討・解析には、USIプロトコルに対応した将棋エンジンを登録して使います。各OSの配布パッケージには将棋エンジンHayanagiを同梱しています。将棋エンジンの開発者の方々に感謝いたします。",
    "groups": {"play": "対局", "analysis": "研究・解析", "records": "棋譜・局面の操作", "tsume": "詰将棋",
               "setup": "導入・画面設定", "ai": "AI連携"},
    "cards": {
        "game-play": ("対局機能", "人間対エンジン・エンジン同士・人間同士で対局。平手と13種類の駒落ち、持ち時間・秒読み・フィッシャー加算（先後で別の時間も可）、最大手数、棋譜の自動保存を設定でき、エンジン同士の連続対局では1局ごとに先後を入れ替えられます。千日手・連続王手の千日手の判定、指せるマスの表示、中断した対局の再開（局面と残り時間を保持）にも対応。"),
        "game-play#engine-registration": ("エンジン登録・設定", "USIエンジンの実行ファイルを選んで登録し、ハッシュサイズ・スレッド数・先読みなどのオプションをエンジンごとに設定できます。"),
        "hayanagi": ("将棋エンジン Hayanagi", "ShogiBoardQと一緒に開発しているUSIエンジン。登録すれば対局・検討・解析に使え、詰将棋対局では玉方を務めます。"),
        "csa-game": ("CSA通信対局", "floodgateなどCSAプロトコル対応サーバーに接続し、人間またはエンジンで通信対局できます。"),
        "nyugyoku": ("入玉宣言", "持将棋の点数を計算し、24点法・27点法に基づく入玉宣言を判定します。"),
        "consideration": ("検討モード", "任意の局面でエンジンに候補手を考えさせ、複数の読み筋と矢印で比較します。読み筋は別の盤（読み筋盤）で1手ずつ再生できます。"),
        "kifu-analysis": ("棋譜解析", "棋譜全体をエンジンで解析し、各手の評価値と形勢の推移をグラフで確認します。"),
        "joseki": ("定跡機能", "定跡ファイルを読み込み、局面に合う定跡手の表示・着手・編集ができます。"),
        "kyokumenshu-viewer": ("局面集ビューア", "SFEN形式の局面集を盤面で閲覧し、選んだ局面をメイン画面に取り込めます。"),
        "kifu-display": ("棋譜表示", "指し手・消費時間・しおり・コメント・分岐を表示。棋譜と座標は英語式の表記にも切り替えられます。"),
        "branch-tree": ("分岐ツリー", "本譜と変化の手順を一覧し、右クリックで本譜の入れ替え・並べ替え・削除や折りたたみができます。盤上で指した手も変化として記録できます。"),
        "kifu-management": ("棋譜管理", "KIF・KI2・CSA・JKF・USI・SFEN・USENの読み込みと保存、クリップボード経由のコピー・貼り付けに対応。局面はBOD形式でもコピーでき、貼り付けではBODも自動判定します。対局者・手合割・持ち時間などの対局情報も編集して棋譜に保存できます。"),
        "board-edit": ("盤面編集", "駒箱を使って自由に駒を配置し、作った局面から対局や検討を始められます。"),
        "image-export": ("画像エクスポート", "盤面や評価値グラフを画像として保存・コピーできます。"),
        "tsume-play": ("詰将棋対局", "問題一覧から選んで盤上で解き、挑戦履歴と正解手順で振り返ります。"),
        "tsumi-search": ("詰み探索", "USIエンジンで詰みを探し、詰み手順を表示します。"),
        "tsumeshogi-generator": ("詰将棋局面生成", "条件に合う詰将棋をGUIやCLIで自動生成し、余詰を検査して保存します。"),
        "piece-style": ("駒の種類", "標準の駒に加え、虎斑・戦国文字・チェス風・アルファベットなど全40種類。"),
        "board-colors": ("対局画面の外観", "駒・将棋盤・背景・駒台・対局者情報を見本から自由に組み合わせます。"),
        "gui-font": ("GUI全体のフォント", "メニュー・棋譜・ログ・各ダイアログの書体をまとめて変更。見本で確かめてから選べます。"),
        "dock": ("ドック機能", "棋譜・思考・評価値グラフなどのパネルを自由に配置し、レイアウトを保存できます。"),
        "menu": ("メニュー機能", "アイコン付きのメニューパネルから各操作へすばやくアクセス。お気に入りも登録できます。"),
        "piece-sound": ("駒音", "駒を指したときの駒音のオン・オフと、音量・音の高さ・音質を調整できます。"),
        "multilanguage": ("多言語対応", "日本語・英語・中国語（簡体字／繁体字）のUIに対応。「設定」→「言語設定」で切り替えられます。"),
        "multi-os": ("クロスプラットフォーム", "Qt 6で開発し、Linux・macOS・Windowsで動作します。"),
        "mcp-server": ("AIクライアント連携（MCP）", "Claude・Cursor・VS Code・Gemini CLI・Codex CLIなどから、棋譜変換・解析・詰将棋生成・アプリ操作を依頼できます。"),
    },
    "dl_title": "ダウンロード",
    "dl_desc": "最新版はGitHubのリリースページから入手できます。無料でご利用いただけます。",
    "dl": {
        "windows": ("Windows", "Windows 64ビット版（x64）",
                    ["ZIPをダウンロードしてすべて展開", "<code>ShogiBoardQ.exe</code> を起動"], "Windows版（ZIP）"),
        "macos": ("macOS", "Apple Silicon搭載のMac",
                  ["ZIPを展開してDMGを開く", "<code>ShogiBoardQ.app</code> をアプリケーションフォルダへ", "初回起動が止められたら「システム設定」→「プライバシーとセキュリティ」で許可"],
                  "macOS版（ZIP）"),
        "linux": ("Linux", "x86_64（AppImage）",
                  ["ZIPを展開", "<code>ShogiBoardQ-linux-x86_64.AppImage</code> に実行権限を付けて起動"], "Linux版（ZIP）"),
    },
    "dl_note": "各パッケージには、詰将棋問題集（6,000題）と将棋エンジンHayanagi（定跡付き）を同梱しています。動作条件と変更点は<a href=\"{rel}\">リリースノート</a>をご覧ください。",
    "build_title": "ソースからビルド",
    "build_desc": "リポジトリをサブモジュールごと取得し、Qt 6とCMakeでビルドできます。各OSの手順を利用ガイドで解説しています。",
    "build_links": ["Linux ビルド手順", "Windows ビルド手順", "macOS ビルド手順"],
    "repo": "GitHubリポジトリ",
    "license_title": "ライセンス",
    "license": [
        "ShogiBoardQは <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\"><strong>GNU General Public License v3.0 (GPL-3.0)</strong></a> のもとで公開されているフリーソフトウェアです。",
        ["ソフトウェアの利用・改変・再配布は自由です。改変・利用にあたって作者への連絡は不要です。",
         "改変して配布する場合は、同じGPL-3.0ライセンスのもとでソースコードを公開する必要があります。",
         "本ソフトウェアは<strong>無保証</strong>で提供されます。いかなる場合においても、作者は本ソフトウェアの使用によって生じた損害について一切の責任を負いません。"],
        "ライセンス全文は <a href=\"{license}\">LICENSE</a> ファイルをご覧ください。",
    ],
    "foot_tag": "将棋の対局・検討・棋譜解析・詰将棋ソフト",
    "foot_site": "サイト", "foot_links": "リンク", "foot_releases": "リリース", "foot_issues": "不具合の報告・要望",
}

T["en"] = {
    "title": "ShogiBoardQ - Shogi Software for Playing, Analysis, and Tsume Shogi",
    "desc": "ShogiBoardQ is free shogi software for playing against USI engines, studying positions, analyzing game records, solving 6,000 bundled tsume shogi puzzles, and customizing the board with 40 piece styles. It runs on Windows, macOS, and Linux.",
    "tw_desc": "Play USI engines, analyze games, and solve 6,000 tsume puzzles. Free shogi software for Windows, macOS, and Linux.",
    "nav": ["Highlights", "Features", "Download", "User Guide"],
    "share": "Share", "menu": "Menu", "language": "Language", "icon_alt": "ShogiBoardQ icon",
    "eyebrow": "Free and open-source shogi software",
    "lead": "From games and engine analysis to tsume shogi:<br>everything you need to study shogi, in one window.",
    "cta": ["Download", "User Guide"],
    "platforms": "Windows / macOS / Linux",
    "window": "ShogiBoardQ",
    "main_alt": "ShogiBoardQ main window in Consideration mode. Hayanagi's three candidate moves are drawn on the board as numbered arrows, and their lines are listed in the Consideration tab.",
    "main_cap": "Consideration mode: compare an engine's candidate moves with on-board arrows and lines (captured on Linux with the English interface)",
    "stats": [("40", "piece styles"), ("6,000", "bundled tsume puzzles (mate in 3–13)"), ("7", "game record formats"), ("4", "interface languages")],
    "hl_title": "Highlights",
    "hl_desc": "A quick tour of studying, customizing, and solving puzzles in ShogiBoardQ.",
    "hl": [
        {
            "kicker": "Study &amp; Analysis",
            "h3": "Candidate moves as arrows,<br>the game's flow as a graph",
            "p": "Register a USI engine to explore any position in Consideration mode or analyze a whole game at once. The evaluation graph and the results table take you straight to the moves where the game turned.",
            "items": [
                "Candidate moves appear as numbered arrows; drops start from the piece stand",
                "Game Analysis lists each move's score, whether it matched the best move, and the score change",
                "See the engine's best move as an arrow during games; engine-vs-engine games show both sides' thinking",
                "Organize your study with the branch tree, bookmarks, and comments",
            ],
            "links": [("consideration", "Consideration Mode"), ("kifu-analysis", "Game Analysis"), ("game-play", "Playing Games")],
            "alt": "Game Analysis results. The evaluation graph shows the game tilting toward White from move 49, next to a table of scores and best moves.",
            "cap": "Game Analysis: the evaluation graph beside the results table",
        },
        {
            "kicker": "Appearance",
            "h3": "40 piece styles,<br>and a board you design",
            "p": "Open View → Game Appearance… to choose pieces, board, background, piece stands, and player cards from samples, and check the whole screen in the live preview.",
            "items": [
                "40 piece styles: standard, tiger grain, wood, light, deep, Sengoku Kanji, chess-inspired, and alphabet",
                "26 boards, backgrounds, and piece stands each, 20 player-card styles, and 24 suggested combinations",
                "Your choice also applies to the PV board, tsume shogi, and exported images",
            ],
            "links": [("piece-style", "Piece Styles"), ("board-colors", "Game Appearance")],
            "alt": "Game Appearance window with Ebony pieces selected and previewed on a Moon White board",
            "cap": "Game Appearance: the Ebony / Moon White combination",
            "pieces_label": "Sample piece styles",
        },
        {
            "kicker": "Tsume Shogi",
            "h3": "Solve 6,000 tsume shogi puzzles<br>on the board",
            "p": "The release packages include 1,000 puzzles each of mate in 3, 5, 7, 9, 11, and 13. Pick one from the list and checkmate the king while the built-in Hayanagi engine defends.",
            "items": [
                "Browse puzzles as board cards and filter them by progress",
                "Mate within the remaining moves to solve it; valid alternative solutions count too",
                "Review your attempt history and replay the solution",
                "Create puzzles with Mate Search and the Tsume Shogi Generator (GUI and CLI)",
            ],
            "links": [("tsume-play", "Tsume Shogi Play"), ("tsumeshogi-generator", "Tsume Shogi Generator"), ("tsumi-search", "Mate Search")],
            "alt": "Tsume shogi puzzle list showing mate-in-5 puzzles as board cards",
            "alt2": "Tsume Shogi Play window where you attack the king defended by Hayanagi",
            "cap": "The puzzle list (back) and the Tsume Shogi Play window (front)",
        },
    ],
    "pieces": ["Standard Pieces", "Light Tiger Grain", "Walnut", "Pale Cherry", "Ebony", "Iron Navy", "Sengoku Kanji",
               "Facet (Wood)", "Atelier (White)", "Ribbon (Ink)", "Sei (Wood)", "Rin (Ink)"],
    "more": "Learn more",
    "ft_title": "Features",
    "ft_desc": "The User Guide explains every feature, organized by task.",
    "engine_note": "Games and analysis use a shogi engine that supports the USI protocol. The release packages for every OS include the Hayanagi engine. We thank the developers of shogi engines for their work.",
    "groups": {"play": "Playing", "analysis": "Study &amp; Analysis", "records": "Records &amp; Positions",
               "tsume": "Tsume Shogi", "setup": "Setup &amp; Appearance", "ai": "AI Integration"},
    "cards": {
        "game-play": ("Playing Games", "Play human vs. engine, engine vs. engine, or human vs. human: an even game or 13 handicaps, time controls with byoyomi or Fischer increment (per side if needed), a move limit, and auto-save, plus side swapping in engine series. Repetition (sennichite) and perpetual check are detected, legal destinations are highlighted, and an interrupted game resumes with its position and clocks."),
        "game-play#engine-registration": ("Engine Registration &amp; Settings", "Register a USI engine by selecting its executable, then set options such as hash size, threads, and pondering for each engine."),
        "hayanagi": ("Hayanagi Shogi Engine", "A USI engine developed alongside ShogiBoardQ. Register it to play and analyze; it also defends in Tsume Shogi Play."),
        "csa-game": ("CSA Network Play", "Connect to CSA-protocol servers such as floodgate and play as a human or with an engine."),
        "nyugyoku": ("Entering King Declaration", "Count jishogi points and check declarations under the 24-point and 27-point rules."),
        "consideration": ("Consideration Mode", "Let an engine study any position and compare several lines with arrows. Replay any line move by move on a separate board (the PV board)."),
        "kifu-analysis": ("Game Analysis", "Analyze a whole game and follow each move's score on the evaluation graph."),
        "joseki": ("Opening Books", "Load opening books to show, play, and edit book moves for the current position."),
        "kyokumenshu-viewer": ("SFEN Collection Viewer", "Browse SFEN position collections on a board and import a position into the main window."),
        "kifu-display": ("Game Record Display", "View moves, times, bookmarks, comments, and variations. Records and coordinates can use Western notation."),
        "branch-tree": ("Branch Tree", "See the main line and all variations at a glance. Promote, reorder, delete, and collapse variations from the right-click menu, and record moves played on the board as variations."),
        "kifu-management": ("Game Record Management", "Load and save KIF, KI2, CSA, JKF, USI, SFEN, and USEN, and copy or paste records via the clipboard. Positions can also be copied as BOD, which pasting detects automatically. Game info such as players, handicap, and time control can be edited and saved with the record."),
        "board-edit": ("Board Editing", "Arrange pieces freely with the piece box, then start a game or analysis from that position."),
        "image-export": ("Image Export", "Save or copy the board and the evaluation graph as images."),
        "tsume-play": ("Tsume Shogi Play", "Choose a puzzle, solve it on the board, and review your history and the solution."),
        "tsumi-search": ("Mate Search", "Search for a mate with a USI engine and show the mating line."),
        "tsumeshogi-generator": ("Tsume Shogi Generator", "Generate puzzles that match your conditions in the GUI or CLI, check them for alternative mates, and save them."),
        "piece-style": ("Piece Styles", "40 styles in all, including tiger grain, Sengoku Kanji, chess-inspired, and alphabet pieces."),
        "board-colors": ("Game Appearance", "Combine pieces, board, background, piece stands, and player cards freely from samples."),
        "gui-font": ("Application Font", "Change the font of menus, game records, logs, and dialogs at once, with a live preview."),
        "dock": ("Dock Panels", "Arrange the record, thinking, graph, and other panels freely and save your layout."),
        "menu": ("Menu Panel", "Reach commands quickly from an icon menu panel, and register your favorites."),
        "piece-sound": ("Piece Sound", "Turn the move sound on or off and adjust its volume, pitch, and tone."),
        "multilanguage": ("Language Support", "Japanese, English, and Chinese (Simplified or Traditional) interfaces, switchable from Settings → Language."),
        "multi-os": ("Cross-Platform Support", "Built with Qt 6 for Linux, macOS, and Windows."),
        "mcp-server": ("AI Client Integration (MCP)", "Ask Claude, Cursor, VS Code, Gemini CLI, Codex CLI, and other AI clients to convert records, analyze, generate puzzles, and operate the app."),
    },
    "dl_title": "Download",
    "dl_desc": "Get the latest release from GitHub. ShogiBoardQ is free to use.",
    "dl": {
        "windows": ("Windows", "64-bit Windows (x64)",
                    ["Download the ZIP and extract all files", "Run <code>ShogiBoardQ.exe</code>"], "Windows (ZIP)"),
        "macos": ("macOS", "Mac with Apple silicon",
                  ["Extract the ZIP and open the DMG", "Drag <code>ShogiBoardQ.app</code> to Applications", "If the first launch is blocked, allow it in System Settings → Privacy &amp; Security"],
                  "macOS (ZIP)"),
        "linux": ("Linux", "x86_64 (AppImage)",
                  ["Extract the ZIP", "Make <code>ShogiBoardQ-linux-x86_64.AppImage</code> executable and run it"], "Linux (ZIP)"),
    },
    "dl_note": "Every package includes the tsume shogi collections (6,000 puzzles) and the Hayanagi engine with its opening book. See the <a href=\"{rel}\">release notes</a> (in Japanese) for system requirements and changes.",
    "build_title": "Build from Source",
    "build_desc": "Clone the repository with its submodules and build it with Qt 6 and CMake. The User Guide walks through each OS.",
    "build_links": ["Linux Build Guide", "Windows Build Guide", "macOS Build Guide"],
    "repo": "GitHub Repository",
    "license_title": "License",
    "license": [
        "ShogiBoardQ is free software released under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\"><strong>GNU General Public License v3.0 (GPL-3.0)</strong></a>.",
        ["You may use, modify, and redistribute this software freely. You do not need to contact the author to use or modify it.",
         "If you distribute a modified version, you must make its source code available under the same GPL-3.0 license.",
         "This software is provided with <strong>NO WARRANTY</strong>. The author accepts no liability for any damages arising from the use of this software."],
        "For the full license text, see the <a href=\"{license}\">LICENSE</a> file.",
    ],
    "foot_tag": "Shogi software for playing, analysis, and tsume shogi",
    "foot_site": "Site", "foot_links": "Links", "foot_releases": "Releases", "foot_issues": "Bug reports &amp; requests",
}

T["zh-cn"] = {
    "title": "ShogiBoardQ - 将棋对局、分析与诘棋软件",
    "desc": "ShogiBoardQ 是一款免费的将棋（日本象棋）软件，支持与 USI 引擎对局、局面研究和棋谱分析，附带 6,000 道诘棋题目，并提供 40 种棋子和可自由搭配的棋盘外观。可在 Windows、macOS 和 Linux 上运行。",
    "tw_desc": "与 USI 引擎对局、分析棋谱、挑战 6,000 道诘棋。适用于 Windows、macOS、Linux 的免费将棋软件。",
    "nav": ["特色", "功能", "下载", "使用指南"],
    "share": "分享", "menu": "菜单", "language": "语言", "icon_alt": "ShogiBoardQ 图标",
    "eyebrow": "免费开源的将棋软件",
    "lead": "从对局、研究、棋谱分析到诘棋，<br>研究将棋所需的功能都集中在一个窗口中。",
    "cta": ["下载", "使用指南"],
    "platforms": "Windows / macOS / Linux",
    "window": "ShogiBoardQ",
    "main_alt": "ShogiBoardQ 主窗口的研究模式。Hayanagi 给出的 3 个候选着法以带编号的箭头显示在棋盘上，下方“研究”标签页列出了各自的主要变化。",
    "main_cap": "研究模式：通过棋盘上的箭头和主要变化比较引擎的候选着法（在 Linux 上以简体中文界面截图）",
    "stats": [("40", "种棋子样式"), ("6,000", "道附带的诘棋（3～13 手诘）"), ("7", "种可读取的棋谱格式"), ("4", "种界面语言")],
    "hl_title": "主要特色",
    "hl_desc": "从研究、外观和诘棋三个场景，介绍 ShogiBoardQ 的使用体验。",
    "hl": [
        {
            "kicker": "研究与分析",
            "h3": "候选着法化作箭头，<br>形势变化一目了然",
            "p": "注册 USI 引擎后，即可在研究模式中分析任意局面，或一次性分析整盘棋谱。借助评价值图表和分析结果列表，可以迅速找到形势发生转折的那一手。",
            "items": [
                "候选着法以带编号的箭头显示，打入的着法从驹台引出",
                "棋谱分析逐手列出评价值、是否与最佳着法一致以及评价值变化",
                "对局中也能用箭头查看引擎的最佳着法；引擎对引擎时可同时显示双方的思考",
                "通过变化树、书签和注释整理研究内容",
            ],
            "links": [("consideration", "研究模式"), ("kifu-analysis", "棋谱分析"), ("game-play", "对局")],
            "alt": "棋谱分析结果。评价值图表显示从第 49 手起形势倒向后手，旁边是各手的评价值与候选着法列表。",
            "cap": "棋谱分析：评价值图表与分析结果并排显示",
        },
        {
            "kicker": "外观",
            "h3": "40 种棋子，<br>自由搭配棋盘外观",
            "p": "通过“视图”→“对局外观…”，可以从样本中分别选择棋子、棋盘、背景、驹台和对局者信息，并在预览中确认整体效果。",
            "items": [
                "棋子共 40 种：标准、虎纹、木纹、浅色、深色、战国汉字、国际象棋风格和字母",
                "棋盘、背景、驹台各 26 种，对局者信息 20 种，另有 24 种推荐组合",
                "所选外观同样应用于变化棋盘、诘棋和导出的图片",
            ],
            "links": [("piece-style", "棋子样式"), ("board-colors", "对局外观")],
            "alt": "对局外观窗口。在棋子样本中选择了乌木，并与月白棋盘组合进行预览。",
            "cap": "对局外观：“乌木／月白”组合",
            "pieces_label": "棋子样式示例",
        },
        {
            "kicker": "诘棋",
            "h3": "在棋盘上挑战<br>6,000 道诘棋",
            "p": "发布包收录 3、5、7、9、11、13 手诘各 1,000 道。从题目列表中选题，由内置引擎 Hayanagi 担任守方，在棋盘上将死对方的玉。",
            "items": [
                "以棋盘卡片浏览题目，并按挑战进度筛选",
                "在剩余手数内将死即为正解，成立的其他解法同样判为正确",
                "通过挑战记录和正解手顺回放进行复盘",
                "还可以用诘棋搜索和诘棋局面生成器（GUI 与 CLI）自己出题",
            ],
            "links": [("tsume-play", "诘棋练习"), ("tsumeshogi-generator", "诘棋局面生成器"), ("tsumi-search", "诘棋搜索")],
            "alt": "诘棋题集列表，以棋盘卡片显示 5 手诘题目",
            "alt2": "诘棋练习窗口，由你作为攻方将死 Hayanagi 防守的玉",
            "cap": "题目列表（后）与诘棋练习窗口（前）",
        },
    ],
    "pieces": ["标准棋子", "浅虎纹", "胡桃木", "淡樱", "乌木", "铁蓝", "战国汉字", "Facet（木色）", "Atelier（白色）",
               "Ribbon（墨色）", "端正（木色）", "凛（墨色）"],
    "more": "了解更多",
    "ft_title": "功能一览",
    "ft_desc": "使用指南按用途介绍了全部功能的用法。",
    "engine_note": "对局、研究和分析需要注册支持 USI 协议的将棋引擎。各操作系统的发布包中都附带了将棋引擎 Hayanagi。在此向各位将棋引擎开发者致以谢意。",
    "groups": {"play": "对局", "analysis": "研究与分析", "records": "棋谱与局面", "tsume": "诘棋",
               "setup": "界面与设置", "ai": "AI 集成"},
    "cards": {
        "game-play": ("对局", "支持人与引擎、引擎与引擎、人与人对局。可设置平手或 13 种让子、用时、读秒或每手加时（先后手可分别设置）、最大手数和棋谱自动保存，引擎连续对局时可每局交换先后手。会判定千日手和连续王手千日手，选中棋子时提示可走的位置，中断的对局可保留局面和剩余时间继续进行。"),
        "game-play#engine-registration": ("引擎注册与设置", "选择 USI 引擎的可执行文件进行注册，并可为每个引擎分别设置哈希大小、线程数、后台思考等选项。"),
        "hayanagi": ("将棋引擎 Hayanagi", "与 ShogiBoardQ 一同开发的 USI 引擎。注册后即可用于对局、研究和分析，在诘棋练习中担任守方。"),
        "csa-game": ("CSA 网络对局", "连接 floodgate 等支持 CSA 协议的服务器，由人或引擎进行网络对局。"),
        "nyugyoku": ("入玉宣言", "计算持将棋点数，按 24 点法和 27 点法判定入玉宣言。"),
        "consideration": ("研究模式", "让引擎思考任意局面的候选着法，通过多条主要变化和箭头进行比较。主要变化可在另一个棋盘（变化棋盘）上逐手回放。"),
        "kifu-analysis": ("棋谱分析", "用引擎分析整盘棋谱，通过图表查看每一手的评价值和形势变化。"),
        "joseki": ("定跡", "读取定跡（开局定式）文件，显示、走出并编辑与当前局面对应的定跡着法。"),
        "kyokumenshu-viewer": ("SFEN 局面集查看器", "在棋盘上浏览 SFEN 格式的局面集，并将选中的局面导入主窗口。"),
        "kifu-display": ("棋谱显示", "显示着法、用时、书签、注释和变化。棋谱和坐标也可切换为西式记法。"),
        "branch-tree": ("变化树", "一览主线与所有变化，可通过右键菜单设为主线、调整顺序、删除和折叠变化，在棋盘上走的着法也能记录为变化。"),
        "kifu-management": ("棋谱管理", "支持读取和保存 KIF、KI2、CSA、JKF、USI、SFEN、USEN，并可通过剪贴板复制和粘贴。局面还可复制为 BOD 格式，粘贴时同样会自动识别。对局者、让子、用时规则等对局信息也可编辑并随棋谱保存。"),
        "board-edit": ("局面编辑", "使用驹箱自由摆放棋子，并从编辑好的局面开始对局或研究。"),
        "image-export": ("图片导出", "将棋盘和评价值图表保存或复制为图片。"),
        "tsume-play": ("诘棋练习", "从题目列表中选题，在棋盘上解题，并通过挑战记录和正解手顺复盘。"),
        "tsumi-search": ("诘棋搜索", "使用 USI 引擎搜索诘杀，并显示诘杀手顺。"),
        "tsumeshogi-generator": ("诘棋局面生成器", "在 GUI 或 CLI 中自动生成符合条件的诘棋，检查余诘后保存。"),
        "piece-style": ("棋子样式", "除标准棋子外，还有虎纹、战国汉字、国际象棋风格、字母等，共 40 种。"),
        "board-colors": ("对局外观", "从样本中自由组合棋子、棋盘、背景、驹台和对局者信息。"),
        "gui-font": ("应用程序字体", "一次性更改菜单、棋谱、日志和各对话框的字体，可先在预览中确认。"),
        "dock": ("停靠面板", "自由排列棋谱、思考、评价值图表等面板，并保存布局。"),
        "menu": ("菜单面板", "通过带图标的菜单面板快速执行各项操作，还可以把常用功能加入收藏。"),
        "piece-sound": ("走子音效", "开启或关闭走子音效，并调整音量、音高和音色。"),
        "multilanguage": ("多语言", "支持日语、英语、中文（简体／繁体）界面，可在“设置”→“语言”中切换。"),
        "multi-os": ("跨平台", "基于 Qt 6 开发，可在 Linux、macOS 和 Windows 上运行。"),
        "mcp-server": ("AI 客户端集成（MCP）", "可以从 Claude、Cursor、VS Code、Gemini CLI、Codex CLI 等 AI 客户端请求棋谱转换、分析、诘棋生成和应用操作。"),
    },
    "dl_title": "下载",
    "dl_desc": "可从 GitHub 发布页面获取最新版本，免费使用。",
    "dl": {
        "windows": ("Windows", "64 位 Windows（x64）",
                    ["下载 ZIP 并全部解压", "运行 <code>ShogiBoardQ.exe</code>"], "Windows 版（ZIP）"),
        "macos": ("macOS", "搭载 Apple 芯片的 Mac",
                  ["解压 ZIP 并打开 DMG", "将 <code>ShogiBoardQ.app</code> 拖到“应用程序”文件夹", "首次启动被阻止时，请在“系统设置”→“隐私与安全性”中允许"],
                  "macOS 版（ZIP）"),
        "linux": ("Linux", "x86_64（AppImage）",
                  ["解压 ZIP", "为 <code>ShogiBoardQ-linux-x86_64.AppImage</code> 添加执行权限后运行"], "Linux 版（ZIP）"),
    },
    "dl_note": "各安装包均附带诘棋题集（6,000 道）和将棋引擎 Hayanagi（含定式文件）。系统要求和更新内容请参阅<a href=\"{rel}\">发布说明</a>（日文）。",
    "build_title": "从源代码构建",
    "build_desc": "获取包含子模块的仓库后，即可使用 Qt 6 和 CMake 构建。各操作系统的步骤请参阅构建指南（macOS 为英文）。",
    "build_links": ["Linux 构建指南", "Windows 构建指南", "macOS 构建指南"],
    "repo": "GitHub 仓库",
    "license_title": "许可证",
    "license": [
        "ShogiBoardQ 是依据 <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\"><strong>GNU 通用公共许可证第 3 版（GPL-3.0）</strong></a>发布的自由软件。",
        ["可以自由使用、修改和再分发本软件。使用或修改时无需联系作者。",
         "修改后再分发时，必须以相同的 GPL-3.0 许可证公开源代码。",
         "本软件<strong>不提供任何担保</strong>。在任何情况下，作者均不对因使用本软件而产生的任何损害承担责任。"],
        "许可证全文请参阅 <a href=\"{license}\">LICENSE</a> 文件。",
    ],
    "foot_tag": "将棋对局、研究、棋谱分析与诘棋软件",
    "foot_site": "网站", "foot_links": "链接", "foot_releases": "发布页面", "foot_issues": "问题反馈与建议",
}

T["zh-tw"] = {
    "title": "ShogiBoardQ - 將棋對局、分析與詰棋軟體",
    "desc": "ShogiBoardQ 是一款免費的將棋（日本象棋）軟體，支援與 USI 引擎對局、局面研究與棋譜分析，附有 6,000 道詰棋題目，並提供 40 種棋子與可自由搭配的棋盤外觀。可在 Windows、macOS 與 Linux 上執行。",
    "tw_desc": "與 USI 引擎對局、分析棋譜、挑戰 6,000 道詰棋。適用於 Windows、macOS、Linux 的免費將棋軟體。",
    "nav": ["特色", "功能", "下載", "使用指南"],
    "share": "分享", "menu": "選單", "language": "語言", "icon_alt": "ShogiBoardQ 圖示",
    "eyebrow": "免費開源的將棋軟體",
    "lead": "從對局、研究、棋譜分析到詰棋，<br>研究將棋所需的功能都整合在同一個視窗中。",
    "cta": ["下載", "使用指南"],
    "platforms": "Windows / macOS / Linux",
    "window": "ShogiBoardQ",
    "main_alt": "ShogiBoardQ 主視窗的研究模式。Hayanagi 提出的 3 個候選著法以附編號的箭頭顯示在棋盤上，下方「研究」分頁列出各自的主要變化。",
    "main_cap": "研究模式：透過棋盤上的箭頭與主要變化比較引擎的候選著法（於 Linux 以繁體中文介面擷取）",
    "stats": [("40", "種棋子樣式"), ("6,000", "道隨附詰棋（3～13 手詰）"), ("7", "種可讀取的棋譜格式"), ("4", "種介面語言")],
    "hl_title": "主要特色",
    "hl_desc": "從研究、外觀與詰棋三個情境，介紹 ShogiBoardQ 的使用體驗。",
    "hl": [
        {
            "kicker": "研究與分析",
            "h3": "候選著法化為箭頭，<br>形勢變化一目了然",
            "p": "註冊 USI 引擎後，即可在研究模式中分析任意局面，或一次分析整盤棋譜。透過評價值圖表與分析結果列表，能迅速找出形勢轉變的那一手。",
            "items": [
                "候選著法以附編號的箭頭顯示，打入的著法從駒台延伸而出",
                "棋譜分析逐手列出評價值、是否與最佳著法一致以及評價值變化",
                "對局中也能以箭頭查看引擎的最佳著法；引擎對引擎時可同時顯示雙方的思考",
                "透過變化樹、書籤與註解整理研究內容",
            ],
            "links": [("consideration", "研究模式"), ("kifu-analysis", "棋譜分析"), ("game-play", "對局")],
            "alt": "棋譜分析結果。評價值圖表顯示自第 49 手起形勢倒向後手，旁邊是各手的評價值與候選著法列表。",
            "cap": "棋譜分析：評價值圖表與分析結果並排顯示",
        },
        {
            "kicker": "外觀",
            "h3": "40 種棋子，<br>自由搭配棋盤外觀",
            "p": "透過「檢視」→「對局外觀…」，可從樣本中分別選擇棋子、棋盤、背景、駒台與對局者資訊，並在預覽中確認整體效果。",
            "items": [
                "棋子共 40 種：標準、虎紋、木紋、淺色、深色、戰國漢字、西洋棋風格與字母",
                "棋盤、背景、駒台各 26 種，對局者資訊 20 種，另有 24 種推薦組合",
                "所選外觀也會套用到變化棋盤、詰棋與匯出的圖片",
            ],
            "links": [("piece-style", "棋子樣式"), ("board-colors", "對局外觀")],
            "alt": "對局外觀視窗。在棋子樣本中選擇烏木，並與月白棋盤組合進行預覽。",
            "cap": "對局外觀：「烏木／月白」組合",
            "pieces_label": "棋子樣式範例",
        },
        {
            "kicker": "詰棋",
            "h3": "在棋盤上挑戰<br>6,000 道詰棋",
            "p": "發行套件收錄 3、5、7、9、11、13 手詰各 1,000 道。從題目列表選題，由內建引擎 Hayanagi 擔任守方，在棋盤上將死對方的玉。",
            "items": [
                "以棋盤卡片瀏覽題目，並依挑戰進度篩選",
                "在剩餘手數內將死即為正解，成立的其他解法同樣判為正確",
                "透過挑戰紀錄與正解手順重播進行復盤",
                "也能以詰棋搜尋與詰棋局面生成器（GUI 與 CLI）自行出題",
            ],
            "links": [("tsume-play", "詰棋練習"), ("tsumeshogi-generator", "詰棋局面生成器"), ("tsumi-search", "詰棋搜尋")],
            "alt": "詰棋題集列表，以棋盤卡片顯示 5 手詰題目",
            "alt2": "詰棋練習視窗，由你擔任攻方將死 Hayanagi 防守的玉",
            "cap": "題目列表（後）與詰棋練習視窗（前）",
        },
    ],
    "pieces": ["標準棋子", "淺虎紋", "胡桃木", "淡櫻", "烏木", "鐵藍", "戰國漢字", "Facet（木色）", "Atelier（白色）",
               "Ribbon（墨色）", "端正（木色）", "凜（墨色）"],
    "more": "了解更多",
    "ft_title": "功能一覽",
    "ft_desc": "使用指南依用途介紹所有功能的用法。",
    "engine_note": "對局、研究與分析需要註冊支援 USI 協定的將棋引擎。各作業系統的發行套件中都附有將棋引擎 Hayanagi。在此向各位將棋引擎開發者致上謝意。",
    "groups": {"play": "對局", "analysis": "研究與分析", "records": "棋譜與局面", "tsume": "詰棋",
               "setup": "介面與設定", "ai": "AI 整合"},
    "cards": {
        "game-play": ("對局", "支援人對引擎、引擎對引擎、人對人對局。可設定平手或 13 種讓子、用時、讀秒或每手加時（先後手可分別設定）、最大手數與棋譜自動儲存，引擎連續對局時可每局交換先後手。會判定千日手與連續王手千日手，選取棋子時會標示可移動的位置，中斷的對局可保留局面與剩餘時間繼續進行。"),
        "game-play#engine-registration": ("引擎註冊與設定", "選擇 USI 引擎的執行檔進行註冊，並可為每個引擎分別設定雜湊大小、執行緒數、背景思考等選項。"),
        "hayanagi": ("將棋引擎 Hayanagi", "與 ShogiBoardQ 一同開發的 USI 引擎。註冊後即可用於對局、研究與分析，並在詰棋練習中擔任守方。"),
        "csa-game": ("CSA 網路對局", "連線至 floodgate 等支援 CSA 協定的伺服器，由人或引擎進行網路對局。"),
        "nyugyoku": ("入玉宣言", "計算持將棋點數，依 24 點法與 27 點法判定入玉宣言。"),
        "consideration": ("研究模式", "讓引擎思考任意局面的候選著法，透過多條主要變化與箭頭進行比較。主要變化可在另一個棋盤（變化棋盤）上逐手重播。"),
        "kifu-analysis": ("棋譜分析", "以引擎分析整盤棋譜，透過圖表查看每一手的評價值與形勢變化。"),
        "joseki": ("定跡", "讀取定跡（開局定式）檔案，顯示、走出並編輯符合目前局面的定跡著法。"),
        "kyokumenshu-viewer": ("SFEN 局面集檢視器", "在棋盤上瀏覽 SFEN 格式的局面集，並將選取的局面匯入主視窗。"),
        "kifu-display": ("棋譜顯示", "顯示著法、用時、書籤、註解與變化。棋譜與座標也可切換為西式記法。"),
        "branch-tree": ("變化樹", "一覽主線與所有變化，可透過右鍵選單設為主線、調整順序、刪除和摺疊變化，在棋盤上走的著法也能記錄為變化。"),
        "kifu-management": ("棋譜管理", "支援讀取與儲存 KIF、KI2、CSA、JKF、USI、SFEN、USEN，並可透過剪貼簿複製與貼上。局面還能複製為 BOD 格式，貼上時同樣會自動識別。對局者、讓子、用時規則等對局資訊也可編輯並隨棋譜儲存。"),
        "board-edit": ("局面編輯", "使用駒箱自由擺放棋子，並從編輯好的局面開始對局或研究。"),
        "image-export": ("圖片匯出", "將棋盤與評價值圖表儲存或複製為圖片。"),
        "tsume-play": ("詰棋練習", "從題目列表選題，在棋盤上解題，並透過挑戰紀錄與正解手順復盤。"),
        "tsumi-search": ("詰棋搜尋", "使用 USI 引擎搜尋詰殺，並顯示詰殺手順。"),
        "tsumeshogi-generator": ("詰棋局面生成器", "在 GUI 或 CLI 中自動產生符合條件的詰棋，檢查餘詰後儲存。"),
        "piece-style": ("棋子樣式", "除標準棋子外，還有虎紋、戰國漢字、西洋棋風格、字母等，共 40 種。"),
        "board-colors": ("對局外觀", "從樣本中自由組合棋子、棋盤、背景、駒台與對局者資訊。"),
        "gui-font": ("應用程式字型", "一次變更選單、棋譜、記錄和各對話方塊的字型，可先在預覽中確認。"),
        "dock": ("停駐面板", "自由排列棋譜、思考、評價值圖表等面板，並儲存版面設定。"),
        "menu": ("選單面板", "透過附圖示的選單面板快速執行各項操作，也能把常用功能加入收藏。"),
        "piece-sound": ("走子音效", "開啟或關閉走子音效，並調整音量、音高與音色。"),
        "multilanguage": ("多語言", "支援日文、英文、中文（簡體／繁體）介面，可在「設定」→「語言」中切換。"),
        "multi-os": ("跨平台", "以 Qt 6 開發，可在 Linux、macOS 與 Windows 上執行。"),
        "mcp-server": ("AI 用戶端整合（MCP）", "可從 Claude、Cursor、VS Code、Gemini CLI、Codex CLI 等 AI 用戶端要求棋譜轉換、分析、詰棋產生與應用程式操作。"),
    },
    "dl_title": "下載",
    "dl_desc": "可從 GitHub 發行頁面取得最新版本，免費使用。",
    "dl": {
        "windows": ("Windows", "64 位元 Windows（x64）",
                    ["下載 ZIP 並全部解壓縮", "執行 <code>ShogiBoardQ.exe</code>"], "Windows 版（ZIP）"),
        "macos": ("macOS", "搭載 Apple 晶片的 Mac",
                  ["解壓縮 ZIP 並開啟 DMG", "將 <code>ShogiBoardQ.app</code> 拖曳到「應用程式」資料夾", "首次啟動遭到阻擋時，請在「系統設定」→「隱私權與安全性」中允許"],
                  "macOS 版（ZIP）"),
        "linux": ("Linux", "x86_64（AppImage）",
                  ["解壓縮 ZIP", "為 <code>ShogiBoardQ-linux-x86_64.AppImage</code> 加上執行權限後執行"], "Linux 版（ZIP）"),
    },
    "dl_note": "各套件皆附有詰棋題集（6,000 道）與將棋引擎 Hayanagi（含定跡檔案）。系統需求與更新內容請參閱<a href=\"{rel}\">發行說明</a>（日文）。",
    "build_title": "從原始碼建置",
    "build_desc": "取得含子模組的儲存庫後，即可使用 Qt 6 與 CMake 建置。各作業系統的步驟請參閱建置指南（macOS 為英文）。",
    "build_links": ["Linux 建置指南", "Windows 建置指南", "macOS 建置指南"],
    "repo": "GitHub 儲存庫",
    "license_title": "授權",
    "license": [
        "ShogiBoardQ 是依據 <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\"><strong>GNU 通用公眾授權條款第 3 版（GPL-3.0）</strong></a>發布的自由軟體。",
        ["可自由使用、修改與再散布本軟體。使用或修改時無須聯絡作者。",
         "修改後再散布時，必須以相同的 GPL-3.0 授權公開原始碼。",
         "本軟體<strong>不提供任何擔保</strong>。在任何情況下，作者均不對因使用本軟體所產生的任何損害負責。"],
        "授權條款全文請參閱 <a href=\"{license}\">LICENSE</a> 檔案。",
    ],
    "foot_tag": "將棋對局、研究、棋譜分析與詰棋軟體",
    "foot_site": "網站", "foot_links": "連結", "foot_releases": "發行頁面", "foot_issues": "問題回報與建議",
}

DL_FILES = {"windows": "ShogiBoardQ-windows.zip", "macos": "ShogiBoardQ-macos.zip", "linux": "ShogiBoardQ-linux.zip"}
BUILD_PAGES = ["linux-build-and-release", "windows-build-and-release", "macos-build-and-release"]

GITHUB_SVG = ('<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/></svg>')
X_SVG = ('<svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M18.244 2.25h3.308l-7.227 8.26 8.502 11.24H16.17l-5.214-6.817L4.99 21.75H1.68l7.73-8.835L1.254 2.25H8.08l4.713 6.231zm-1.161 17.52h1.833L7.084 4.126H5.117z"/></svg>')
GLOBE_SVG = ('<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.5 2.7 3.8 5.7 3.8 9s-1.3 6.3-3.8 9c-2.5-2.7-3.8-5.7-3.8-9S9.5 5.7 12 3Z"/></svg>')

MENU_SCRIPT = """<script>
document.addEventListener('click', function (event) {
    document.querySelectorAll('.lang-menu[open]').forEach(function (menu) {
        if (!menu.contains(event.target)) menu.removeAttribute('open');
    });
});
document.addEventListener('keydown', function (event) {
    if (event.key !== 'Escape') return;
    document.querySelectorAll('.lang-menu[open]').forEach(function (menu) {
        menu.removeAttribute('open');
        menu.querySelector('summary').focus();
    });
});
</script>"""


def page(code):
    t = T[code]
    m = META[code]
    up = "../" if m["dir"] else ""
    img = up + "images/"
    shots = up + m["shots"]
    # その言語の利用ガイドの目次があればそこへ、なければ英語版へ
    guide = "guide/" if os.path.exists(os.path.join(DOCS, m["dir"], "guide", "index.html")) else "../en/guide/"
    url = SITE + m["dir"]
    css_v = "20261004"

    def gl(slug):
        # 翻訳済みのガイドがあればその言語のページへ、なければ英語版へ（# 以降はページ内の節）
        page, sep, frag = slug.partition("#")
        if os.path.exists(os.path.join(DOCS, m["dir"], "guide", page + ".html")):
            return "guide/" + page + ".html" + sep + frag
        return up + "en/guide/" + page + ".html" + sep + frag

    def lang_href(target):
        if target == code:
            return "index.html"
        return up + META[target]["dir"] + "index.html"

    e = html.escape
    out = []
    w = out.append

    alternates = "\n".join(
        f'    <link rel="alternate" hreflang="{META[c]["hreflang"]}" href="{SITE + META[c]["dir"]}">' for c in ORDER)
    share = ("https://x.com/intent/post?url=" + quote(url, safe="") + "&amp;text=" + quote(t["title"], safe=""))
    ld = {
        "@context": "https://schema.org",
        "@type": "SoftwareApplication",
        "name": "ShogiBoardQ",
        "url": url,
        "description": t["desc"],
        "applicationCategory": "GameApplication",
        "operatingSystem": "Windows, macOS, Linux",
        "inLanguage": m["hreflang"],
        "image": SITE + m["shots"] + "screenshot-main.png",
        "license": "https://www.gnu.org/licenses/gpl-3.0.html",
        "downloadUrl": RELEASES + "/latest",
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "JPY"},
    }
    # 画像を差し替えたら版を上げる（X などが古い取得結果を使い続けないよう URL を変える）
    og_image = SITE + ("images/" if code == "ja" else m["shots"]) + "og-image.png?v=20261005"

    w(f"""<!DOCTYPE html>
<html lang="{m['lang']}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(t['title'])}</title>
    <meta name="description" content="{e(t['desc'])}">
    <meta name="google-site-verification" content="6uVKShyfeRaK8nx6ROgoIuBXb2qdMtwgX8NJfiBrwVM" />
{alternates}
    <link rel="alternate" hreflang="x-default" href="{SITE}">
    <link rel="canonical" href="{url}">
    <!-- Open Graph -->
    <meta property="og:type" content="website">
    <meta property="og:url" content="{url}">
    <meta property="og:title" content="{e(t['title'])}">
    <meta property="og:description" content="{e(t['desc'])}">
    <meta property="og:image" content="{og_image}">
    <meta property="og:image:width" content="1200">
    <meta property="og:image:height" content="630">
    <meta property="og:locale" content="{m['locale']}">
""")
    for c in ORDER:
        if c != code:
            w(f'    <meta property="og:locale:alternate" content="{META[c]["locale"]}">\n')
    w(f"""    <meta property="og:site_name" content="ShogiBoardQ">
    <!-- Twitter Card -->
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(t['title'])}">
    <meta name="twitter:description" content="{e(t['tw_desc'])}">
    <meta name="twitter:image" content="{og_image}">
    <link rel="icon" type="image/png" href="{img}shogiboardq-icon.png">
    <link rel="stylesheet" href="{up}css/style.css?v={css_v}">
    <link rel="stylesheet" href="{up}css/home.css?v={css_v}">
    <script type="application/ld+json">
{json.dumps(ld, ensure_ascii=False, indent=4)}
    </script>
</head>
<body class="home">

<!-- Navigation -->
<nav class="site-nav">
    <div class="nav-inner">
        <a href="#top" class="nav-logo"><img src="{img}shogiboardq-icon.png" alt="" width="28" height="28"><span>ShogiBoardQ</span></a>
        <ul class="nav-links" id="guide-nav" onclick="if (event.target.closest('a')) {{ this.classList.remove('open'); document.querySelector('.nav-toggle').setAttribute('aria-expanded', 'false'); }}">
            <li><a href="#highlights">{t['nav'][0]}</a></li>
            <li><a href="#features">{t['nav'][1]}</a></li>
            <li><a href="#download">{t['nav'][2]}</a></li>
            <li><a href="{guide}index.html">{t['nav'][3]}</a></li>
            <li><a href="{REPO}">GitHub</a></li>
            <li>
                <a href="{share}" target="_blank" rel="noopener noreferrer" class="share-btn-x">
                    {X_SVG}
                    {t['share']}
                </a>
            </li>
        </ul>
        <details class="lang-menu">
            <summary aria-label="{t['language']}">{GLOBE_SVG}<span>{m['name']}</span></summary>
            <ul>
""")
    for c in ORDER:
        cur = ' aria-current="page"' if c == code else ""
        w(f'                <li><a href="{lang_href(c)}" lang="{META[c]["lang"]}" hreflang="{META[c]["hreflang"]}"{cur}>{META[c]["name"]}</a></li>\n')
    w(f"""            </ul>
        </details>
        <button class="nav-toggle" aria-label="{t['menu']}" aria-controls="guide-nav" aria-expanded="false" onclick="const menu = document.getElementById('guide-nav'); this.setAttribute('aria-expanded', menu.classList.toggle('open'));">
            <span></span><span></span><span></span>
        </button>
    </div>
</nav>

<main id="top">

<!-- Hero -->
<header class="home-hero">
    <div class="home-hero__inner">
        <img src="{img}shogiboardq-icon.png" alt="{t['icon_alt']}" class="home-hero__icon" width="96" height="96">
        <p class="home-hero__eyebrow">{t['eyebrow']}</p>
        <h1>ShogiBoardQ</h1>
        <p class="home-hero__lead">{t['lead']}</p>
        <div class="home-hero__actions">
            <a href="#download" class="btn btn--wood"><img src="{img}icons/download.svg" alt="">{t['cta'][0]}</a>
            <a href="{guide}index.html" class="btn btn--ghost">{t['cta'][1]}</a>
        </div>
        <p class="home-hero__meta"><span>{t['platforms']}</span><span><span lang="ja">日本語</span> / <span lang="en">English</span> / <span lang="zh-Hans">简体中文</span> / <span lang="zh-Hant">繁體中文</span></span></p>
    </div>
    <figure class="home-hero__shot">
        <div class="window-frame">
            <div class="window-frame__bar" aria-hidden="true">{t['window']}</div>
            <a href="{shots}screenshot-main.png"><img src="{shots}screenshot-main.png" alt="{e(t['main_alt'])}" width="1400" height="1000"></a>
        </div>
        <figcaption>{t['main_cap']}</figcaption>
    </figure>
</header>

<!-- Stats -->
<div class="stats">
    <ul class="stats__grid">
""")
    for value, label in t["stats"]:
        w(f'        <li class="stats__item"><span class="stats__value">{value}</span><span class="stats__label">{label}</span></li>\n')
    w(f"""    </ul>
</div>

<!-- Highlights -->
<section id="highlights" class="section">
    <div class="section-inner">
        <h2 class="section-title">{t['hl_title']}</h2>
        <p class="section-desc">{t['hl_desc']}</p>
""")
    kicker_icons = ["eval-graph", "appearance", "tsume-play"]
    for i, h in enumerate(t["hl"]):
        rev = " highlight--reverse" if i % 2 == 1 else ""
        w(f"""
        <article class="highlight{rev}">
            <div class="highlight__text">
                <p class="highlight__kicker"><img src="{img}icons/{kicker_icons[i]}.svg" alt="">{h['kicker']}</p>
                <h3>{h['h3']}</h3>
                <p>{h['p']}</p>
""")
        if i == 1:
            w(f'                <ul class="piece-row" aria-label="{h["pieces_label"]}">\n')
            for f, name in zip(PIECES, t["pieces"]):
                w(f'                    <li><img src="{img}pieces/showcase/{f}.png" alt="{e(name)}" title="{e(name)}" width="40" height="40" loading="lazy"></li>\n')
            w('                </ul>\n')
        w('                <ul class="check-list">\n')
        for item in h["items"]:
            w(f"                    <li>{item}</li>\n")
        w('                </ul>\n                <p class="link-row">\n')
        for slug, label in h["links"]:
            w(f'                    <a href="{gl(slug)}">{label} &rarr;</a>\n')
        w('                </p>\n            </div>\n')
        if i == 0:
            w(f"""            <figure class="shot">
                <a href="{shots}home/analysis.png"><img src="{shots}home/analysis.png" alt="{e(h['alt'])}" width="1400" height="1000" loading="lazy"></a>
                <figcaption>{h['cap']}</figcaption>
            </figure>
""")
        elif i == 1:
            w(f"""            <figure class="shot">
                <a href="{shots}home/appearance.png"><img src="{shots}home/appearance.png" alt="{e(h['alt'])}" width="1180" height="800" loading="lazy"></a>
                <figcaption>{h['cap']}</figcaption>
            </figure>
""")
        else:
            play_h = 856 if code == "en" else 864
            w(f"""            <figure class="shot">
                <div class="shot-stack">
                    <a href="{shots}home/tsume-list.png" class="shot-stack__back"><img src="{shots}home/tsume-list.png" alt="{e(h['alt'])}" width="1100" height="800" loading="lazy"></a>
                    <a href="{shots}home/tsume-play.png" class="shot-stack__front"><img src="{shots}home/tsume-play.png" alt="{e(h['alt2'])}" width="760" height="{play_h}" loading="lazy"></a>
                </div>
                <figcaption>{h['cap']}</figcaption>
            </figure>
""")
        w("        </article>\n")
    w(f"""    </div>
</section>

<!-- Features -->
<section id="features" class="section section--alt">
    <div class="section-inner">
        <h2 class="section-title">{t['ft_title']}</h2>
        <p class="section-desc">{t['ft_desc']}</p>
        <p class="feature-intro"><img src="{img}icons/engine.svg" alt=""><span>{t['engine_note']}</span></p>
""")
    for gkey, items in FEATURE_GROUPS:
        w(f"""
        <div class="feature-group">
            <h3 class="feature-group__title">{t['groups'][gkey]}</h3>
            <ul class="mini-grid">
""")
        for icon, slug in items:
            title, desc = t["cards"][slug]
            w(f"""                <li><a href="{gl(slug)}" class="mini-card">
                    <span class="mini-card__icon"><img src="{img}icons/{icon}.svg" alt=""></span>
                    <div class="mini-card__text"><h4>{title}</h4><p>{desc}</p></div>
                </a></li>
""")
        w("            </ul>\n        </div>\n")
    w(f"""    </div>
</section>

<!-- Download -->
<section id="download" class="section">
    <div class="section-inner">
        <h2 class="section-title">{t['dl_title']}</h2>
        <p class="section-desc">{t['dl_desc']}</p>
        <ul class="download-grid">
""")
    for os_key in ("windows", "macos", "linux"):
        name, arch, steps, button = t["dl"][os_key]
        w(f"""            <li class="dl-card">
                <h3>{name}</h3>
                <p class="dl-card__arch">{arch}</p>
                <ol>
""")
        for s in steps:
            w(f"                    <li>{s}</li>\n")
        w(f"""                </ol>
                <a href="{LATEST}{DL_FILES[os_key]}" class="btn btn--accent"><img src="{img}icons/download.svg" alt="">{button}</a>
            </li>
""")
    w(f"""        </ul>
        <p class="dl-note">{t['dl_note'].format(rel=RELEASES + '/latest')}</p>

        <div class="build-box" id="build">
            <h3><img src="{img}icons/code.svg" alt="">{t['build_title']}</h3>
            <p>{t['build_desc']}</p>
            <div class="build-box__links">
""")
    for label, slug in zip(t["build_links"], BUILD_PAGES):
        w(f'                <a href="{gl(slug)}" class="btn btn--outline">{label}</a>\n')
    w(f"""                <a href="{REPO}" class="btn btn--outline">{GITHUB_SVG}{t['repo']}</a>
            </div>
        </div>
    </div>
</section>

<!-- License -->
<section id="license" class="section section--alt">
    <div class="section-inner">
        <h2 class="section-title">{t['license_title']}</h2>
        <div class="license-content">
            <p>{t['license'][0]}</p>
            <ul class="license-points">
""")
    for li in t["license"][1]:
        w(f"                <li>{li}</li>\n")
    w(f"""            </ul>
            <p class="license-link">{t['license'][2].format(license=REPO + '/blob/main/LICENSE')}</p>
        </div>
    </div>
</section>

</main>

<!-- Footer -->
<footer class="home-footer">
    <div class="home-footer__inner">
        <div>
            <p class="home-footer__brand"><img src="{img}shogiboardq-icon.png" alt="" width="32" height="32">ShogiBoardQ</p>
            <p>{t['foot_tag']}</p>
        </div>
        <nav aria-label="{t['foot_site']}">
            <h2>{t['foot_site']}</h2>
            <ul>
                <li><a href="#highlights">{t['nav'][0]}</a></li>
                <li><a href="#features">{t['nav'][1]}</a></li>
                <li><a href="#download">{t['nav'][2]}</a></li>
                <li><a href="{guide}index.html">{t['nav'][3]}</a></li>
            </ul>
        </nav>
        <nav aria-label="{t['foot_links']}">
            <h2>{t['foot_links']}</h2>
            <ul>
                <li><a href="{REPO}">GitHub</a></li>
                <li><a href="{RELEASES}">{t['foot_releases']}</a></li>
                <li><a href="{REPO}/issues">{t['foot_issues']}</a></li>
            </ul>
        </nav>
        <nav aria-label="{t['language']}">
            <h2>{t['language']}</h2>
            <ul>
""")
    for c in ORDER:
        cur = ' aria-current="page"' if c == code else ""
        w(f'                <li><a href="{lang_href(c)}" lang="{META[c]["lang"]}" hreflang="{META[c]["hreflang"]}"{cur}>{META[c]["name"]}</a></li>\n')
    w(f"""            </ul>
        </nav>
    </div>
    <p class="home-footer__copy">&copy; 2025 ShogiBoardQ</p>
</footer>

{MENU_SCRIPT}

</body>
</html>
""")
    return "".join(out)


if __name__ == "__main__":
    for code in ORDER:
        path = os.path.join(DOCS, META[code]["dir"], "index.html")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(page(code))
        print("wrote", path)
