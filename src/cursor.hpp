#ifndef CURSOR_H
#define CURSOR_H

#include <string>

class Cursor {
	public:
		/* Constructors */
		Cursor(const std::string&);

		/* Operators */
		Cursor& operator =(const Cursor& x);
		Cursor& operator ++();
		bool operator <(const Cursor& x) const;
		bool operator <=(const Cursor& x) const;
		bool operator ==(const Cursor& x) const;

		/* Methods */
		bool end_of_file() const { return i_ == inputLength_; };

		std::string read_to(const Cursor& y);

		/* Accessors */
		unsigned line() const { return line_; }
		unsigned column() const { return column_; }
		char c() const { return this->end_of_file() ? '\f' : input_[i_]; }

	private:
		/* Variables */
		const std::string& input_;
		unsigned i_ = 0, line_ = 1, column_ = 1, inputLength_;
};

#endif
