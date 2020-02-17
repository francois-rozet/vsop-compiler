#include "bnf.hpp"

#include <set>

// Special characters
static expr_ptr all = special([](Cursor& x) { ++x; return true; });

static expr_ptr null = equality('\0');
static expr_ptr backspace = equality('\b');
static expr_ptr tab = equality('\t');
static expr_ptr lf = equality('\n');
static expr_ptr ff = equality('\f');
static expr_ptr cr = equality('\r');
static expr_ptr double_quote = equality('\"');
static expr_ptr backslash = equality('\\');

static expr_ptr space = equality(' ');
static expr_ptr underscore = equality('_');

// Letters
static expr_ptr lowercase_letter = range('a', 'z');
static expr_ptr uppercase_letter = range('A', 'Z');
static expr_ptr letter = lowercase_letter | uppercase_letter;

// Digits
static expr_ptr digit = range('0', '9');
static expr_ptr hex_digit = digit | range('a', 'f') | range('A', 'F');
static expr_ptr hex_prefix = equality("0x");

// Operators
static expr_ptr lbrace = equality('{');
static expr_ptr rbrace = equality('}');
static expr_ptr lpar = equality('(');
static expr_ptr rpar = equality(')');
static expr_ptr colon = equality(':');
static expr_ptr semicolon = equality(';');
static expr_ptr comma = equality(',');
static expr_ptr plus_sign = equality('+');
static expr_ptr minus_sign = equality('-');
static expr_ptr asterisk = equality('*');
static expr_ptr slash = equality('/');
static expr_ptr pow = equality('^');
static expr_ptr dot = equality('.');
static expr_ptr equal_sign = equality('=');
static expr_ptr lower = equality('<');
static expr_ptr lower_equal = equality("<=");
static expr_ptr assign = equality("<-");

// Identifiers
static expr_ptr base_identifier = (letter | digit | underscore)++;
static expr_ptr type_identifier = uppercase_letter + base_identifier;
static expr_ptr object_identifier = lowercase_letter + base_identifier;

// Keywords
static std::set<std::string> keywords = {
	"and", "bool", "class", "do",
	"else", "extends", "false", "if",
	"in", "int32", "isnull", "let",
	"new", "not", "string", "then",
	"true", "unit", "while"
};

// Interger literals
static expr_ptr base10_literal = digit + digit++;
static expr_ptr base16_literal = hex_prefix + hex_digit + hex_digit++;
static expr_ptr integer_literal = base10_literal | base16_literal;

// String literals
static expr_ptr regular_char = all - null - lf - ff - double_quote - backslash;
static expr_ptr escape_char =
	equality('b') | equality('t') | equality('n') | equality('r') |
	double_quote | backslash |
	(equality('x') + hex_digit + hex_digit) |
	(lf + (space | tab)++);

static expr_ptr string_literal = double_quote + (regular_char | (backslash + escape_char))++ + double_quote;

// Whitespaces
static expr_ptr blankspace = space | tab | lf | cr;
static expr_ptr whitespace = blankspace + blankspace++;

// Comments
static expr_ptr single_line_comment = slash + slash + (all - null - ff - lf)++ + (lf | ff);

static expr_ptr multiline_char = all - null - ff - lpar - asterisk;
static expr_ptr multiline_tail = special(
	[](Cursor& x) {
		while (true) {
			if (multiline_char->f(x))
				continue;

			if (asterisk->f(x)) {
				if (rpar->f(x))
					return true;
			} else if (lpar->f(x)) {
				if (asterisk->f(x))
					if (not multiline_tail->f(x))
						return false;
			} else
				return false;
		}
	}
);
static expr_ptr multiline_comment = lpar + asterisk + multiline_tail;

static expr_ptr comment = single_line_comment | multiline_comment;
