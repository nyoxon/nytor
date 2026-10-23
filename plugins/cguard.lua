local current = nyt.current_file()

local filename = nyt.filename(current)

if not filename or #filename == 0 then
	return
end

local line_count = nyt.line_count(current)

if line_count == 0 then
	return
end

local guard = filename:upper():gsub("[^%w]", "_")
local length = nyt.line_length(line_count)

nyt.set_cursor(length + 1, line_count)
nyt.insert_newline(true)
nyt.insert("#endif")

nyt.set_cursor(1, 1)
nyt.insert_newline(true)
nyt.set_cursor(1, 1)
nyt.insert("#ifndef " .. guard)
nyt.insert_newline(true)
nyt.insert("#define " .. guard)
nyt.clear_selection()
nyt.insert_newline(true)

nyt.set_all_lines_dirty(current)