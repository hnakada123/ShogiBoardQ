cmake_minimum_required(VERSION 3.16...3.28)

# アプリケーションバージョン (CalVer: YYYY.MM.DD)
# CMake の project VERSION / バンドル埋め込み / APP_VERSION マクロで共通利用する。
# リリースする日にだけ、その日の日付に上げる（docs/dev/versioning.md）。
set(APP_VERSION "2026.10.23")

project(ShogiBoardQ VERSION ${APP_VERSION} LANGUAGES CXX)

# ==== 実行ファイルの堅牢化 ====
# スタックの保護・FORTIFY・制御フローの保護・PIE・RELRO（セキュリティ監査 S16）。
# 配布する実行ファイル（同梱の Hayanagi も）に付けるので、Hayanagi を組み込む前に設定する。
# ディストリビューションが同じフラグを足しても同じ値になる
option(ENABLE_HARDENING "Build with stack protector, FORTIFY_SOURCE, CET, PIE and full RELRO" ON)

if(ENABLE_HARDENING)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    include(CheckPIESupported)
    check_pie_supported()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fstack-protector-strong)
        # FORTIFY は最適化したビルドでだけ働く（Debug で付けると警告になる。サニタイザーとは併用しない）
        if(NOT ENABLE_SANITIZERS)
            add_compile_options($<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE> $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
        endif()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$")
            add_compile_options(-fcf-protection=full)
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_link_options(-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
        endif()
    elseif(MSVC)
        add_compile_options(/guard:cf)
        add_link_options(/guard:cf)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            add_link_options(/CETCOMPAT)
        endif()
    endif()
endif()

# Hayanagi is built both as a USI engine and as the embedded tsume opponent.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Hayanagi/CMakeLists.txt")
    message(FATAL_ERROR
        "Hayanagi submodule is missing. Run from the repository root: "
        "git submodule update --init --recursive")
endif()
add_subdirectory(Hayanagi)

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
        # チェック内容は .clang-tidy ファイルで管理。
        # 一部環境で GCC 由来の -mno-direct-extern-access が clang-tidy 実行時に
        # 未対応フラグとして失敗するため、ラッパー経由で除去する。
        set(CLANG_TIDY_WRAPPER "${CMAKE_SOURCE_DIR}/scripts/clang_tidy_wrapper.sh")
        if(EXISTS "${CLANG_TIDY_WRAPPER}")
            set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_WRAPPER};${CLANG_TIDY_EXE}")
        else()
            set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
        endif()
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# ==== cppcheck 設定 ====
option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)
option(ENABLE_CPPCHECK_UNUSED_FUNCTION "Enable noisy cppcheck unusedFunction checks" OFF)

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        set(_cppcheck_enable "warning,style,performance,portability")
        if(ENABLE_CPPCHECK_UNUSED_FUNCTION)
            set(_cppcheck_enable "${_cppcheck_enable},unusedFunction")
        endif()
        set(CMAKE_CXX_CPPCHECK
            ${CPPCHECK_EXE};
            --enable=${_cppcheck_enable};
            --inconclusive;
            --inline-suppr;
            --suppress=missingIncludeSystem;
            --suppress=unmatchedSuppression;
            --suppress=unknownMacro;
            --suppress=syntaxError:*elidelabel.h;
            --suppress=syntaxError:*_autogen/*;
            --quiet
        )
        unset(_cppcheck_enable)
    else()
        message(WARNING "cppcheck not found, static analysis disabled")
    endif()
endif()

# ==== -Werror 設定 ====
option(ENABLE_WERROR "Treat compiler warnings as errors" OFF)

# ==== Sanitizer 設定 ====
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
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
        add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
    endif()
    # GCC 固有の追加警告
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
            -Wold-style-cast
        )
    endif()
    # -Wnull-dereference は GCC の最適化（特にインライン展開）により
    # Qt ヘッダやメンバ変数アクセスで誤検知が多発するため無効化
    if(ENABLE_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

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
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets LinguistTools Charts Network Concurrent Multimedia Sql)

# ==== Translations (.ts) ====
set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
    resources/translations/ShogiBoardQ_en.ts
    resources/translations/ShogiBoardQ_zh_CN.ts
    resources/translations/ShogiBoardQ_zh_TW.ts
)

# ==== UI (.ui) ====
set(UI_FILES
    src/app/mainwindow.ui
    src/dialogs/changeenginesettingsdialog.ui
    src/dialogs/tsumeshogisearchdialog.ui
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
    src/app/applicationlogging.cpp
    src/app/applicationlogging.h
    src/app/gamesession.cpp
    src/app/gamesession.h
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindowactions.cpp
    src/app/mainwindowactions.h
    src/app/mainwindowcomponents.h
    src/app/mainwindowcomposer.cpp
    src/app/mainwindowcomposer.h
    src/app/mainwindowcomposer_automation.cpp
    src/app/mainwindowcomposer_create.cpp
    src/app/mainwindowcomposer_wire.cpp
    src/app/menuactionswiring.cpp
    src/app/menuactionswiring.h
    src/app/sessionresetcoordinator.cpp
    src/app/sessionresetcoordinator.h
)

set(SRC_CORE
    src/core/boardconstants.h
    src/core/boardcoordinates.h
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
    src/core/fmvlegalcore_internal.h
    src/core/fmvmovegeneration.cpp
    src/core/fmvposition.cpp
    src/core/fmvposition.h
    src/core/fmvtypes.h
    src/core/handicaps.h
    src/core/josekiplayoptions.h
    src/core/legalmovestatus.h
    src/core/matchplayers.h
    src/core/piecerules.h
    src/core/positionvalidator.cpp
    src/core/positionvalidator.h
    src/core/pvtext.cpp
    src/core/pvtext.h
    src/core/sfenvalidator.cpp
    src/core/sfenvalidator.h
    src/core/shogiposition.cpp
    src/core/shogiposition.h
    src/core/recordtime.h
    src/core/sfenreplay.cpp
    src/core/sfenreplay.h
    src/core/sfenutils.h
    src/core/shogiboard.cpp
    src/core/shogiboard.h
    src/core/shogiclock.cpp
    src/core/shogiclock.h
    src/core/shogimove.cpp
    src/core/shogimove.h
    src/core/shoginotation.h
    src/core/shogitypes.h
    src/core/terminaltype.cpp
    src/core/terminaltype.h
    src/core/timecontrol.h
    src/core/usimove.h
    src/core/usiposition.cpp
    src/core/usiposition.h
    src/core/usiscore.cpp
    src/core/usiscore.h
)

set(SRC_GAME
    src/game/consecutivegameseries.cpp
    src/game/consecutivegameseries.h
    src/game/enginevsenginestrategy.cpp
    src/game/enginevsenginestrategy.h
    src/game/gamemodestrategy.h
    src/game/gamestartoptionsbuilder.cpp
    src/game/gamestartoptionsbuilder.h
    src/game/gameboard.cpp
    src/game/gameboard.h
    src/game/gamechannel.cpp
    src/game/gamechannel.h
    src/game/humanvsenginestrategy.cpp
    src/game/humanvsenginestrategy.h
    src/game/humanvshumanstrategy.cpp
    src/game/humanvshumanstrategy.h
    src/game/matchautosave.h
    src/game/matchcoordinator.cpp
    src/game/matchcoordinator.h
    src/game/matchengineseat.cpp
    src/game/matchengineseat.h
    src/game/matchrecordsink.h
    src/kifu/kifudocument.cpp
    src/kifu/kifudocument.h
    src/kifu/kifuexport.cpp
    src/kifu/kifuexport.h
    src/kifu/matchtimerecord.cpp
    src/kifu/matchtimerecord.h
    src/game/nyugyokujudgement.cpp
    src/game/nyugyokujudgement.h
    src/game/sennichitedetector.cpp
    src/game/sennichitedetector.h
    src/game/startgamedialogdata.h
    src/game/startoptions.h
    src/game/timecontrolvalidator.cpp
    src/game/timecontrolvalidator.h
)

set(SRC_KIFU
    src/kifu/formats/bodtextgenerator.cpp
    src/kifu/formats/bodtextgenerator.h
    src/kifu/formats/csaexporter.cpp
    src/kifu/formats/csaexporter.h
    src/kifu/formats/csaformatter.cpp
    src/kifu/formats/csaformatter.h
    src/kifu/formats/csalexer.cpp
    src/kifu/formats/csalexer.h
    src/kifu/formats/csatosfenconverter.cpp
    src/kifu/formats/csatosfenconverter.h
    src/kifu/formats/jkfcodec.cpp
    src/kifu/formats/jkfcodec.h
    src/kifu/formats/jkfexporter.cpp
    src/kifu/formats/jkfexporter.h
    src/kifu/formats/jkfformatter.cpp
    src/kifu/formats/jkfformatter.h
    src/kifu/formats/jkftosfenconverter.cpp
    src/kifu/formats/jkftosfenconverter.h
    src/kifu/formats/ki2exporter.cpp
    src/kifu/formats/ki2exporter.h
    src/kifu/formats/ki2lexer.cpp
    src/kifu/formats/ki2lexer.h
    src/kifu/formats/ki2movenotation.cpp
    src/kifu/formats/ki2movenotation.h
    src/kifu/formats/kifexporter.cpp
    src/kifu/formats/kifexporter.h
    src/kifu/formats/kiflexer.cpp
    src/kifu/formats/kiflexer.h
    src/kifu/formats/kiflinebuilder.cpp
    src/kifu/formats/kiflinebuilder.h
    src/kifu/formats/kifmovegrammar.cpp
    src/kifu/formats/kifmovegrammar.h
    src/kifu/formats/kifmovetext.h
    src/kifu/formats/kiftextparser.cpp
    src/kifu/formats/kiftextparser.h
    src/kifu/formats/kifuexportcommon.cpp
    src/kifu/formats/kifuexportcommon.h
    src/kifu/formats/parsecommon.cpp
    src/kifu/formats/parsecommon.h
    src/kifu/formats/sfencsapositionconverter.cpp
    src/kifu/formats/sfencsapositionconverter.h
    src/kifu/formats/terminalcodes.cpp
    src/kifu/formats/terminalcodes.h
    src/kifu/formats/kifunodereplay.cpp
    src/kifu/formats/kifunodereplay.h
    src/kifu/formats/usencodec.cpp
    src/kifu/formats/usencodec.h
    src/kifu/formats/usenexporter.cpp
    src/kifu/formats/usenexporter.h
    src/kifu/formats/usentosfenconverter.cpp
    src/kifu/formats/usentosfenconverter.h
    src/kifu/formats/usiexporter.cpp
    src/kifu/formats/usiexporter.h
    src/kifu/formats/usitosfenconverter.cpp
    src/kifu/formats/usitosfenconverter.h
    src/kifu/gameinfo.cpp
    src/kifu/gameinfo.h
    src/kifu/gameinfokeys.h
    src/kifu/josekibooksession.cpp
    src/kifu/josekibooksession.h
    src/kifu/josekibook.cpp
    src/kifu/josekibook.h
    src/kifu/josekiimport.cpp
    src/kifu/josekiimport.h
    src/kifu/josekilines.cpp
    src/kifu/josekilines.h
    src/kifu/josekimovepicker.cpp
    src/kifu/josekimovepicker.h
    src/kifu/josekitypes.h
    src/kifu/kifparsetypes.h
    src/kifu/kifubranchnode.cpp
    src/kifu/kifubranchnode.h
    src/kifu/kifubranchtree.cpp
    src/kifu/kifubranchtree.h
    src/kifu/kifubranchtreebuilder.cpp
    src/kifu/kifubranchtreebuilder.h
    src/kifu/kifuexportmetadata.h
    src/kifu/kifutextdecoder.cpp
    src/kifu/kifutextdecoder.h
    src/kifu/positioncollectionfile.cpp
    src/kifu/positioncollectionfile.h
    src/kifu/timecontroltext.cpp
    src/kifu/timecontroltext.h
    src/kifu/kifuformat.cpp
    src/kifu/kifuformat.h
    src/kifu/kifuloader.cpp
    src/kifu/kifuloader.h
    src/kifu/kifuloadparser.cpp
    src/kifu/kifuloadparser.h
    src/kifu/kifunavigationstate.cpp
    src/kifu/kifunavigationstate.h
    src/kifu/kifusavefile.cpp
    src/kifu/kifusavefile.h
    src/kifu/livegamesession.cpp
    src/kifu/livegamesession.h
    src/kifu/moverecord.h
    src/kifu/recordentry.h
    src/kifu/recordline.h
    src/kifu/shiftjiscodec.cpp
    src/kifu/shiftjiscodec.h
)

set(SRC_ANALYSIS
    src/analysis/analysisoptions.h
    src/analysis/analysissession.cpp
    src/analysis/analysissession.h
    src/analysis/engineanalysisrunner.cpp
    src/analysis/engineanalysisrunner.h
    src/analysis/kifuanalysissession.cpp
    src/analysis/kifuanalysissession.h
    src/analysis/matesearchposition.h
    src/analysis/tsumecandidatejob.cpp
    src/analysis/tsumecandidatejob.h
    src/analysis/tsumecollection.cpp
    src/analysis/tsumecollection.h
    src/analysis/tsumecollectionsession.cpp
    src/analysis/tsumecollectionsession.h
    src/analysis/tsumecollectionwriter.cpp
    src/analysis/tsumecollectionwriter.h
    src/analysis/tsumegamesession.cpp
    src/analysis/tsumegamesession.h
    src/analysis/tsumepositionanalyzer.cpp
    src/analysis/tsumepositionanalyzer.h
    src/analysis/tsumeprogressstore.cpp
    src/analysis/tsumeprogressstore.h
    src/analysis/tsumeshogicandidatescreener.cpp
    src/analysis/tsumeshogicandidatescreener.h
    src/analysis/tsumeshogiexportheaderbuilder.cpp
    src/analysis/tsumeshogiexportheaderbuilder.h
    src/analysis/tsumeshogigenerator.cpp
    src/analysis/tsumeshogigenerator.h
    src/analysis/tsumeshogipositiongenerator.cpp
    src/analysis/tsumeshogipositiongenerator.h
    src/analysis/tsumeshogiverifier.cpp
    src/analysis/tsumeshogiverifier.h
    src/analysis/tsumeverificationjob.cpp
    src/analysis/tsumeverificationjob.h
    src/analysis/tsumesolutionreplay.cpp
    src/analysis/tsumesolutionreplay.h
    src/analysis/tsumethreadbudget.h
    resources/tsumeshogi.qrc
)

set(SRC_ENGINE
    src/engine/enginesearchstatus.h
    src/engine/engineprocessmanager.cpp
    src/engine/engineprocessmanager.h
    src/engine/engineregistrationhandler.cpp
    src/engine/engineregistrationhandler.h
    src/engine/engineslots.cpp
    src/engine/engineslots.h
    src/engine/matchtimesource.cpp
    src/engine/matchtimesource.h
    src/engine/thinkinginfoformatter.cpp
    src/engine/thinkinginfoformatter.h
    src/engine/thinkinginforow.h
    src/engine/mateoracle.cpp
    src/engine/mateoracle.h
    src/engine/matequeryloop.cpp
    src/engine/matequeryloop.h
    src/engine/tsumeevaluation.h
    src/engine/tsumemateengine.cpp
    src/engine/tsumemateengine.h
    src/engine/usi.cpp
    src/engine/usi.h
    src/engine/usienginesession.cpp
    src/engine/usienginesession.h
    src/engine/usigotime.cpp
    src/engine/usigotime.h
    src/engine/usiinfo.h
    src/engine/usilineparser.cpp
    src/engine/usilineparser.h
    src/engine/usimatchhandler.cpp
    src/engine/usimatchhandler.h
    src/engine/usimessages.h
    src/engine/usiprotocolhandler.cpp
    src/engine/usiprotocolhandler.h
)

set(SRC_NETWORK
    src/network/csaclient.cpp
    src/network/csaclient.h
    src/network/csaclient_commands.cpp
    src/network/csaclient_parser.cpp
    src/network/csagamechannel.cpp
    src/network/csagamechannel.h
    src/network/csagamecoordinator.cpp
    src/network/csagamecoordinator.h
    src/network/csagamecoordinator_moves.cpp
    src/network/csagamecoordinator_setup.cpp
    src/network/csagameprogress.cpp
    src/network/csagameprogress.h
    src/network/csamoveconverter.cpp
    src/network/csamoveconverter.h
)

set(SRC_BOARD
    src/board/boardinputroute.h
    src/board/boardinteractioncontroller.cpp
    src/board/boardinteractioncontroller.h
    src/board/positioneditrules.cpp
    src/board/positioneditrules.h
    src/board/positioneditsession.cpp
    src/board/positioneditsession.h
)

set(SRC_UI
    src/ui/controllers/boardarrowactions.cpp
    src/ui/controllers/boardarrowactions.h
    src/ui/controllers/boardmovecontroller.cpp
    src/ui/controllers/boardmovecontroller.h
    src/ui/controllers/branchtreeeditcontroller.cpp
    src/ui/controllers/branchtreeeditcontroller.h
    src/ui/controllers/considerationuicontroller.cpp
    src/ui/controllers/considerationuicontroller.h
    src/ui/controllers/debugscreenshotshortcut.cpp
    src/ui/controllers/debugscreenshotshortcut.h
    src/ui/controllers/dialoglauncher.cpp
    src/ui/controllers/dialoglauncher.h
    src/ui/controllers/imageexportcontroller.cpp
    src/ui/controllers/imageexportcontroller.h
    src/ui/controllers/josekibookcontroller.cpp
    src/ui/controllers/josekibookcontroller.h
    src/ui/controllers/josekieditcontroller.cpp
    src/ui/controllers/josekieditcontroller.h
    src/ui/controllers/josekifilecontroller.cpp
    src/ui/controllers/josekifilecontroller.h
    src/ui/controllers/josekimergecontroller.cpp
    src/ui/controllers/josekimergecontroller.h
    src/ui/controllers/josekiregistrationcontroller.cpp
    src/ui/controllers/josekiregistrationcontroller.h
    src/ui/controllers/josekiwindowcontroller.cpp
    src/ui/controllers/josekiwindowcontroller.h
    src/ui/controllers/kifuanalysiscontroller.cpp
    src/ui/controllers/kifuanalysiscontroller.h
    src/ui/controllers/kifufilecontroller.cpp
    src/ui/controllers/kifufilecontroller.h
    src/ui/controllers/kifunavigationcontroller.cpp
    src/ui/controllers/kifunavigationcontroller.h
    src/ui/controllers/languagecontroller.cpp
    src/ui/controllers/languagecontroller.h
    src/ui/controllers/mainwindowappearancecontroller.cpp
    src/ui/controllers/mainwindowappearancecontroller.h
    src/ui/controllers/menuwindowcontroller.cpp
    src/ui/controllers/menuwindowcontroller.h
    src/ui/controllers/nyugyokudeclarationhandler.cpp
    src/ui/controllers/nyugyokudeclarationhandler.h
    src/ui/controllers/pvclickcontroller.cpp
    src/ui/controllers/pvclickcontroller.h
    src/ui/controllers/screenshotservice.cpp
    src/ui/controllers/screenshotservice.h
    src/ui/controllers/settingsresetcontroller.cpp
    src/ui/controllers/settingsresetcontroller.h
    src/ui/controllers/tsumesearchcontroller.cpp
    src/ui/controllers/tsumesearchcontroller.h
    src/ui/controllers/usicommandcontroller.cpp
    src/ui/controllers/usicommandcontroller.h
    src/ui/controllers/widgetimageexporter.cpp
    src/ui/controllers/widgetimageexporter.h
    src/ui/coordinators/commentcoordinator.cpp
    src/ui/coordinators/commentcoordinator.h
    src/ui/coordinators/csagamesession.cpp
    src/ui/coordinators/csagamesession.h
    src/ui/presenters/kifuanalysisrowpresenter.cpp
    src/ui/presenters/kifuanalysisrowpresenter.h
    src/ui/presenters/boardsyncpresenter.cpp
    src/ui/presenters/boardsyncpresenter.h
    src/ui/presenters/candidatearrowpresenter.cpp
    src/ui/presenters/candidatearrowpresenter.h
    src/ui/presenters/enginedisplaypresenter.cpp
    src/ui/presenters/enginedisplaypresenter.h
    src/ui/presenters/evaluationgraphpresenter.cpp
    src/ui/presenters/evaluationgraphpresenter.h
    src/ui/presenters/kifudisplaypresenter.cpp
    src/ui/presenters/kifudisplaypresenter.h
    src/ui/presenters/notificationpresenter.cpp
    src/ui/presenters/notificationpresenter.h
    src/ui/presenters/piecesoundplayer.cpp
    src/ui/presenters/piecesoundplayer.h
    src/ui/presenters/piecesoundprocessor.cpp
    src/ui/presenters/piecesoundprocessor.h
    src/ui/presenters/playerinfopresenter.cpp
    src/ui/presenters/playerinfopresenter.h
    src/ui/presenters/timedisplaypresenter.cpp
    src/ui/presenters/timedisplaypresenter.h
    src/ui/presenters/usimodelbinding.cpp
    src/ui/presenters/usimodelbinding.h
    src/ui/policy/humanturnpolicy.cpp
    src/ui/policy/humanturnpolicy.h
    src/ui/policy/uistatepolicymanager.cpp
    src/ui/policy/uistatepolicymanager.h
    src/ui/docks/docklayoutmanager.cpp
    src/ui/docks/docklayoutmanager.h
    src/ui/docks/dockregistry.cpp
    src/ui/docks/dockregistry.h
)

set(SRC_VIEWS
    src/views/boardchromewidget.cpp
    src/views/boardchromewidget.h
    src/views/boardgeometry.cpp
    src/views/boardgeometry.h
    src/views/playercard.cpp
    src/views/playercard.h
    src/views/boardstaticlayer.cpp
    src/views/boardstaticlayer.h
    src/views/boardsurfacepainter.cpp
    src/views/boardsurfacepainter.h
    src/views/piecepainter.cpp
    src/views/piecepainter.h
    src/views/shogiview.cpp
    src/views/shogiview.h
    src/views/shogiviewhighlighting.cpp
    src/views/shogiviewhighlighting.h
    src/views/shogiviewinteraction.cpp
    src/views/shogiviewinteraction.h
)

set(SRC_WIDGETS
    src/widgets/analysisresultspanel.cpp
    src/widgets/analysisresultspanel.h
    src/widgets/apptooltipfilter.cpp
    src/widgets/apptooltipfilter.h
    src/widgets/boardviewerwidget.cpp
    src/widgets/boardviewerwidget.h
    src/widgets/branchtreelayout.cpp
    src/widgets/branchtreelayout.h
    src/widgets/branchnodeitem.cpp
    src/widgets/branchnodeitem.h
    src/widgets/branchtreeview.cpp
    src/widgets/branchtreeview.h
    src/widgets/collapsiblegroupbox.cpp
    src/widgets/collapsiblegroupbox.h
    src/widgets/columnwidthmemory.cpp
    src/widgets/columnwidthmemory.h
    src/widgets/commenteditorpanel.cpp
    src/widgets/commenteditorpanel.h
    src/widgets/considerationpanel.cpp
    src/widgets/considerationpanel.h
    src/widgets/countspinbox.cpp
    src/widgets/countspinbox.h
    src/widgets/csalogpanel.cpp
    src/widgets/csalogpanel.h
    src/widgets/dialogfontscale.cpp
    src/widgets/dialogfontscale.h
    src/widgets/dialogutils.cpp
    src/widgets/dialogutils.h
    src/widgets/elidelabel.cpp
    src/widgets/elidelabel.h
    src/widgets/engineinfowidget.cpp
    src/widgets/engineinfowidget.h
    src/widgets/engineselector.cpp
    src/widgets/engineselector.h
    src/widgets/evaluationchartrange.cpp
    src/widgets/evaluationchartrange.h
    src/widgets/evaluationchartview.cpp
    src/widgets/evaluationchartview.h
    src/widgets/evaluationchartwidget.cpp
    src/widgets/evaluationchartwidget.h
    src/widgets/flowlayout.cpp
    src/widgets/flowlayout.h
    src/widgets/fontsizecontrol.cpp
    src/widgets/fontsizecontrol.h
    src/widgets/menuwindow.cpp
    src/widgets/menuwindow.h
    src/widgets/recentfilesmenu.cpp
    src/widgets/recentfilesmenu.h
    src/widgets/editactions.cpp
    src/widgets/editactions.h
    src/widgets/fontsizehelper.cpp
    src/widgets/fontsizehelper.h
    src/widgets/gameinfopane.cpp
    src/widgets/gameinfopane.h
    src/widgets/globaltooltip.cpp
    src/widgets/globaltooltip.h
    src/widgets/incrementaltableview.cpp
    src/widgets/incrementaltableview.h
    src/widgets/itemtextcutter.cpp
    src/widgets/itemtextcutter.h
    src/widgets/itemtextdelegate.cpp
    src/widgets/itemtextdelegate.h
    src/widgets/itemtextpainter.cpp
    src/widgets/itemtextpainter.h
    src/widgets/josekitableview.cpp
    src/widgets/josekitableview.h
    src/widgets/josekiwindow.cpp
    src/widgets/josekiwindow.h
    src/widgets/kifumovedelegate.h
    src/widgets/kifufilefilter.cpp
    src/widgets/kifufilefilter.h
    src/widgets/layoutboundarywidget.cpp
    src/widgets/layoutboundarywidget.h
    src/widgets/longlongspinbox.cpp
    src/widgets/longlongspinbox.h
    src/widgets/menubuttonwidget.cpp
    src/widgets/menubuttonwidget.h
    src/widgets/numericrightaligncommadelegate.h
    src/widgets/opaquebackground.cpp
    src/widgets/opaquebackground.h
    src/widgets/playersetupwidget.cpp
    src/widgets/playersetupwidget.h
    src/widgets/pvtableview.cpp
    src/widgets/pvtableview.h
    src/widgets/pvtextdelegate.h
    src/widgets/recordhighlightdelegate.cpp
    src/widgets/recordhighlightdelegate.h
    src/widgets/recordpane.cpp
    src/widgets/recordpane.h
    src/widgets/panelbutton.cpp
    src/widgets/panelbutton.h
    src/widgets/settingsformbinding.cpp
    src/widgets/settingsformbinding.h
    src/widgets/stepbuttons.cpp
    src/widgets/stepbuttons.h
    src/widgets/tablebuttondelegate.h
    src/widgets/tableheaderview.cpp
    src/widgets/tableheaderview.h
    src/widgets/thinkingpanel.cpp
    src/widgets/thinkingpanel.h
    src/widgets/thinkingtableview.cpp
    src/widgets/thinkingtableview.h
    src/widgets/timecontroleditor.cpp
    src/widgets/timecontroleditor.h
    src/widgets/tsumepositionpreview.cpp
    src/widgets/tsumepositionpreview.h
    src/widgets/unsavedchangesprompt.cpp
    src/widgets/unsavedchangesprompt.h
    src/widgets/logpanel.cpp
    src/widgets/logpanel.h
    src/widgets/usilogpanel.cpp
    src/widgets/usilogpanel.h
    src/widgets/toolbarwidth.h
)

set(SRC_DIALOGS
    src/dialogs/boardappearancepreview.cpp
    src/dialogs/boardappearancepreview.h
    src/dialogs/boardappearancedialog.cpp
    src/dialogs/boardappearancedialog.h
    src/dialogs/boardappearancesamples.cpp
    src/dialogs/boardappearancesamples.h
    src/dialogs/changeenginesettingsdialog.cpp
    src/dialogs/changeenginesettingsdialog.h
    src/dialogs/csagamedialog.cpp
    src/dialogs/csagamedialog.h
    src/dialogs/csawaitingdialog.cpp
    src/dialogs/csawaitingdialog.h
    src/dialogs/engineoptiondescriptions.cpp
    src/dialogs/engineoptiondescriptions.h
    src/dialogs/evaluationchartsettingsdialog.cpp
    src/dialogs/evaluationchartsettingsdialog.h
    src/dialogs/engineregistrationdialog.cpp
    src/dialogs/engineregistrationdialog.h
    src/dialogs/enginesettingsoptionhandler.cpp
    src/dialogs/enginesettingsoptionhandler.h
    src/dialogs/fontsettingsdialog.cpp
    src/dialogs/fontsettingsdialog.h
    src/dialogs/jishogiscoredialog.cpp
    src/dialogs/jishogiscoredialog.h
    src/dialogs/josekianalysisregisterdialog.cpp
    src/dialogs/josekianalysisregisterdialog.h
    src/dialogs/josekibookmergedialog.cpp
    src/dialogs/josekibookmergedialog.h
    src/dialogs/josekiexpanddialog.cpp
    src/dialogs/josekiexpanddialog.h
    src/dialogs/josekiimportdialog.cpp
    src/dialogs/josekiimportdialog.h
    src/dialogs/josekimergedialog.cpp
    src/dialogs/josekimergedialog.h
    src/dialogs/josekimovedialog.cpp
    src/dialogs/josekiplayoptionsdialog.cpp
    src/dialogs/josekiplayoptionsdialog.h
    src/dialogs/josekimovedialog.h
    src/dialogs/josekimoveinputwidget.cpp
    src/dialogs/josekimoveinputwidget.h
    src/dialogs/kifuanalysisdialog.cpp
    src/dialogs/kifuanalysisdialog.h
    src/dialogs/kifupastedialog.cpp
    src/dialogs/kifupastedialog.h
    src/dialogs/kifusavedialogs.cpp
    src/dialogs/kifusavedialogs.h
    src/dialogs/piecesoundsettingsdialog.cpp
    src/dialogs/piecesoundsettingsdialog.h
    src/dialogs/promotedialog.cpp
    src/dialogs/promotedialog.h
    src/dialogs/pvline.cpp
    src/dialogs/pvline.h
    src/dialogs/pvboarddialog.cpp
    src/dialogs/pvboarddialog.h
    src/dialogs/sfencollectiondialog.cpp
    src/dialogs/sfencollectiondialog.h
    src/dialogs/sfendiffutils.h
    src/dialogs/startgamedialog.cpp
    src/dialogs/startgamedialog.h
    src/dialogs/tsumecollectiondialog.cpp
    src/dialogs/tsumecollectiondialog.h
    src/dialogs/tsumeplaydialog.cpp
    src/dialogs/tsumeplaydialog.h
    src/dialogs/tsumeshogigeneratordialog.cpp
    src/dialogs/tsumeshogigeneratordialog.h
    src/dialogs/tsumeshogisearchdialog.cpp
    src/dialogs/tsumeshogisearchdialog.h
    src/dialogs/versiondialog.cpp
    src/dialogs/versiondialog.h
)

set(SRC_MODELS
    src/models/gameinfoeditmodel.cpp
    src/models/gameinfoeditmodel.h
    src/models/josekitablemodel.cpp
    src/models/josekitablemodel.h
    src/models/kifuanalysislistmodel.cpp
    src/models/kifuanalysislistmodel.h
    src/models/kifuanalysisrow.h
    src/models/notationtextcache.h
    src/models/kifubranchlistmodel.cpp
    src/models/kifubranchlistmodel.h
    src/models/kifurecordlistmodel.cpp
    src/models/kifurecordlistmodel.h
    src/models/shogienginethinkingmodel.cpp
    src/models/shogienginethinkingmodel.h
    src/models/scoretext.cpp
    src/models/scoretext.h
    src/models/shogiinforecord.cpp
    src/models/shogiinforecord.h
    src/models/evaluationseries.cpp
    src/models/evaluationseries.h
    src/models/usicommlogmodel.cpp
    src/models/usicommlogmodel.h
)

set(SRC_SERVICES
    src/services/analysissettings.cpp
    src/services/analysissettings.h
    src/services/applicationtranslations.cpp
    src/services/notationstyle.cpp
    src/services/notationstyle.h
    src/services/applicationtranslations.h
    src/services/apppaths.h
    src/services/appsettings.h
    src/services/docksettings.cpp
    src/services/docksettings.h
    src/services/enginedialogsettings.h
    src/services/enginelistsettings.cpp
    src/services/registeredengines.cpp
    src/services/registeredengines.h
    src/services/enginelistsettings.h
    src/services/engineoption.h
    src/services/engineprofilestore.cpp
    src/services/engineprofilestore.h
    src/services/enginesettingsconstants.h
    src/services/gamesettings.cpp
    src/services/gamesettings.h
    src/services/josekisettings.h
    src/services/networksettings.h
    src/services/piecesoundtone.h
    src/services/setting.h
    src/services/settingscommon.cpp
    src/services/settingscommon.h
    src/services/settingskeys.h
    src/services/settingsservice.h
    src/services/tsumeshogisettings.cpp
    src/services/tsumeshogisettings.h
)

set(SRC_COMMON
    src/common/appactivitystate.cpp
    src/common/appactivitystate.h
    src/style/appearancesettings.cpp
    src/style/appearancesettings.h
    src/style/appearancestate.h
    src/style/applicationfonts.cpp
    src/style/applicationfonts.h
    src/style/applicationrendering.cpp
    src/style/applicationrendering.h
    src/style/applicationstyle.cpp
    src/style/applicationstyle.h
    src/style/boardappearance.cpp
    src/style/boardappearance.h
    src/style/boardappearancecatalog.cpp
    src/style/boardappearancecatalog.h
    src/style/boardcolorpresets.cpp
    src/style/boardcolorpresets.h
    src/style/boardcolors.h
    src/common/boundedfile.h
    src/style/boardvisuals.h
    src/common/branchtreeitemroles.h
    src/style/buttonstyles.h
    src/common/errorbus.cpp
    src/common/errorbus.h
    src/core/jishogicalculator.cpp
    src/core/jishogicalculator.h
    src/common/recentfilelist.h
    src/kifu/kifupresentation.cpp
    src/kifu/kifupresentation.h
    src/common/logcategories.cpp
    src/common/logcategories.h
    src/style/movefont.h
    src/style/pieceimageprovider.cpp
    src/style/pieceimageprovider.h
    src/style/piecestylecatalog.cpp
    src/style/piecestylecatalog.h
    src/widgets/tablemodelutils.h
    src/style/tablestyles.h
    src/common/threadtypes.h
)

# CLI・自動化 API・MCP が共有する GUI 非依存の部品（引数の表・エラーの分類・変換・エンジンの実行器）
set(SRC_HEADLESS
    src/headless/automationendpoint.cpp
    src/headless/automationendpoint.h
    src/headless/automationmethodspecs.cpp
    src/headless/automationmethodspecs.h
    src/headless/enginecatalog.cpp
    src/headless/enginecatalog.h
    src/headless/errorkinds.cpp
    src/headless/errorkinds.h
    src/headless/kifuconversionservice.cpp
    src/headless/kifuconversionservice.h
    src/headless/matesearchrunner.cpp
    src/headless/matesearchrunner.h
    src/headless/outputpath.cpp
    src/headless/outputpath.h
    src/headless/recordjson.cpp
    src/headless/recordjson.h
    src/headless/paramspec.cpp
    src/headless/paramspec.h
    src/headless/sfenvalidationjson.cpp
    src/headless/sfenvalidationjson.h
    src/headless/tsumeverificationrunner.cpp
    src/headless/tsumeverificationrunner.h
)

# 自動化 API（--automation）
set(SRC_AUTOMATION
    src/automation/automationactionpolicy.cpp
    src/automation/automationactionpolicy.h
    src/automation/automationcommands.cpp
    src/automation/automationcommands.h
    src/automation/automationcommands_action.cpp
    src/automation/automationcommands_app.cpp
    src/automation/automationcommands_board.cpp
    src/automation/automationcommands_branch.cpp
    src/automation/automationcommands_clipboard.cpp
    src/automation/automationcommands_dialog.cpp
    src/automation/automationcommands_dock.cpp
    src/automation/automationcommands_kifu.cpp
    src/automation/automationcommands_menu.cpp
    src/automation/automationcommands_position.cpp
    src/automation/automationcommands_screenshot.cpp
    src/automation/automationcommands_ui.cpp
    src/automation/automationcommands_widget.cpp
    src/automation/automationcontext.h
    src/automation/automationdispatcher.cpp
    src/automation/automationdispatcher.h
    src/automation/automationparams.h
    src/automation/automationserver.cpp
    src/automation/automationserver.h
    src/automation/automationwidgets.cpp
    src/automation/automationwidgetadapters.cpp
    src/automation/automationwidgetadapters.h
    src/automation/automationinteractions.cpp
    src/automation/automationinteractions.h
    src/automation/automationinput.cpp
    src/automation/automationinput.h
    src/automation/automationwidgets.h
)

# shogiboardq-cli（GUI を起動しない実行ファイル。盤面画像 boardimagerenderer は CLI だけが使う）
set(SRC_CLI
    src/cli/boardimagerenderer.cpp
    src/cli/boardimagerenderer.h
    src/cli/cliargs.cpp
    src/cli/cliargs.h
    src/cli/clicommandspecs.cpp
    src/cli/clicommandspecs.h
    src/cli/clicommands.h
    src/cli/clicommands_basic.cpp
    src/cli/clicommands_engine.cpp
    src/cli/clicommands_tsume.cpp
    src/cli/clioutput.cpp
    src/cli/clioutput.h
    src/cli/clistopwatcher.cpp
    src/cli/clistopwatcher.h
    src/cli/main.cpp
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

# ==== 共通オブジェクトライブラリ ====
# GUI 本体・shogiboardq-cli・新しいテストが同じクラスを共有する。
# ShogiBoardQ はオブジェクトライブラリを直接リンクし（従来どおり全オブジェクトを含む）、
# CLI とテストは同じオブジェクトから作る静的ライブラリをリンクして参照した分だけ取り込む。
set(SBQ_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/automation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/headless
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kifu/formats
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/coordinators
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/docks
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/policy
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/presenters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/style
)

set(SBQ_LINK_LIBS
    hayanagi_tsume
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Sql
)

# Shift_JIS 変換: ICU なしの Qt では OS の変換機能を使う（Windows は Win32 API）
if(NOT WIN32)
    find_package(Iconv REQUIRED)
    list(APPEND SBQ_LINK_LIBS Iconv::Iconv)
endif()

# リリースビルドでは qDebug() 出力を無効化
set(SBQ_COMPILE_DEFINITIONS
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
    APP_VERSION="${APP_VERSION}"
)

qt_add_library(shogiboardq_objects OBJECT
    ${SRC_APP}
    ${SRC_CORE}
    ${SRC_GAME}
    ${SRC_KIFU}
    ${SRC_ANALYSIS}
    ${SRC_ENGINE}
    ${SRC_NETWORK}
    ${SRC_BOARD}
    ${SRC_UI}
    ${SRC_VIEWS}
    ${SRC_WIDGETS}
    ${SRC_DIALOGS}
    ${SRC_MODELS}
    ${SRC_SERVICES}
    ${SRC_COMMON}
    ${SRC_HEADLESS}
    ${SRC_AUTOMATION}
    ${UI_FILES}
)
target_include_directories(shogiboardq_objects PUBLIC ${SBQ_INCLUDE_DIRS})
target_link_libraries(shogiboardq_objects PUBLIC ${SBQ_LINK_LIBS})
target_compile_definitions(shogiboardq_objects PUBLIC ${SBQ_COMPILE_DEFINITIONS})

# CLI・テスト用の静的ライブラリ（同じオブジェクトファイルをアーカイブする）
add_library(shogiboardq_static STATIC $<TARGET_OBJECTS:shogiboardq_objects>)
target_include_directories(shogiboardq_static PUBLIC ${SBQ_INCLUDE_DIRS})
target_link_libraries(shogiboardq_static PUBLIC ${SBQ_LINK_LIBS})
target_compile_definitions(shogiboardq_static PUBLIC ${SBQ_COMPILE_DEFINITIONS})

# ==== Executable ====
qt_add_executable(ShogiBoardQ
    MANUAL_FINALIZATION
    src/app/main.cpp
    resources/shogiboardq.qrc
    ${APP_ICON_RESOURCE_WINDOWS}
    ${APP_ICON_MACOS}
)
target_link_libraries(ShogiBoardQ PRIVATE shogiboardq_objects)

# ==== CLI ====
# MCP サーバー（mcp/）から使う GUI 非依存の実行ファイル。offscreen の QApplication で動く
qt_add_executable(shogiboardq-cli
    ${SRC_CLI}
    resources/shogiboardq.qrc
)
target_link_libraries(shogiboardq-cli PRIVATE shogiboardq_static)

# .qm 生成（.ts → .qm コンパイルのみ、高速）
# qt_add_translation は lrelease のみ実行し、lupdate（ソーススキャン）をスキップする
# lupdate が必要な場合は update_translations ターゲットを使用する
qt_add_translation(QM_FILES ${TS_FILES})

# .qm ファイルをビルドターゲットとして追加
add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShogiBoardQ translations)

# lupdate（ソースから .ts ファイルを更新）は明示的に実行する
# 使用方法: cmake --build build --target update_translations
add_custom_target(update_translations
    COMMAND Qt6::lupdate ${CMAKE_SOURCE_DIR}/src -ts ${TS_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running lupdate to update .ts files from source..."
    VERBATIM
)

# .qm ファイルを実行可能ファイルと同じディレクトリにコピー
foreach(QM_FILE ${QM_FILES})
    add_custom_command(TARGET ShogiBoardQ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QM_FILE}
            $<TARGET_FILE_DIR:ShogiBoardQ>
        COMMENT "Copying ${QM_FILE} to output directory"
    )
endforeach()

# Bundle the standard Qt dialog translations as well (deploy scripts may omit them).
get_filename_component(SHOGIBOARDQ_QT_TRANSLATIONS_DIR "${QT6_INSTALL_TRANSLATIONS}"
    ABSOLUTE BASE_DIR "${QT6_INSTALL_PREFIX}")
set(SHOGIBOARDQ_QT_DIALOG_TRANSLATIONS)
foreach(language ja zh_CN zh_TW)
    set(qm "${SHOGIBOARDQ_QT_TRANSLATIONS_DIR}/qtbase_${language}.qm")
    if(EXISTS "${qm}")
        list(APPEND SHOGIBOARDQ_QT_DIALOG_TRANSLATIONS "${qm}")
    endif()
endforeach()
if(SHOGIBOARDQ_QT_DIALOG_TRANSLATIONS)
    qt_add_resources(ShogiBoardQ qt_dialog_translations
        PREFIX "/translations/qt"
        BASE "${SHOGIBOARDQ_QT_TRANSLATIONS_DIR}"
        FILES ${SHOGIBOARDQ_QT_DIALOG_TRANSLATIONS})
endif()

# ==== Bundle properties ====
get_target_property(SHOGIBOARDQ_QT_LIBRARY_TYPE Qt6::Core TYPE)
configure_file(cmake/qt-build.json.in ${CMAKE_CURRENT_BINARY_DIR}/qt-build.json @ONLY)

# 通常ビルドでもオフラインでライセンスを参照できる。配布版には scripts/qt_licenses.py
# が対応する Qt ソースのメタデータと第三者ライセンスを追加する。
add_custom_command(TARGET ShogiBoardQ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/licenses $<TARGET_FILE_DIR:ShogiBoardQ>/licenses
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/qt-build.json $<TARGET_FILE_DIR:ShogiBoardQ>/licenses/BUILD.json
    VERBATIM
)

set_target_properties(ShogiBoardQ PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.hnakada123.ShogiBoardQ
    MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION}
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

install(DIRECTORY resources/licenses/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/ShogiBoardQ)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qt-build.json
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/ShogiBoardQ RENAME BUILD.json)

# 実行ファイル
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS shogiboardq-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# デスクトップエントリ
install(FILES resources/platform/shogiboardq.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# Linux用アプリケーションアイコン（512x512、透明な余白を縮小）
install(FILES resources/icons/linux/shogiboardq.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps
)

# 翻訳ファイル（.qm）
install(FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==== Finalize ====
qt_finalize_executable(ShogiBoardQ)
