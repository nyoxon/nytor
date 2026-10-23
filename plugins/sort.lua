if not nyt.has_selection() then
	return
end

local _, ay, _, by = nyt.get_selection_limits()
local by_length = nyt.line_length(by)

nyt.clear_selection()
nyt.select_range(1, ay, by_length + 1, by)

local content = nyt.get_selection()

nyt.delete()

local lines = {}

for line in (content .. "\n"):gmatch("(.-)\n") do
	table.insert(lines, line)
end

table.sort(lines)

content = table.concat(lines, "\n")

nyt.insert(content)