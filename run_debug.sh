#!/bin/bash
cd /home/nakada/GitHub/ShogiBoardQ
export QT_LOGGING_RULES="shogi.navigation.debug=true;shogi.ui.debug=true"
exec ./build/ShogiBoardQ 2>/home/nakada/GitHub/ShogiBoardQ/debug.log
