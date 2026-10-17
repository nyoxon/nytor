-- Show a quick summary of the current file in the prompt.

local lines = nyt.line_count()
local characters = 0
local words = 0
local longest_line = 1
local longest_length = 0

for y = 1, lines do
	local text = nyt.line(y) or ""
	local length = nyt.line_length(y)

	characters = characters + length
	for _ in text:gmatch("%S+") do
		words = words + 1
	end

	if length > longest_length then
		longest_line = y
		longest_length = length
	end
end

nyt.set_prompt(string.format(
	"%d lines, %d characters, %d words; longest: line %d (%d chars)",
	lines,
	characters,
	words,
	longest_line,
	longest_length
))
