#include "lexer.hpp"
#include "regex.hpp"
#include "define.hpp"

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

#define expr_ptr expr_ptr<Cursor>
#define special special<Cursor>
#define equality equality<Cursor>
#define range range<Cursor>

using namespace std;

/* (Regular) expressions */

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

expr_ptr asterisk = equality('*');
expr_ptr slash = equality('/');

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
expr_ptr times_sign = equality('*');
expr_ptr div_sign = equality('/');
expr_ptr pow_sign = equality('^');
expr_ptr dot = equality('.');
expr_ptr equal_sign = equality('=');
expr_ptr lower = equality('<');
expr_ptr lower_equal = equality("<=");
expr_ptr assign = equality("<-");

// Letters

expr_ptr lowercase_letter = range('a', 'z');
expr_ptr uppercase_letter = range('A', 'Z');
expr_ptr letter = lowercase_letter | uppercase_letter;

// Digits

expr_ptr digit = range('0', '9');
expr_ptr hex_digit = digit | range('a', 'f') | range('A', 'F');
expr_ptr hex_prefix = equality("0x");

// Identifiers

expr_ptr base_identifier = letter | digit | underscore;
expr_ptr type_identifier = uppercase_letter + base_identifier++;
expr_ptr object_identifier = lowercase_letter + base_identifier++;

// Interger literals

expr_ptr base10_literal = digit + digit++;
expr_ptr base16_literal = hex_prefix + hex_digit + hex_digit++;
expr_ptr integer_literal = base10_literal | base16_literal;

// String literals

expr_ptr regular_char = all - null - lf - ff - double_quote - backslash;
expr_ptr escape_char =
	equality('b') | equality('t') | equality('n') | equality('r') |
	double_quote | backslash |
	(equality('x') + hex_digit + hex_digit) |
	(lf + (space | tab)++);

expr_ptr string_literal = double_quote + (regular_char | (backslash + escape_char))++ + double_quote;

// Whitespaces

expr_ptr blankspace = space | tab | lf | cr;
expr_ptr whitespace = blankspace + blankspace++;

// Comments

expr_ptr single_line_comment = slash + slash + (all - null - ff - lf)++ + (lf | ff);

expr_ptr multiline_char = all - null - ff - lpar - asterisk;
expr_ptr multiline_tail = special(
	[](Cursor& x) {
		while (true) {
			if ((*multiline_char)(x))
				continue;

			if ((*asterisk)(x)) {
				if ((*rpar)(x))
					return true;
			} else if ((*lpar)(x)) {
				if ((*asterisk)(x))
					if (not (*multiline_tail)(x))
						return false;
			} else
				return false;
		}
	}
);
expr_ptr multiline_comment = lpar + asterisk + multiline_tail;

expr_ptr comment = single_line_comment | multiline_comment;

/* Parsing functions */

void empty_value(Token& t, Cursor& x, const Cursor& y) {
	x = y;
}

void read_value(Token& t, Cursor& x, const Cursor& y) {
	t.value.str = read(x, y);
}

void base10_value(Token& t, Cursor& x, const Cursor& y) {
	string s = read(x, y);
	t.value.num = strtol(&s[0], NULL, 10);
}

void base16_value(Token& t, Cursor& x, const Cursor& y) {
	string s = read(x, y);
	t.value.num = strtol(&s[0], NULL, 16);
}

