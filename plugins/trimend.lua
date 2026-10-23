-- remove trailing spaces and tabs
-- from every line in the current file.
-- the cursor is left at the end of the final line.

local lines = nyt.line_count(nyt.current_file())
local removed = 0

for y = 1, lines do
	local text = nyt.line(y) or ""
	local trailing = text:match("[ \t]+$")

	if trailing then
		-- spaces and tabs are single-byte characters.
		local count = #trailing
		local length = nyt.line_length(y)

		-- delete_right removes from the cursor
		-- through the end of this line.
		nyt.set_cursor(length - count + 1, y)
		nyt.delete_right()

		removed = removed + count
	end
end

nyt.set_cursor(nyt.line_length(lines) + 1, lines)

local x, y = nyt.cursor()

nyt.set_prompt(string.format(
	"Trimmed %d trailing spaces/tabs; cursor at %d:%d",
	removed,
	y,
	x
))