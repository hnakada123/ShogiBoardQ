cmake_minimum_required(VERSION 3.16)

project(ShogiBoardQ VERSION 0.1 LANGUAGES CXX)

# ==== Qt auto features ====
set(CMAKE_AUTOUIC ON)
# .ui ファイルはソースと同じディレクトリに配置
# 他のディレクトリから ui_*.h をインクルードする場合のための検索パス
set(CMAKE_AUTOUIC_SEARCH_PATHS src/app src/dialogs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==== compile_commands.json 生成 (clang-tidy, cppcheck 用) ====
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==== clang-tidy 設定 ====
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE};
            -checks=-*,clang-diagnostic-unused-*,misc-unused-*,clang-analyzer-deadcode.*,readability-redundant-*;
            -header-filter=.*
        )
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# ==== cppcheck 設定（未使用関数検出用） ====
option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK
            ${CPPCHECK_EXE};
            --enable=warning,unusedFunction;
            --inconclusive;
            --inline-suppr;
            --suppress=missingIncludeSystem;
            --suppress=unmatchedSuppression;
            --suppress=unknownMacro;
            --suppress=syntaxError:*elidelabel.h;
            --suppress=syntaxError:*_autogen/*;
            --quiet
        )
    else()
        message(WARNING "cppcheck not found, static analysis disabled")
    endif()
endif()

# ==== コンパイラ警告オプション ====
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wformat=2
        -Wimplicit-fallthrough
        # 未使用系
        -Wunused
        -Wunused-function
        -Wunused-variable
        -Wunused-parameter
    )
    # Qt6のqCDebug/qCInfo/qCWarningマクロはstream構文（<< "..."）で
    # variadic引数が空になるため、C++17では警告が出る。Qt側のマクロ設計の問題。
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-variadic-macro-arguments-omitted)
    endif()
    # -Wnull-dereference は GCC の最適化（特にインライン展開）により
    # Qt ヘッダやメンバ変数アクセスで誤検知が多発するため無効化
endif()

# --- 厳格なコンパイルオプション
#if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#    add_compile_options(
#        -Wall
#        -Wextra
#        -Wpedantic
#        -Wshadow                    # 変数のシャドウイング
#        -Wconversion                # 暗黙の型変換
#        -Wsign-conversion           # 符号付き/符号なしの変換
#        -Wnon-virtual-dtor          # 仮想デストラクタがない
#        -Wold-style-cast            # Cスタイルキャスト
#        -Wcast-align                # アライメントを増加させるキャスト
#        -Woverloaded-virtual        # 仮想関数の隠蔽
#        -Wnull-dereference          # nullポインタ参照の可能性
#        -Wdouble-promotion          # floatからdoubleへの暗黙変換
#        -Wformat=2                  # printf系のフォーマット文字列チェック
#        -Wimplicit-fallthrough      # switch文のフォールスルー
#    )
#
#    # GCC固有
#    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#        add_compile_options(
#            -Wduplicated-cond       # 重複した条件
#            -Wduplicated-branches   # 重複した分岐
#            -Wlogical-op            # 論理演算の誤り
#            -Wuseless-cast          # 不要なキャスト
#        )
#    endif()
#
#    # Clang固有
#    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#        add_compile_options(
#            -Weverything            # 全ての警告（非常に厳格）
#            -Wno-c++98-compat       # C++98互換性警告を除外
#            -Wno-c++98-compat-pedantic
#            -Wno-padded             # パディング警告を除外（うるさい）
#        )
#    endif()
#endif()
# --- ここまで

# ==== Qt6 自動検出 ====
# CMAKE_PREFIX_PATH が未設定の場合、一般的なインストールパスから Qt6 を探す
if(NOT CMAKE_PREFIX_PATH AND NOT Qt6_DIR AND NOT DEFINED ENV{QTDIR})
    # 探索候補のベースディレクトリ
    if(APPLE)
        set(_qt_platform_suffix "macos")
    elseif(WIN32)
        set(_qt_platform_suffix "msvc*")
    else()
        set(_qt_platform_suffix "gcc_64")
    endif()

    # Qt インストーラの標準パス
    set(_qt_search_roots
        "$ENV{HOME}/Qt"        # macOS / Linux
        "C:/Qt"                # Windows
        "D:/Qt"                # Windows (別ドライブ)
    )

    foreach(_root ${_qt_search_roots})
        file(GLOB _qt_versions "${_root}/6.*")
        if(_qt_versions)
            # バージョン番号でソートして最新を選択
            list(SORT _qt_versions COMPARE NATURAL ORDER DESCENDING)
            foreach(_ver_dir ${_qt_versions})
                file(GLOB _qt_platform_dirs "${_ver_dir}/${_qt_platform_suffix}")
                if(_qt_platform_dirs)
                    list(GET _qt_platform_dirs 0 _qt_dir)
                    set(CMAKE_PREFIX_PATH "${_qt_dir}")
                    message(STATUS "Auto-detected Qt6: ${_qt_dir}")
                    break()
                endif()
            endforeach()
            if(CMAKE_PREFIX_PATH)
                break()
            endif()
        endif()
    endforeach()

    unset(_qt_platform_suffix)
    unset(_qt_search_roots)
    unset(_qt_versions)
    unset(_qt_platform_dirs)
    unset(_qt_dir)
endif()

# ==== Qt6 packages ====
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Charts Network)

# ==== Translations (.ts) ====
set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
    resources/translations/ShogiBoardQ_en.ts
)

# ==== UI (.ui) ====
set(UI_FILES
    src/app/mainwindow.ui
    src/dialogs/changeenginesettingsdialog.ui
    src/dialogs/considerationdialog.ui
    src/dialogs/csagamedialog.ui
    src/dialogs/engineregistrationdialog.ui
    src/dialogs/kifuanalysisdialog.ui
    src/dialogs/promotedialog.ui
    src/dialogs/startgamedialog.ui
    src/dialogs/versiondialog.ui
)

# ==== Sources (機能別ディレクトリ構造) ====
# ソースファイル一覧の更新: ./scripts/update-sources.sh

set(SRC_APP
    src/app/commentcoordinator.cpp
    src/app/commentcoordinator.h
    src/app/dockcreationservice.cpp
    src/app/dockcreationservice.h
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/kifunavigationcoordinator.cpp
    src/app/kifunavigationcoordinator.h
    src/app/boardloadservice.cpp
    src/app/boardloadservice.h
    src/app/gamerecordupdateservice.cpp
    src/app/gamerecordupdateservice.h
    src/app/turnstatesyncservice.cpp
    src/app/turnstatesyncservice.h
    src/app/livegamesessionupdater.cpp
    src/app/livegamesessionupdater.h
    src/app/mainwindowcompositionroot.cpp
    src/app/mainwindowcompositionroot.h
    src/app/mainwindowdepsfactory.cpp
    src/app/mainwindowdepsfactory.h
    src/app/mainwindowgamestartservice.cpp
    src/app/mainwindowgamestartservice.h
    src/app/mainwindowmatchwiringdepsservice.cpp
    src/app/mainwindowmatchwiringdepsservice.h
    src/app/mainwindowresetservice.cpp
    src/app/mainwindowresetservice.h
    src/app/mainwindowruntimerefs.h
    src/app/mainwindowuibootstrapper.cpp
    src/app/mainwindowuibootstrapper.h
    src/app/playmodepolicyservice.cpp
    src/app/playmodepolicyservice.h
)

set(SRC_CORE
    src/core/enginemovevalidator.cpp
    src/core/enginemovevalidator.h
    src/core/fmvattacks.cpp
    src/core/fmvattacks.h
    src/core/fmvbitboard81.cpp
    src/core/fmvbitboard81.h
    src/core/fmvconverter.cpp
    src/core/fmvconverter.h
    src/core/fmvlegalcore.cpp
    src/core/fmvlegalcore.h
    src/core/fmvposition.cpp
    src/core/fmvposition.h
    src/core/fmvtypes.h
    src/core/legalmovestatus.h
    src/core/playmode.h
    src/core/sfenutils.h
    src/core/shogiboard.cpp
    src/core/shogiboard.h
    src/core/shogiclock.cpp
    src/core/shogiclock.h
    src/core/shogimove.cpp
    src/core/shogimove.h
    src/core/shogitypes.h
    src/core/shogiutils.cpp
    src/core/shogiutils.h
)

set(SRC_GAME
    src/game/analysissessionhandler.cpp
    src/game/analysissessionhandler.h
    src/game/consecutivegamescontroller.cpp
    src/game/consecutivegamescontroller.h
    src/game/enginegameovernotifier.cpp
    src/game/enginegameovernotifier.h
    src/game/gameendhandler.cpp
    src/game/gameendhandler.h
    src/game/gamestartorchestrator.cpp
    src/game/gamestartorchestrator.h
    src/game/enginevsenginestrategy.cpp
    src/game/enginevsenginestrategy.h
    src/game/gamemodestrategy.h
    src/game/gamesessionfacade.cpp
    src/game/gamesessionfacade.h
    src/game/gamestartcoordinator.cpp
    src/game/gamestartcoordinator.h
    src/game/gamestatecontroller.cpp
    src/game/gamestatecontroller.h
    src/game/humanvsenginestrategy.cpp
    src/game/humanvsenginestrategy.h
    src/game/humanvshumanstrategy.cpp
    src/game/humanvshumanstrategy.h
    src/game/matchcoordinator.cpp
    src/game/matchcoordinator.h
    src/game/prestartcleanuphandler.cpp
    src/game/prestartcleanuphandler.h
    src/game/promotionflow.cpp
    src/game/promotionflow.h
    src/game/sennichitedetector.cpp
    src/game/sennichitedetector.h
    src/game/shogigamecontroller.cpp
    src/game/shogigamecontroller.h
    src/game/strategycontext.h
    src/game/turnmanager.cpp
    src/game/turnmanager.h
    src/game/turnsyncbridge.cpp
    src/game/turnsyncbridge.h
)

set(SRC_KIFU
    src/kifu/formats/csaexporter.cpp
    src/kifu/formats/csaexporter.h
    src/kifu/formats/csatosfenconverter.cpp
    src/kifu/formats/csatosfenconverter.h
    src/kifu/formats/jkfexporter.cpp
    src/kifu/formats/jkfexporter.h
    src/kifu/formats/jkftosfenconverter.cpp
    src/kifu/formats/jkftosfenconverter.h
    src/kifu/formats/ki2tosfenconverter.cpp
    src/kifu/formats/ki2tosfenconverter.h
    src/kifu/formats/kiftosfenconverter.cpp
    src/kifu/formats/kiftosfenconverter.h
    src/kifu/formats/notationutils.cpp
    src/kifu/formats/notationutils.h
    src/kifu/formats/parsecommon.cpp
    src/kifu/formats/parsecommon.h
    src/kifu/formats/sfencsapositionconverter.cpp
    src/kifu/formats/sfencsapositionconverter.h
    src/kifu/formats/usentosfenconverter.cpp
    src/kifu/formats/usentosfenconverter.h
    src/kifu/formats/usitosfenconverter.cpp
    src/kifu/formats/usitosfenconverter.h
    src/kifu/gamerecordmodel.cpp
    src/kifu/gamerecordmodel.h
    src/kifu/kifdisplayitem.h
    src/kifu/kifparsetypes.h
    src/kifu/kifreader.cpp
    src/kifu/kifreader.h
    src/kifu/kifubranchtree.cpp
    src/kifu/kifubranchtree.h
    src/kifu/kifubranchtreebuilder.cpp
    src/kifu/kifubranchtreebuilder.h
    src/kifu/kifubranchnode.cpp
    src/kifu/kifubranchnode.h
    src/kifu/kifuclipboardservice.cpp
    src/kifu/kifuclipboardservice.h
    src/kifu/considerationpositionresolver.cpp
    src/kifu/considerationpositionresolver.h
    src/kifu/kifucontentbuilder.cpp
    src/kifu/kifucontentbuilder.h
    src/kifu/kifuexportcontroller.cpp
    src/kifu/kifuexportcontroller.h
    src/kifu/kifufilecontroller.cpp
    src/kifu/kifufilecontroller.h
    src/kifu/kifuioservice.cpp
    src/kifu/kifuioservice.h
    src/kifu/kifuloadcoordinator.cpp
    src/kifu/kifuloadcoordinator.h
    src/kifu/kifunavigationstate.cpp
    src/kifu/kifunavigationstate.h
    src/kifu/kifusavecoordinator.cpp
    src/kifu/kifusavecoordinator.h
    src/kifu/kifutypes.h
    src/kifu/livegamesession.cpp
    src/kifu/livegamesession.h
    src/kifu/shogiinforecord.cpp
    src/kifu/shogiinforecord.h
)

set(SRC_ANALYSIS
    src/analysis/analysiscoordinator.cpp
    src/analysis/analysiscoordinator.h
    src/analysis/analysisflowcontroller.cpp
    src/analysis/analysisflowcontroller.h
    src/analysis/analysisresultspresenter.cpp
    src/analysis/analysisresultspresenter.h
    src/analysis/considerationflowcontroller.cpp
    src/analysis/considerationflowcontroller.h
    src/analysis/considerationmodeuicontroller.cpp
    src/analysis/considerationmodeuicontroller.h
    src/analysis/kifuanalysislistmodel.cpp
    src/analysis/kifuanalysislistmodel.h
    src/analysis/tsumeshogigenerator.cpp
    src/analysis/tsumeshogigenerator.h
    src/analysis/tsumeshogipositiongenerator.cpp
    src/analysis/tsumeshogipositiongenerator.h
    src/analysis/tsumesearchflowcontroller.cpp
    src/analysis/tsumesearchflowcontroller.h
)

set(SRC_ENGINE
    src/engine/engineoptiondescriptions.cpp
    src/engine/engineoptiondescriptions.h
    src/engine/engineoptions.h
    src/engine/engineprocessmanager.cpp
    src/engine/engineprocessmanager.h
    src/engine/enginesettingsconstants.h
    src/engine/enginesettingscoordinator.cpp
    src/engine/enginesettingscoordinator.h
    src/engine/shogiengineinfoparser.cpp
    src/engine/shogiengineinfoparser.h
    src/engine/shogienginethinkingmodel.cpp
    src/engine/shogienginethinkingmodel.h
    src/engine/thinkinginfopresenter.cpp
    src/engine/thinkinginfopresenter.h
    src/engine/usi.cpp
    src/engine/usi.h
    src/engine/usicommlogmodel.cpp
    src/engine/usicommlogmodel.h
    src/engine/usiprotocolhandler.cpp
    src/engine/usiprotocolhandler.h
)

set(SRC_NETWORK
    src/network/csaclient.cpp
    src/network/csaclient.h
    src/network/csagamecoordinator.cpp
    src/network/csagamecoordinator.h
)

set(SRC_NAVIGATION
    src/navigation/kifunavigationcontroller.cpp
    src/navigation/kifunavigationcontroller.h
    src/navigation/recordnavigationhandler.cpp
    src/navigation/recordnavigationhandler.h
)

set(SRC_BOARD
    src/board/boardimageexporter.cpp
    src/board/boardimageexporter.h
    src/board/boardinteractioncontroller.cpp
    src/board/boardinteractioncontroller.h
    src/board/positioneditcontroller.cpp
    src/board/positioneditcontroller.h
    src/board/sfenpositiontracer.cpp
    src/board/sfenpositiontracer.h
)

set(SRC_UI
    src/ui/controllers/boardsetupcontroller.cpp
    src/ui/controllers/boardsetupcontroller.h
    src/ui/controllers/evaluationgraphcontroller.cpp
    src/ui/controllers/evaluationgraphcontroller.h
    src/ui/controllers/jishogiscoredialogcontroller.cpp
    src/ui/controllers/jishogiscoredialogcontroller.h
    src/ui/controllers/languagecontroller.cpp
    src/ui/controllers/languagecontroller.h
    src/ui/controllers/nyugyokudeclarationhandler.cpp
    src/ui/controllers/nyugyokudeclarationhandler.h
    src/ui/controllers/playerinfocontroller.cpp
    src/ui/controllers/playerinfocontroller.h
    src/ui/controllers/pvclickcontroller.cpp
    src/ui/controllers/pvclickcontroller.h
    src/ui/controllers/replaycontroller.cpp
    src/ui/controllers/replaycontroller.h
    src/ui/controllers/timecontrolcontroller.cpp
    src/ui/controllers/timecontrolcontroller.h
    src/ui/controllers/usicommandcontroller.cpp
    src/ui/controllers/usicommandcontroller.h
    src/ui/coordinators/aboutcoordinator.cpp
    src/ui/coordinators/aboutcoordinator.h
    src/ui/coordinators/dialogcoordinator.cpp
    src/ui/coordinators/dialogcoordinator.h
    src/ui/coordinators/dialogorchestrator.cpp
    src/ui/coordinators/dialogorchestrator.h
    src/ui/coordinators/docklayoutmanager.cpp
    src/ui/coordinators/docklayoutmanager.h
    src/ui/coordinators/dockuicoordinator.cpp
    src/ui/coordinators/dockuicoordinator.h
    src/ui/coordinators/gamelayoutbuilder.cpp
    src/ui/coordinators/gamelayoutbuilder.h
    src/ui/coordinators/kifudisplaycoordinator.cpp
    src/ui/coordinators/kifudisplaycoordinator.h
    src/ui/coordinators/positioneditcoordinator.cpp
    src/ui/coordinators/positioneditcoordinator.h
    src/ui/coordinators/uistatepolicymanager.cpp
    src/ui/coordinators/uistatepolicymanager.h
    src/ui/presenters/boardsyncpresenter.cpp
    src/ui/presenters/boardsyncpresenter.h
    src/ui/presenters/evalgraphpresenter.cpp
    src/ui/presenters/evalgraphpresenter.h
    src/ui/presenters/gamerecordpresenter.cpp
    src/ui/presenters/gamerecordpresenter.h
    src/ui/presenters/navigationpresenter.cpp
    src/ui/presenters/navigationpresenter.h
    src/ui/presenters/timedisplaypresenter.cpp
    src/ui/presenters/timedisplaypresenter.h
    src/ui/wiring/analysistabwiring.cpp
    src/ui/wiring/analysistabwiring.h
    src/ui/wiring/considerationwiring.cpp
    src/ui/wiring/considerationwiring.h
    src/ui/wiring/csagamewiring.cpp
    src/ui/wiring/csagamewiring.h
    src/ui/wiring/branchnavigationwiring.cpp
    src/ui/wiring/branchnavigationwiring.h
    src/ui/wiring/dialogcoordinatorwiring.cpp
    src/ui/wiring/dialogcoordinatorwiring.h
    src/ui/wiring/dialoglaunchwiring.cpp
    src/ui/wiring/dialoglaunchwiring.h
    src/ui/wiring/editactionswiring.cpp
    src/ui/wiring/editactionswiring.h
    src/ui/wiring/fileactionswiring.cpp
    src/ui/wiring/fileactionswiring.h
    src/ui/wiring/gameactionswiring.cpp
    src/ui/wiring/gameactionswiring.h
    src/ui/wiring/josekiwindowwiring.cpp
    src/ui/wiring/josekiwindowwiring.h
    src/ui/wiring/matchcoordinatorhooksfactory.cpp
    src/ui/wiring/matchcoordinatorhooksfactory.h
    src/ui/wiring/matchcoordinatorwiring.cpp
    src/ui/wiring/matchcoordinatorwiring.h
    src/ui/wiring/menuwindowwiring.cpp
    src/ui/wiring/menuwindowwiring.h
    src/ui/wiring/playerinfowiring.cpp
    src/ui/wiring/playerinfowiring.h
    src/ui/wiring/recordnavigationwiring.cpp
    src/ui/wiring/recordnavigationwiring.h
    src/ui/wiring/recordpanewiring.cpp
    src/ui/wiring/recordpanewiring.h
    src/ui/wiring/uiactionswiring.cpp
    src/ui/wiring/uiactionswiring.h
    src/ui/wiring/viewactionswiring.cpp
    src/ui/wiring/viewactionswiring.h
)

set(SRC_VIEWS
    src/views/shogiview.cpp
    src/views/shogiview.h
    src/views/shogiviewhighlighting.cpp
    src/views/shogiviewhighlighting.h
    src/views/shogiviewinteraction.cpp
    src/views/shogiviewinteraction.h
    src/views/shogiviewlayout.cpp
    src/views/shogiviewlayout.h
)

set(SRC_WIDGETS
    src/widgets/apptooltipfilter.cpp
    src/widgets/apptooltipfilter.h
    src/widgets/branchrowdelegate.cpp
    src/widgets/branchrowdelegate.h
    src/widgets/branchtreemanager.cpp
    src/widgets/branchtreemanager.h
    src/widgets/branchtreewidget.cpp
    src/widgets/branchtreewidget.h
    src/widgets/collapsiblegroupbox.cpp
    src/widgets/collapsiblegroupbox.h
    src/widgets/commenteditorpanel.cpp
    src/widgets/commenteditorpanel.h
    src/widgets/considerationtabmanager.cpp
    src/widgets/considerationtabmanager.h
    src/widgets/elidelabel.cpp
    src/widgets/elidelabel.h
    src/widgets/engineanalysistab.cpp
    src/widgets/engineanalysistab.h
    src/widgets/engineinfowidget.cpp
    src/widgets/engineinfowidget.h
    src/widgets/evaluationchartwidget.cpp
    src/widgets/evaluationchartwidget.h
    src/widgets/flowlayout.cpp
    src/widgets/flowlayout.h
    src/widgets/gameinfopanecontroller.cpp
    src/widgets/gameinfopanecontroller.h
    src/widgets/globaltooltip.cpp
    src/widgets/globaltooltip.h
    src/widgets/kifuanalysisresultsdisplay.cpp
    src/widgets/kifuanalysisresultsdisplay.h
    src/widgets/kifubranchdisplay.cpp
    src/widgets/kifubranchdisplay.h
    src/widgets/kifudisplay.cpp
    src/widgets/kifudisplay.h
    src/widgets/logviewfontmanager.cpp
    src/widgets/logviewfontmanager.h
    src/widgets/longlongspinbox.cpp
    src/widgets/longlongspinbox.h
    src/widgets/menubuttonwidget.cpp
    src/widgets/menubuttonwidget.h
    src/widgets/numericrightaligncommadelegate.h
    src/widgets/recordpane.cpp
    src/widgets/recordpane.h
)

set(SRC_DIALOGS
    src/dialogs/changeenginesettingsdialog.cpp
    src/dialogs/changeenginesettingsdialog.h
    src/dialogs/considerationdialog.cpp
    src/dialogs/considerationdialog.h
    src/dialogs/csagamedialog.cpp
    src/dialogs/csagamedialog.h
    src/dialogs/csawaitingdialog.cpp
    src/dialogs/csawaitingdialog.h
    src/dialogs/engineregistrationdialog.cpp
    src/dialogs/engineregistrationdialog.h
    src/dialogs/josekimergedialog.cpp
    src/dialogs/josekimergedialog.h
    src/dialogs/josekimovedialog.cpp
    src/dialogs/josekimovedialog.h
    src/dialogs/josekiwindow.cpp
    src/dialogs/josekiwindow.h
    src/dialogs/kifuanalysisdialog.cpp
    src/dialogs/kifuanalysisdialog.h
    src/dialogs/kifupastedialog.cpp
    src/dialogs/kifupastedialog.h
    src/dialogs/menuwindow.cpp
    src/dialogs/menuwindow.h
    src/dialogs/promotedialog.cpp
    src/dialogs/promotedialog.h
    src/dialogs/pvboarddialog.cpp
    src/dialogs/pvboarddialog.h
    src/dialogs/sfencollectiondialog.cpp
    src/dialogs/sfencollectiondialog.h
    src/dialogs/startgamedialog.cpp
    src/dialogs/startgamedialog.h
    src/dialogs/tsumeshogigeneratordialog.cpp
    src/dialogs/tsumeshogigeneratordialog.h
    src/dialogs/tsumeshogisearchdialog.cpp
    src/dialogs/tsumeshogisearchdialog.h
    src/dialogs/versiondialog.cpp
    src/dialogs/versiondialog.h
)

set(SRC_MODELS
    src/models/abstractlistmodel.h
    src/models/kifubranchlistmodel.cpp
    src/models/kifubranchlistmodel.h
    src/models/kifurecordlistmodel.cpp
    src/models/kifurecordlistmodel.h
)

set(SRC_SERVICES
    src/services/debugscreenshotservice.cpp
    src/services/debugscreenshotservice.h
    src/services/debugscreenshotwiring.cpp
    src/services/debugscreenshotwiring.h
    src/services/playernameservice.cpp
    src/services/playernameservice.h
    src/services/settingskeys.h
    src/services/settingsservice.cpp
    src/services/settingsservice.h
    src/services/timecontrolutil.cpp
    src/services/timecontrolutil.h
    src/services/timekeepingservice.cpp
    src/services/timekeepingservice.h
)

set(SRC_COMMON
    src/common/buttonstyles.h
    src/common/errorbus.cpp
    src/common/errorbus.h
    src/common/jishogicalculator.cpp
    src/common/jishogicalculator.h
    src/common/logcategories.cpp
    src/common/logcategories.h
    src/common/tsumepositionutil.cpp
    src/common/tsumepositionutil.h
)

# ==== Application Icon ====
# Windows用リソースファイル
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/platform/app.rc")
endif()

# macOS用アイコン
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE shogiboardq.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/shogiboardq.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

# ==== Executable ====
qt_add_executable(ShogiBoardQ
    MANUAL_FINALIZATION
    ${SRC_APP}
    ${SRC_CORE}
    ${SRC_GAME}
    ${SRC_KIFU}
    ${SRC_ANALYSIS}
    ${SRC_ENGINE}
    ${SRC_NETWORK}
    ${SRC_NAVIGATION}
    ${SRC_BOARD}
    ${SRC_UI}
    ${SRC_VIEWS}
    ${SRC_WIDGETS}
    ${SRC_DIALOGS}
    ${SRC_MODELS}
    ${SRC_SERVICES}
    ${SRC_COMMON}
    ${UI_FILES}
    resources/shogiboardq.qrc
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_ICON_MACOS}
)

# .qm 生成（出力はビルド側に）
# 注意: CMAKE_SOURCE_DIR 全体を渡すとビルドディレクトリ内の autogen ファイルも
# スキャンされ、循環依存が発生するため、ソースディレクトリのみを指定する
# .ui ファイルはソースディレクトリに含まれる
qt_create_translation(QM_FILES
    ${CMAKE_SOURCE_DIR}/src
    ${TS_FILES}
)

# .qm ファイルをビルドターゲットとして追加
add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShogiBoardQ translations)

# .qm ファイルを実行可能ファイルと同じディレクトリにコピー
foreach(QM_FILE ${QM_FILES})
    add_custom_command(TARGET ShogiBoardQ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QM_FILE}
            $<TARGET_FILE_DIR:ShogiBoardQ>
        COMMENT "Copying ${QM_FILE} to output directory"
    )
endforeach()

# ==== Includes / Links ====
# ヘッダーはソースと同じディレクトリに配置
target_include_directories(ShogiBoardQ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu/formats
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/navigation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/coordinators
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/presenters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(ShogiBoardQ PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
)

# アプリケーションバージョン (CalVer: YYYY.MM.DD)
set(APP_VERSION "2026.02.25")

# リリースビルドではqDebug()出力を無効化
target_compile_definitions(ShogiBoardQ PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
    APP_VERSION="${APP_VERSION}"
)

# ==== Bundle properties ====
set_target_properties(ShogiBoardQ PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ShogiBoardQ
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ==== Testing ====
option(BUILD_TESTING "Build test executables" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==== Install ====
include(GNUInstallDirs)

# 実行ファイル
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# デスクトップエントリ
install(FILES resources/platform/shogiboardq.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# アプリケーションアイコン（256x256）
install(FILES resources/icons/shogiboardq.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
)

# 翻訳ファイル（.qm）
install(FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==== Finalize ====
qt_finalize_executable(ShogiBoardQ)
