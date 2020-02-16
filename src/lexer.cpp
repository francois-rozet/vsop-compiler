#include "lexer.hpp"
#include "vsop.hpp"

using namespace std;

// Output formats
function<string(Cursor&, const Cursor&)> empty_value =
	[](Cursor& x, const Cursor& y) {
		x = y;
		return "";
	};

function<string(Cursor&, const Cursor&)> read_value =
	[](Cursor& x, const Cursor& y) {
		return x.read_to(y);
	};

function<string(Cursor&, const Cursor&)> base10_value =
	[](Cursor& x, const Cursor& y) {
		int n = 0;

		for (char c; x < y; ++x) {
			c = x.c();
			n *= 10;
			n += c - '0';
		}

		return to_string(n);
	};

function<string(Cursor&, const Cursor&)> base16_value =
	[](Cursor& x, const Cursor& y) {
		int n = 0;

		hex_prefix->f(x);

		for (char c; x < y;) {
			c = x.c();
			n *= 16;

			if (digit->f(x))
				n += c - '0';
			else if (lowercase_letter->f(x))
				n += c - 'a' + 10;
			else if (uppercase_letter->f(x))
				n += c - 'A' + 10;
		}

		return to_string(n);
	};

function<string(Cursor&, const Cursor&)> string_value =
	[](Cursor& x, const Cursor& y) {
		string s = "";

		for (char c, b, d; x < y;) {
			c = x.c();

			if (backslash->f(x)) {
				if (equality('b')->f(x))
					c = '\b';
				else if (equality('t')->f(x))
					c = '\t';
				else if (equality('n')->f(x))
					c = '\n';
				else if (equality('r')->f(x))
					c = '\r';
				else if (double_quote->f(x)) {
					s += "\\x22";
					continue;
				} else if (backslash->f(x)) {
					s += "\\x5c";
					continue;
				} else if (lf->f(x) and ((space | tab)++)->f(x))
					continue;
			} else {
				double_quote->f(x) or regular_char->f(x);
			}

			if (c >= 32 and c <= 126)
				s += c;
			else {
				b = c % 16;
				d = c / 16;

				s += "\\x";
				s += d < 10 ? d + '0' : d + 'a' - 10;
				s += b < 10 ? b + '0' : b + 'a' - 10;
			}
		}

		return s;
	};

// Lexical analysis
struct Rule {
	string name;
	expr_ptr expr;
	function<string(Cursor&, const Cursor&)>& value = empty_value;
};

vector<Rule> forbidden = {
	{"invalid integer literal", (digit + base_identifier) - integer_literal, read_value}
};

vector<Rule> rules = {
	{"integer-literal", base16_literal, base16_value},
	{"integer-literal", base10_literal, base10_value},
	{"string-literal", string_literal, string_value},
	{"type-identifier", type_identifier, read_value},
	{"object-identifier", object_identifier, read_value},
	{"whitespace", whitespace},
	{"comment", comment},
	{"lower-equal", lower_equal},
	{"assign", assign},
	{"lbrace", lbrace},
	{"rbrace", rbrace},
	{"lpar", lpar},
	{"rpar", rpar},
	{"colon", colon},
	{"semicolon", semicolon},
	{"comma", comma},
	{"plus", plus_sign},
	{"minus", minus_sign},
	{"times", asterisk},
	{"div", slash},
	{"pow", pow},
	{"dot", dot},
	{"equal", equal_sign},
	{"lower", lower}
};

Token Lexer::next() {
	if (this->end_of_file())
		return {"end-of-file"};

	string msg = to_string(x_->line() + 1) + ":" + to_string(x_->column() + 1) + ": lexical error: ";

	// Forbbiden
	Cursor y(*x_);

	for (auto it = forbidden.begin(); it != forbidden.end(); it++, y = *x_)
		if (it->expr->f(y))
			throw LexicalError(msg + it->name + " " + it->value(*x_, y));

	// Rules
	auto deepest = rules.begin();
	Cursor z(*x_);

	for (auto it = rules.begin(); it != rules.end(); it++, y = *x_)
		if (it->expr->f(y) and not (y < z)) { // First accepted rule
			if (it->name == "object-identifier") {
				string value = it->value(*x_, y);

				if (keywords.find(value) == keywords.end())
					return {it->name, value};
				else
					return {value};
			} else
				return {it->name, it->value(*x_, y)};
		} else if (z < y) { // Deepest non-accepting rule
			deepest = it;
			z = y;
		}

	// No accepting rule
	*x_ = z;
	if (not z.end_of_file())
		msg = to_string(x_->line() + 1) + ":" + to_string(x_->column() + 1) + ": lexical error: ";
	++(*x_);

	if (deepest->name == "comment" and z.end_of_file())
		throw LexicalError(msg + "unterminated comment");
	else if (deepest->name == "string-literal" and z.end_of_file())
		throw LexicalError(msg + "unterminated string literal");
	else if (deepest->name == "string-literal" and null->f(z))
		throw LexicalError(msg + "null character in string literal");
	else if (deepest->name == "string-literal" and lf->f(z))
		throw LexicalError(msg + "raw line feed in string literal");
	else if (deepest->name == "string-literal" and backslash->f(z))
		throw LexicalError(msg + "invalid escape sequence in string literal");
	else
		throw LexicalError(msg + "invalid character " + z.c());

	return {};
}
