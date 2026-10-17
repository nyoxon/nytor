-- Exercises range selection and clipboard copy without editing the buffer.
-- Run with: lua plugins/test_selection.lua
local columns, rows = nyt.terminal_size()
local line_count = nyt.line_count()

if line_count > 0 then
	-- Selecting a zero-width range works even for an empty first line.
	local result = nyt.select_range(1, 1, 1, 1)
	if result == 0 then
		nyt.copy()
		nyt.clear_selection()
	end
end

nyt.set_prompt(string.format("Lua selection test: terminal %dx%d", columns, rows))
