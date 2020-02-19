#ifndef LEXER_H
#define LEXER_H

#include "cursor.hpp"

#include <string>

class LexicalError: public std::exception {
	public:
		explicit LexicalError(const std::string& message) : message(message) {};
		virtual const char* what() const throw() { return message.c_str(); };

	private:
		std::string message;
};

typedef unsigned token_type;

struct Token {
	token_type type;
	struct {
		std::string str;
		int num;
	} value = {"", 0};
};

class Lexer {
	public:
		/* Constructors */
		Lexer(const std::string& input) : x_(Cursor(input)) {}

		/* Methods */
		operator bool() const { return x_; };

		Token next(bool);

		/* Accessors */
		unsigned line() const { return x_.line(); };
		unsigned column() const { return x_.column(); };

	private:
		/* Variables */
		Cursor x_;
};

#endif
