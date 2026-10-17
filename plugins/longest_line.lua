-- move the cursor to the end of the first longest line in the current file.
-- line and column numbers passed to the API start at 1.

local line_count = nyt.line_count()

local max_line = 1
local max_line_length = nyt.line_length(1)

if line_count == 1 and max_line_length == 0 then
	nyt.set_prompt("Empty file")
	return
end

for y = 2, line_count do
	local length = nyt.line_length(y)
	
	if length > max_line_length then
		max_line = y
		max_line_length = length
	end
end

-- the end position is one column after the final character.
nyt.set_cursor(max_line_length + 1, max_line)
nyt.set_prompt(string.format(
	"Longest line: line %d (%d characters)",
	max_line,
	max_line_length
))