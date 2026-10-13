-- Lua lexer showcase
-- Keywords, constants, names, functions, operators, punctuation, numbers,
-- quoted strings, continued strings, and short/long comments are represented.

global shared_value = nil -- Lua 5.5 keyword and constant
local enabled, disabled = true, false
local _private_name = 42

-- Exercise the reserved words in Lua syntax so they appear as code tokens.
local logic = not false and true or false
if logic then
	logic = true
elseif false then
	logic = false
else
	logic = nil
end

do
	local count = 0
	while count < 1 do
		count = count + 1
		break
	end
end

for index = 1, 1 do
	logic = index == 1
end
for key, value in pairs({ sample = true }) do
	logic = value
	break
end

repeat
	logic = false
until logic == false

-- Function names followed by '(' are highlighted as functions.
local function greet(name)
	print("hello, " .. name)
	object:method_call(name)
	return name
end

local function goto_demo()
	goto finished
	::finished::
end

-- Numeric forms covered by the lexer.
local whole, underscored = 123, 1_000_000
local decimal, leading_decimal = 3.1415, .25
local scientific, signed_exponent = 6.02e23, 1.5E-4
local hexadecimal = 0xCAFE

-- Quoted strings, escapes, and a short string continued with a backslash.
local single = 'single quoted string'
local escaped = "quote: \" and slash: \\"
local continued = "this string continues \
on the next source line"

-- Long bracket strings support matching levels of '=' signs.
local long_string = [[
This is a long string.
It can span multiple lines and contain "quotes".
]]

local equal_level_string = [=[
This delimiter uses one '=' on both sides.
The text ]==] does not close it.
]=]

-- Long comments use the same bracket delimiter rules as long strings.
--[[
This is a multiline comment.
The lexer should keep highlighting until the matching close.
]]

--[==[
This comment uses two '=' signs.
The shorter close ]=] does not end it.
]==]

-- Operators and punctuation: + - * / // % ^ # & ~ | << >>
-- == ~= < <= > >= = .. ... : :: ( ) [ ] { } , . ;
local result = (whole + 2) * 3 // 2 % 5 ^ 2
local bits = (result << 1) | (result >> 1) & ~0x0F
local same, different = result == 3, result ~= 3
local ordering = result < 10 and result <= 10 and result > 0 and result >= 0
local joined = "Lua" .. " lexer" .. " showcase"
local table_value = { first = 1, second = 2; [3] = "three" }
local property = table_value.first
local method = table_value:second()
::continue_here::

-- Identifier forms, attributes, and function-call lookahead.
local ordinary_name = greet
local called_name = greet("Lua")
local field_name = table_value.second

return {
	enabled = enabled,
	disabled = disabled,
	private = _private_name,
	property = property,
	method = method,
	goto_demo = goto_demo,
	joined = joined,
}