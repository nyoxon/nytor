-- Read-only API smoke test. Run with:
--   lua plugins/test_api.lua
local x, y = nyt.cursor()
local count = nyt.line_count()
local current = nyt.line(y) or ""
local input = nyt.prompt() or ""

nyt.set_prompt(string.format(
	"Lua API: cursor %d:%d, %d lines, line length %d, prompt %d chars",
	x, y, count, #current, #input
))
