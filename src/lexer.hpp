#ifndef LEXER_H
#define LEXER_H

#include "cursor.hpp"

#include <memory>
#include <string>

class LexicalError: public std::exception {
	public:
		explicit LexicalError(const std::string& message) : message(message) {};
		virtual const char* what() const throw() { return message.c_str(); };

	private:
		std::string message;
};

struct Token {
	std::string type = "";
	std::string value = "";
};

class Lexer {
	public:
		/* Constructors */
		Lexer(const std::string& input) : x_(std::make_shared<Cursor>(input)) {}

		/* Methods */
		bool end_of_file() const { return x_->end_of_file(); };

		Token next();

		/* Accessors */
		Cursor x() const { return *x_; }
		unsigned line() const { return x_->line(); };
		unsigned column() const { return x_->column(); };
		char c() const { return x_->c(); };

	private:
		/* Variables */
		std::shared_ptr<Cursor> x_;
};

#endif
