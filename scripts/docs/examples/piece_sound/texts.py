"""Texts of docs/guide/piece-sound.html for each language ([[label|href]] becomes a link)."""
T = {}
T["ja"] = dict(
    title="駒音 - ShogiBoardQ",
    desc="ShogiBoardQの駒音の使い方を紹介します。「設定」メニューでのオン・オフ、駒音の設定ダイアログでの音量・音の高さ・イコライザーの調整、駒音が鳴る場面を解説します。",
    og_desc="駒を指した瞬間に「パチッ」と鳴る駒音。オン・オフ、音量・音の高さ・イコライザーを調整できます。",
    menu="メニュー", lang="表示言語",
    nav=["ホーム", "利用ガイド", "オン・オフ", "音量と音質", "鳴る場面"],
    h1="駒音", subtitle="駒を指した瞬間に、盤に駒を打つ「パチッ」という音が鳴ります",
    badges=["オン・オフ切り替え", "音量調整", "イコライザー"],
    note="",
    s_toggle=("駒音のオン・オフ", "「設定」メニューから、いつでも駒音を切り替えられます"),
    toggle=("「設定」→「駒音」で切り替える", [
        "メニューバーの「設定」を開くと「駒音」の項目があり、チェックが付いていれば駒音が鳴ります。初期状態ではオンです。選ぶたびにオンとオフが切り替わります。"]),
    saved=("設定は自動で保存", [
        "オン・オフの状態は自動で保存され、次回の起動時にも使われます。[[メニュードック|menu.html]]の「設定」タブにも同じ「駒音」「駒音の設定…」のボタンがあります。"]),
    s_settings=("音量と音質の調整", "「駒音の設定」ダイアログで、音量・音の高さ・イコライザーを調整できます"),
    open=("「設定」→「駒音の設定…」を開く", [
        "ダイアログには音量と音の高さの横スライダー、イコライザーの低音・中音・高音の縦スライダーが並びます。スライダーを動かすと、離したときに今の設定の駒音が鳴るので、音を聞きながら調整できます。「試聴」を押せば、いつでも今の設定の音を確かめられます（駒音をオフにしていても鳴ります）。"]),
    sliders=("スライダーで好みの音にする", "各スライダーの右や下の欄に数値を直接入力することもできます。", [
        ("音量", "0〜100%。初期値は30%です。耳の感覚に合わせた目盛りになっています。"),
        ("音の高さ", "-12〜+12半音。上げると高く短い音、下げると低く長い音になります。"),
        ("イコライザー", "低音・中音・高音をそれぞれ-12〜+12dBで調整します。中音を上げると打つ感じが強まり、高音を下げると柔らかい音になります。"),
    ]),
    finish=("確定・取り消し・標準に戻す", [
        "「OK」で設定を確定します。「キャンセル」を押すと、ダイアログを開いたときの設定に戻ります。「標準に戻す」を押すと、音量30%・音の高さ0・イコライザー0の初期設定に戻せます。確定した設定は自動で保存され、次回の起動時にも使われます。",
        "左下の「A-」「A+」でダイアログの文字の大きさを変えられます。文字の大きさとダイアログの大きさも次回に引き継がれます。"]),
    s_when=("駒音が鳴る場面", "対局で盤上の駒が動いたときに鳴り、棋譜の閲覧や局面編集では鳴りません"),
    table_head=("場面", "駒音", "補足"),
    rows=[
        ("人間の着手", True, "人間対人間、人間対エンジンで自分が指したとき"),
        ("エンジンの着手", True, "人間対エンジン、エンジン対エンジンでエンジンが指したとき"),
        ("通信対局（CSA）", True, "自分の着手と相手の着手の両方"),
        ("定跡手の着手", True, "[[定跡ウィンドウ|joseki.html]]から指した手も、盤上の着手として扱われます"),
        ("[[詰将棋対局|tsume-play.html]]", True, "自分の手と玉方の応手の両方。「正解手順」での再生では鳴りません"),
        ("棋譜の再生・分岐の切り替え", False, "読み込んだ棋譜を進めたり戻したりする操作"),
        ("局面編集・解析・検討・詰み探索", False, "駒の配置の変更や、エンジンの読み筋の表示"),
    ],
    yes="鳴る", no="鳴らない",
    after="駒音は、実際の駒音の録音を分析し、その特徴に合わせて合成した音です。Qt Multimediaを使い、OSの標準の音声出力から再生されます。",
    shots={
        "settings-menu": ("「設定」メニューを開き、「駒音の設定…」を選んだところ", "「設定」メニュー：「駒音」でオン・オフ、「駒音の設定…」で音を調整"),
        "piece-sound-settings": ("音量・音の高さ・イコライザーのスライダーと、標準に戻す・試聴・OK・キャンセルのボタンが並ぶ駒音の設定ダイアログ", "駒音の設定ダイアログ（初期状態）"),
    },
    back="← 利用ガイドに戻る",
)
T["en"] = dict(
    title="Piece Sound - ShogiBoardQ",
    desc="How to use the ShogiBoardQ piece sound: turn it on or off from the Settings menu, adjust volume, pitch, and equalizer in Piece Sound Settings, and see when it plays.",
    og_desc="A crisp click plays the moment a piece lands. Turn it on or off and adjust volume, pitch, and equalizer.",
    menu="Menu", lang="Language",
    nav=["Home", "User Guide", "On/Off", "Volume & Tone", "When It Plays"],
    h1="Piece Sound", subtitle="A crisp click plays the moment a piece lands on the board",
    badges=["On/Off Toggle", "Volume Control", "Equalizer"],
    note="Screenshots were captured on Linux with the English interface.",
    s_toggle=("Turn the Piece Sound On or Off", "Toggle the sound at any time from the Settings menu"),
    toggle=("Choose Settings → Piece Sound", [
        "The Settings menu in the menu bar contains Piece Sound. When it is checked, the sound plays with every move. It is on by default, and each time you choose it, it switches between on and off."]),
    saved=("Saved Automatically", [
        "The on/off state is saved automatically and used the next time you start the application. The Settings tab of the [[menu dock|menu.html]] has the same Piece Sound and Piece Sound Settings… buttons."]),
    s_settings=("Adjust Volume and Tone", "Piece Sound Settings controls the volume, pitch, and a three-band equalizer"),
    open=("Open Settings → Piece Sound Settings…", [
        "The dialog shows horizontal sliders for Volume and Pitch, and vertical Low, Mid, and High sliders under Equalizer. When you release a slider, the sound plays with the current settings, so you can tune it by ear. Click Preview to hear the current sound at any time (it plays even when the piece sound is off)."]),
    sliders=("Shape the Sound with the Sliders", "You can also type a value in the box to the right of or below each slider.", [
        ("Volume", "0–100%, 30% by default. The scale follows how loudness is perceived."),
        ("Pitch", "−12 to +12 semitones. Raising it gives a higher, shorter sound; lowering it gives a deeper, longer one."),
        ("Equalizer", "Low, Mid, and High from −12 to +12 dB each. Boosting Mid adds punch; cutting High makes the sound softer."),
    ]),
    finish=("Confirm, Cancel, or Restore Defaults", [
        "Click OK to keep your changes. Cancel returns to the settings that were in effect when the dialog opened. Restore Defaults resets the sound to 30% volume with pitch and equalizer at 0. Confirmed settings are saved automatically and used the next time you start the application.",
        "Use A- and A+ at the bottom left to change the dialog's text size. The text size and the dialog size are also kept for next time."]),
    s_when=("When the Piece Sound Plays", "It plays when a piece moves on the board in a game, not while browsing a game record or editing a position"),
    table_head=("Situation", "Sound", "Notes"),
    rows=[
        ("Human move", True, "Your moves in human vs. human and human vs. engine games"),
        ("Engine move", True, "Engine moves in human vs. engine and engine vs. engine games"),
        ("Network game (CSA)", True, "Both your moves and your opponent's moves"),
        ("Opening book move", True, "Moves played from the [[Opening Book Window|joseki.html]] count as board moves"),
        ("[[Tsume shogi play|tsume-play.html]]", True, "Both your moves and the defender's replies. Replaying the moves with the Solution button is silent"),
        ("Game record playback and branch switching", False, "Moving forward or back through a loaded game record"),
        ("Position editing, analysis, consideration, and mate search", False, "Rearranging pieces or showing engine lines"),
    ],
    yes="Plays", no="Silent",
    after="The sound was synthesized to match an analysis of recorded piece sounds. It plays through Qt Multimedia on the operating system's default audio output.",
    shots={
        "settings-menu": ("Settings menu open with Piece Sound Settings… highlighted", "Settings menu: Piece Sound turns the sound on or off, and Piece Sound Settings… adjusts it"),
        "piece-sound-settings": ("Piece Sound Settings dialog with Volume, Pitch, and Equalizer sliders and the Restore Defaults, Preview, OK, and Cancel buttons", "Piece Sound Settings dialog (default values)"),
    },
    back="← Back to User Guide",
)
T["zh-cn"] = dict(
    title="走子音效 - ShogiBoardQ",
    desc="ShogiBoardQ 走子音效的使用方法：在“设置”菜单中开关音效，在走子音效设置中调整音量、音高和均衡器，并说明音效在哪些场合播放。",
    og_desc="棋子落在棋盘上的瞬间会发出清脆的声音。可以开关音效，并调整音量、音高和均衡器。",
    menu="菜单", lang="界面语言",
    nav=["首页", "使用指南", "开关", "音量与音色", "播放场合"],
    h1="走子音效", subtitle="走子的瞬间，会发出棋子落在棋盘上的清脆声音",
    badges=["开关音效", "调整音量", "均衡器"],
    note="截图在 Linux 上以简体中文界面拍摄。",
    s_toggle=("开关走子音效", "随时可以从“设置”菜单切换走子音效"),
    toggle=("用“设置”→“走子音效”切换", [
        "打开菜单栏的“设置”，其中有“走子音效”项目，带有勾选标记时会播放走子音效。默认为开启，每次选择都会在开启和关闭之间切换。"]),
    saved=("自动保存设置", [
        "开关状态会自动保存，下次启动时继续使用。[[菜单面板|menu.html]]的“设置”标签页中也有相同的“走子音效”和“走子音效设置…”按钮。"]),
    s_settings=("调整音量与音色", "在“走子音效设置”对话框中调整音量、音高和均衡器"),
    open=("打开“设置”→“走子音效设置…”", [
        "对话框中有音量和音高的横向滑块，以及均衡器的低频、中频、高频纵向滑块。松开滑块时会以当前设置播放走子音效，可以边听边调整。点击“试听”随时可以确认当前设置的声音（即使关闭了走子音效也会播放）。"]),
    sliders=("用滑块调出喜欢的声音", "也可以在各滑块右侧或下方的输入框中直接输入数值。", [
        ("音量", "0〜100%，默认值为 30%。刻度符合人耳对音量的感受。"),
        ("音高", "-12〜+12 半音。调高时声音更高更短，调低时声音更低更长。"),
        ("均衡器", "低频、中频、高频分别在 -12〜+12 dB 之间调整。提高中频会增强敲击感，降低高频会使声音更柔和。"),
    ]),
    finish=("确定、取消与恢复默认值", [
        "点击“确定”确认设置。点击“取消”会恢复到打开对话框时的设置。点击“恢复默认值”可恢复为音量 30%、音高 0、均衡器 0 的初始设置。确认的设置会自动保存，下次启动时继续使用。",
        "左下角的“A-”“A+”可以调整对话框的文字大小。文字大小和对话框大小也会在下次继续使用。"]),
    s_when=("播放走子音效的场合", "对局中棋盘上的棋子移动时播放，浏览棋谱或编辑局面时不播放"),
    table_head=("场合", "音效", "说明"),
    rows=[
        ("人类走子", True, "人类对人类、人类对引擎时自己走子"),
        ("引擎走子", True, "人类对引擎、引擎对引擎时引擎走子"),
        ("网络对局 (CSA)", True, "自己和对手的走子都会播放"),
        ("定跡走子", True, "从[[定跡窗口|joseki.html]]走的棋也视为棋盘上的走子"),
        ("[[练习诘棋|tsume-play.html]]", True, "自己的走子和玉方的应手都会播放。用“解答”回放时不播放"),
        ("播放棋谱、切换分支", False, "前进或后退已读取的棋谱"),
        ("编辑局面、分析、研究、搜索诘棋", False, "改变棋子布置，或显示引擎的读筋"),
    ],
    yes="播放", no="不播放",
    after="走子音效是分析实际棋子声音的录音，并按其特征合成的声音。通过 Qt Multimedia 从操作系统的默认音频输出播放。",
    shots={
        "settings-menu": ("打开“设置”菜单并选中“走子音效设置…”", "“设置”菜单：用“走子音效”开关，用“走子音效设置…”调整声音"),
        "piece-sound-settings": ("显示音量、音高、均衡器滑块，以及恢复默认值、试听、确定、取消按钮的走子音效设置对话框", "走子音效设置对话框（初始状态）"),
    },
    back="← 返回使用指南",
)
T["zh-tw"] = dict(
    title="走子音效 - ShogiBoardQ",
    desc="ShogiBoardQ 走子音效的使用方法：在「設定」選單中開關音效，在走子音效設定中調整音量、音高和等化器，並說明音效在哪些場合播放。",
    og_desc="棋子落在棋盤上的瞬間會發出清脆的聲音。可以開關音效，並調整音量、音高和等化器。",
    menu="選單", lang="介面語言",
    nav=["首頁", "使用指南", "開關", "音量與音色", "播放場合"],
    h1="走子音效", subtitle="走子的瞬間，會發出棋子落在棋盤上的清脆聲音",
    badges=["開關音效", "調整音量", "等化器"],
    note="截圖於 Linux 上以繁體中文介面擷取。",
    s_toggle=("開關走子音效", "隨時可以從「設定」選單切換走子音效"),
    toggle=("以「設定」→「走子音效」切換", [
        "開啟選單列的「設定」，其中有「走子音效」項目，有勾選標記時會播放走子音效。預設為開啟，每次選擇都會在開啟和關閉之間切換。"]),
    saved=("自動儲存設定", [
        "開關狀態會自動儲存，下次啟動時繼續使用。[[選單面板|menu.html]]的「設定」分頁中也有相同的「走子音效」和「走子音效設定…」按鈕。"]),
    s_settings=("調整音量與音色", "在「走子音效設定」對話方塊中調整音量、音高和等化器"),
    open=("開啟「設定」→「走子音效設定…」", [
        "對話方塊中有音量和音高的橫向滑桿，以及等化器的低頻、中頻、高頻縱向滑桿。放開滑桿時會以目前設定播放走子音效，可以邊聽邊調整。按一下「試聽」隨時可以確認目前設定的聲音（即使關閉了走子音效也會播放）。"]),
    sliders=("以滑桿調出喜歡的聲音", "也可以在各滑桿右側或下方的輸入框中直接輸入數值。", [
        ("音量", "0〜100%，預設值為 30%。刻度符合人耳對音量的感受。"),
        ("音高", "-12〜+12 半音。調高時聲音更高更短，調低時聲音更低更長。"),
        ("等化器", "低頻、中頻、高頻分別在 -12〜+12 dB 之間調整。提高中頻會增強敲擊感，降低高頻會使聲音更柔和。"),
    ]),
    finish=("確定、取消與恢復預設值", [
        "按一下「確定」確認設定。按一下「取消」會恢復到開啟對話方塊時的設定。按一下「恢復預設值」可恢復為音量 30%、音高 0、等化器 0 的初始設定。確認的設定會自動儲存，下次啟動時繼續使用。",
        "左下角的「A-」「A+」可以調整對話方塊的文字大小。文字大小和對話方塊大小也會在下次繼續使用。"]),
    s_when=("播放走子音效的場合", "對局中棋盤上的棋子移動時播放，瀏覽棋譜或編輯局面時不播放"),
    table_head=("場合", "音效", "說明"),
    rows=[
        ("人類走子", True, "人類對人類、人類對引擎時自己走子"),
        ("引擎走子", True, "人類對引擎、引擎對引擎時引擎走子"),
        ("網路對局 (CSA)", True, "自己和對手的走子都會播放"),
        ("定跡走子", True, "從[[定跡視窗|joseki.html]]走的棋也視為棋盤上的走子"),
        ("[[練習詰棋|tsume-play.html]]", True, "自己的走子和玉方的應手都會播放。以「解答」重播時不播放"),
        ("播放棋譜、切換分支", False, "前進或後退已讀取的棋譜"),
        ("編輯局面、分析、研究、搜尋詰棋", False, "改變棋子佈置，或顯示引擎的讀筋"),
    ],
    yes="播放", no="不播放",
    after="走子音效是分析實際棋子聲音的錄音，並依其特徵合成的聲音。透過 Qt Multimedia 從作業系統的預設音訊輸出播放。",
    shots={
        "settings-menu": ("開啟「設定」選單並選取「走子音效設定…」", "「設定」選單：以「走子音效」開關，以「走子音效設定…」調整聲音"),
        "piece-sound-settings": ("顯示音量、音高、等化器滑桿，以及恢復預設值、試聽、確定、取消按鈕的走子音效設定對話方塊", "走子音效設定對話方塊（初始狀態）"),
    },
    back="← 返回使用指南",
)
