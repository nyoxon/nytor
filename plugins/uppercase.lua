local selected = nyt.get_selection()

if selected and #selected > 0 then
	nyt.delete()
	nyt.insert(string.upper(selected))
end