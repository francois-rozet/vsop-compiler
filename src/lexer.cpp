#include "lexer.hpp"
#include "vsop.hpp"

using namespace std;

// Output formats
function<string(Cursor&, const Cursor&)> default_value =
	[](Cursor& x, const Cursor& y) {
		return x.read_to(y);
	};

function<string(Cursor&, const Cursor&)> empty_value =
	[](Cursor& x, const Cursor& y) {
		x = y;
		return "";
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
				if (equality('b')->f(x)) {
					c = '\b';
				} else if (equality('t')->f(x)) {
					c = '\t';
				} else if (equality('n')->f(x)) {
					c = '\n';
				} else if (equality('r')->f(x)) {
					c = '\r';
				} else if (lf->f(x)) {
					((space | tab)++)->f(x);
					continue;
				}
			} else {
				double_quote->f(x) or regular_char->f(x);
			}

			if (c >= 32 and c <= 126) {
				s += c;
			} else {
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
	function<string(Cursor&, const Cursor&)>& value = default_value;
};

vector<Rule> rules = {
	{"keyword", keyword},
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
	{"lower", lower},
	{"lower-equal", lower_equal},
	{"assign", assign},
	{"type-identifier", type_identifier},
	{"object-identifier", object_identifier},
	{"integer-literal", base10_literal, base10_value},
	{"integer-literal", base16_literal, base16_value},
	{"string-literal", string_literal, string_value},
	{"whitespace", whitespace, empty_value},
	{"comment", comment, empty_value}
};

vector<Rule> errors = {
	{"invalid integer literal", invalid_integer_literal}
};

Token Lexer::next() {
	if (this->end_of_file())
		return {"end-of-file"};

	string msg = to_string(x_->line() + 1) + ":" + to_string(x_->column() + 1) + ": lexical error: ";

	Cursor y(*x_), z(*x_);

	for (auto it = errors.begin(); it != errors.end(); it++, y = *x_)
		if (it->expr->f(y))
			throw LexicalError(msg + it->name + " " + it->value(*x_, y));

	auto rule = rules.begin();
	bool temp, error = false;

	for (auto it = rules.begin(); it != rules.end(); it++) {
		temp = not it->expr->f(y = *x_);

		if (z < y or (z == y and error)) {
			z = y;
			rule = it;
			error = temp;
		}
	}

	if (*x_ < z) {
		if (error) {
			*x_ = z;

			if (not z.end_of_file())
				msg = to_string(x_->line() + 1) + ":" + to_string(x_->column() + 1) + ": lexical error: ";

			++(*x_);

			if (rule->name == "comment" and z.end_of_file()) {
				throw LexicalError(msg + "unterminated comment");
			} else if (rule->name == "string-literal") {
				if (z.end_of_file()) {
					throw LexicalError(msg + "unterminated string literal");
				} else if (null->f(z) or lf->f(z)) {
					throw LexicalError(msg + "raw line feed or null character in string literal");
				} else if (backslash->f(z)) {
					throw LexicalError(msg + "invalid escape sequence in string literal");
				}
			} else {
				throw LexicalError(msg + "invalid character " + z.c());
			}
		} else {
			return {rule->name, rule->value(*x_, z)};
		}
	} else {
		++(*x_);
		throw LexicalError(msg + "invalid character " + z.c());
	}

	return {};
}
