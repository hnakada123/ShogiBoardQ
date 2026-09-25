"""Error types shared by the tool handlers."""

from __future__ import annotations


class ToolError(Exception):
    """An error that is reported to the MCP client as ``isError: true``.

    ``code`` is a short machine readable slug, ``message`` explains what went
    wrong and how to fix it.
    """

    def __init__(self, code: str, message: str, data: dict | None = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data or {}

    def __str__(self) -> str:  # pragma: no cover - trivial
        return f"{self.code}: {self.message}"
