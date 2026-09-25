"""ShogiBoardQ MCP server package.

The server speaks the Model Context Protocol over stdio and forwards tool
calls either to the ``shogiboardq-cli`` executable (no GUI required) or to a
running ``ShogiBoardQ --automation`` instance through its local socket.
"""

__version__ = "2026.9.25"