void string_value(Token& t, Cursor& x, const Cursor& y) {
	string s = "";

	for (char c, b, d; x < y;) {
		c = *x;

		if ((*backslash)(x)) {
			switch (*x) {
				case 'b': c = '\b';
					break;
				case 't': c = '\t';
					break;
				case 'n': c = '\n';
					break;
				case 'r': c = '\r';
					break;
				case '\"': s += "\\x22";
					++x;
					continue;
				case '\\': s += "\\x5c";
					++x;
					continue;
				case '\n':
					(*lf)(x) and (*((space | tab)++))(x);
					continue;
			}
		} else {
			(*double_quote)(x) or (*regular_char)(x);
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

	t.value.str = s;
}

/* Rules */

struct Rule {
	token_type type;
	expr_ptr expr;
	function<void(Token&, Cursor&, const Cursor&)> value = empty_value;
};

vector<Rule> rules = {
	{T_WHITESPACE, whitespace},
	{T_COMMENT, comment, empty_value},
	{T_INTEGER_LITERAL, base16_literal, base16_value},
	{T_INTEGER_LITERAL, base10_literal, base10_value},
	{T_STRING_LITERAL, string_literal, string_value},
	{T_TYPE_IDENTIFIER, type_identifier, read_value},
	{T_OBJECT_IDENTIFIER, object_identifier, read_value},
	{T_LOWER_EQUAL, lower_equal},
	{T_ASSIGN, assign},
	{T_LBRACE, lbrace},
	{T_RBRACE, rbrace},
	{T_LPAR, lpar},
	{T_RPAR, rpar},
	{T_COLON, colon},
	{T_SEMICOLON, semicolon},
	{T_COMMA, comma},
	{T_PLUS, plus_sign},
	{T_MINUS, minus_sign},
	{T_TIMES, times_sign},
	{T_DIV, div_sign},
	{T_POW, pow_sign},
	{T_DOT, dot},
	{T_EQUAL, equal_sign},
	{T_LOWER, lower}
};

/* Keywords */

unordered_map<string, token_type> keywords = {
	{"and", T_AND},
	{"bool", T_BOOL},
	{"class", T_CLASS},
	{"do", T_DO},
	{"else", T_ELSE},
	{"extends", T_EXTENDS},
	{"false", T_FALSE},
	{"if", T_IF},
	{"in", T_IN},
	{"int32", T_INT32},
	{"isnull", T_ISNULL},
	{"let", T_LET},
	{"new", T_NEW},
	{"not", T_NOT},
	{"string", T_STRING},
	{"then", T_THEN},
	{"true", T_TRUE},
	{"unit", T_UNIT},
	{"while", T_WHILE}
};

/* (Standard) output */

unordered_map<token_type, string> output = {
	{T_LBRACE, "lbrace"},
	{T_RBRACE, "rbrace"},
	{T_LPAR, "lpar"},
	{T_RPAR, "rpar"},
	{T_COLON, "colon"},
	{T_SEMICOLON, "semicolon"},
	{T_COMMA, "comma"},
	{T_PLUS, "plus"},
	{T_MINUS, "minus"},
	{T_TIMES, "times"},
	{T_DIV, "div"},
	{T_POW, "pow"},
	{T_DOT, "dot"},
	{T_EQUAL, "equal"},
	{T_LOWER, "lower"},
	{T_LOWER_EQUAL, "lower-equal"},
	{T_ASSIGN, "assign"},
	{T_INTEGER_LITERAL, "integer-literal"},
	{T_STRING_LITERAL, "string-literal"},
	{T_TYPE_IDENTIFIER, "type-identifier"},
	{T_OBJECT_IDENTIFIER, "object-identifier"},
	{T_AND, "and"},
	{T_BOOL, "bool"},
	{T_CLASS, "class"},
	{T_DO, "do"},
	{T_ELSE, "else"},
	{T_EXTENDS, "extends"},
	{T_FALSE, "false"},
	{T_IF, "if"},
	{T_IN, "in"},
	{T_INT32, "int32"},
	{T_ISNULL, "isnull"},
	{T_LET, "let"},
	{T_NEW, "new"},
	{T_NOT, "not"},
	{T_STRING, "string"},
	{T_THEN, "then"},
	{T_TRUE, "true"},
	{T_UNIT, "unit"},
	{T_WHILE, "while"}
};

/* Lexer */

Token Lexer::next(bool print = false) {
	if (not *this)
		return {T_EOF};

	Cursor y(x_), z(x_);

	// Rules
	string line = to_string(this->line()), column = to_string(this->column());
	string msg = line + ":" + column + ": lexical error: ";

	auto deepest = rules.begin();

	for (auto it = rules.begin(); it != rules.end(); it++, y = x_) {
		if ((*(it->expr))(y) and z <= y) { // First accepted rule			
			Token t = {it->type};

			switch (t.type) {
				case T_WHITESPACE:
					it->value(t, x_, y);
					return this->next(print);
				case T_COMMENT:
					it->value(t, x_, y);
					return this->next(print);
				case T_OBJECT_IDENTIFIER:
					it->value(t, x_, y);
					{
					auto itk = keywords.find(t.value.str);
					if (itk != keywords.end())
						t = {itk->second};
					}
					break;
				case T_INTEGER_LITERAL:
					if ((*(base_identifier + base_identifier++))(y)) {
						deepest = it;
						z = y;
						continue;
					}
				default:
					it->value(t, x_, y);
			}

			if (print) {
				cout << line << ',' << column << ',' << output[t.type];

				switch (t.type) {
					case T_INTEGER_LITERAL:
						cout << ',' << t.value.num;
						break;
					case T_STRING_LITERAL:
					case T_TYPE_IDENTIFIER:
					case T_OBJECT_IDENTIFIER:
						cout << ',' << t.value.str;
				}

				cout << endl;
			}

			return t;
		} else if (z < y) { // Deepest non-accepting rule
			deepest = it;
			z = y;
		}
	}

	// Error
	if (deepest->type != T_INTEGER_LITERAL) {
		x_ = z;
		if (z)
			msg = to_string(this->line()) + ":" + to_string(this->column()) + ": lexical error: ";
		++x_;
	}

	switch (deepest->type) {
		case T_COMMENT:
			if (not z)
				throw LexicalError(msg + "unterminated comment");
			break;
		case T_STRING_LITERAL:
			if (not z)
				throw LexicalError(msg + "unterminated string-literal");
			else if ((*null)(z))
				throw LexicalError(msg + "null character in string-literal");
			else if ((*lf)(z))
				throw LexicalError(msg + "raw line feed in string literal");
			else if ((*backslash)(z))
				throw LexicalError(msg + "invalid escape sequence in string-literal");
			break;
		case T_INTEGER_LITERAL:
			throw LexicalError(msg + "invalid integer-literal " + read(x_, z));
	}

	throw LexicalError(msg + "lexical error: invalid character " + *z);
}
