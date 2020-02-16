#include "bnf.hpp"

// Special characters
expr_ptr all = special([](Cursor& x) { ++x; return true; });

expr_ptr null = equality('\0');
expr_ptr backspace = equality('\b');
expr_ptr tab = equality('\t');
expr_ptr lf = equality('\n');
expr_ptr ff = equality('\f');
expr_ptr cr = equality('\r');
expr_ptr double_quote = equality('\"');
expr_ptr backslash = equality('\\');

expr_ptr space = equality(' ');
expr_ptr underscore = equality('_');

// Letters
expr_ptr lowercase_letter = range('a', 'z');
expr_ptr uppercase_letter = range('A', 'Z');
expr_ptr letter = lowercase_letter | uppercase_letter;

// Digits
expr_ptr digit = range('0', '9');
expr_ptr hex_digit = digit | range('a', 'f') | range('A', 'F');
expr_ptr hex_prefix = equality("0x");

// Keywords
expr_ptr keyword = equality("and") |
	equality("bool") | equality("class") | equality("do") |
	equality("else") | equality("extends") | equality("false") |
	equality("if") | equality("in") | equality("int32") |
	equality("isnull") | equality("let") | equality("new") |
	equality("not") | equality("string") | equality("then") |
	equality("true") | equality("unit") | equality("while");

// Operators
expr_ptr lbrace = equality('{');
expr_ptr rbrace = equality('}');
expr_ptr lpar = equality('(');
expr_ptr rpar = equality(')');
expr_ptr colon = equality(':');
expr_ptr semicolon = equality(';');
expr_ptr comma = equality(',');
expr_ptr plus_sign = equality('+');
expr_ptr minus_sign = equality('-');
expr_ptr asterisk = equality('*');
expr_ptr slash = equality('/');
expr_ptr pow = equality('^');
expr_ptr dot = equality('.');
expr_ptr equal_sign = equality('=');
expr_ptr lower = equality('<');
expr_ptr lower_equal = equality("<=");
expr_ptr assign = equality("<-");

// Identifiers
expr_ptr base_identifier = (letter | digit | underscore)++;
expr_ptr type_identifier = uppercase_letter + base_identifier;
expr_ptr object_identifier = (lowercase_letter + base_identifier) - keyword;

// Interger literals
expr_ptr base10_literal = digit + digit++;
expr_ptr base16_literal = hex_prefix + hex_digit + hex_digit++;
expr_ptr integer_literal = base10_literal | base16_literal;
expr_ptr invalid_integer_literal = (digit + base_identifier) - integer_literal;

// String literals
expr_ptr regular_char = all - null - lf - ff - double_quote - backslash;
expr_ptr escape_char =
	equality('b') | equality('t') | equality('n') | equality('r') |
	double_quote | backslash |
	(equality('x') + hex_digit + hex_digit) |
	(lf + (space | tab)++);

expr_ptr string_literal = double_quote + (regular_char | (backslash + escape_char))++ + double_quote;

// Whitespaces
expr_ptr whitespace = (space | tab | lf | cr)++;

// Comments
expr_ptr tail_char = all - null - ff - lpar - asterisk;
std::function<bool(Cursor&)> tail = [](Cursor& x) {
	while (true) {
		if (tail_char->f(x))
			continue;

		if (asterisk->f(x)) {
			if (rpar->f(x))
				return true;
		} else if (lpar->f(x)) {
			if (asterisk->f(x) and not tail(x))
				return false;
		} else
			return false;
	}
};

expr_ptr single_line_comment = slash + slash + (all - null - ff - lf)++ + (lf | ff);
expr_ptr multiple_line_comment = lpar + asterisk + special(tail);
expr_ptr comment = single_line_comment | multiple_line_comment;
