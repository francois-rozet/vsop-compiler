#ifndef CURSOR_H
#define CURSOR_H

#include <string>
#include <vector>

class Cursor {
	public:
		/* Constructors */
		Cursor(const std::vector<std::string>&);

		/* Operators */
		Cursor& operator =(const Cursor& x);
		Cursor& operator ++();
		Cursor& operator --();
		bool operator <(const Cursor& x) const;
		bool operator ==(const Cursor& x) const;

		/* Methods */
		bool end_of_file() const;

		std::string read_to(const Cursor& y);

		/* Accessors */
		unsigned line() const { return line_; }
		unsigned column() const { return column_; }
		char c() const { return c_; }

	private:
		/* Variables */
		const std::vector<std::string>& input_;
		unsigned line_, column_, lineNumber_, lineLength_;
		char c_;

		/* Methods */
		char update();
};

#endif
