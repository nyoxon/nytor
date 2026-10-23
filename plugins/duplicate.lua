local selection = nyt.has_selection()

local content = ""
local ignore_auto_indent = true

if selection then
	local _, ay, _, by = nyt.get_selection_limits()
	local by_length = nyt.line_length(by)

	nyt.clear_selection()
	nyt.select_range(1, ay, by_length + 1, by)

	content = nyt.get_selection()
	nyt.clear_selection()

	nyt.set_cursor(by_length + 1, by)
else
	local _, y = nyt.cursor()
	local line_length = nyt.line_length(y)

	content = nyt.line(y)
	nyt.set_cursor(line_length + 1, y)
end

nyt.insert_newline(ignore_auto_indent)
nyt.insert(content)
nyt.clear_selection()