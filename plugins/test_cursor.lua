-- Moves to the first character of the last line and reports the new position.
-- Run with: lua plugins/test_cursor.lua
local count = nyt.line_count()
if count > 0 then
	nyt.set_cursor(1, count)
end

local x, y = nyt.cursor()
nyt.set_prompt(string.format("Lua cursor test: now at %d:%d", x, y))
