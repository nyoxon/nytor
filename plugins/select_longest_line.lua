-- Select the first longest line in the current file.

local lines = nyt.line_count()
local longest_line = 1
local longest_length = nyt.line_length(1)

for y = 2, lines do
	local length = nyt.line_length(y)
	if length > longest_length then
		longest_line = y
		longest_length = length
	end
end

nyt.set_cursor(1, longest_line)
local result = nyt.select_line()

if result == 0 then
	nyt.set_prompt(string.format(
		"Selected longest line: %d (%d characters)",
		longest_line,
		longest_length
	))
else
	nyt.set_prompt("Could not select the longest line")
end
