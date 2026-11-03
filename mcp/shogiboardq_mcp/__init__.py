"""ShogiBoardQ MCP server package.

The server speaks the Model Context Protocol over stdio and forwards tool
calls either to the ``shogiboardq-cli`` executable (no GUI required) or to a
running ``ShogiBoardQ --automation`` instance through its local socket.
"""

# MCP サーバーの版（ここだけに書く。pyproject.toml は dynamic でここから読み、initialize の応答の serverInfo にも使う）
__version__ = "2026.9.25"
