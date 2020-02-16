#ifndef BNF_H
#define BNF_H

#include "cursor.hpp"

#include <functional>
#include <memory>

/* Classes */
class Expression {
	public:
		/* Constructors */
		Expression() {}
		Expression(const std::function<bool(Cursor&)>& f) : f_(f) {}

		/* Accessors */
		virtual bool f(Cursor& x) const { return f_(x); }

	private:
		/* Variables */
		std::function<bool(Cursor&)> f_;
};

typedef std::shared_ptr<const Expression> expr_ptr;

class Repetition : public Expression {
	public:
		Repetition(const expr_ptr& expr) : expr_(expr) {}

		bool f(Cursor& x) const;

	private:
		expr_ptr expr_;
};

class Option : public Expression {
	public:
		Option(const expr_ptr& expr) : expr_(expr) {}

		bool f(Cursor& x) const;

	private:
		expr_ptr expr_;
};

class Alternation : public Expression {
	public:
		Alternation(const expr_ptr& a, const expr_ptr& b) : a_(a), b_(b) {}

		bool f(Cursor& x) const;

	private:
		expr_ptr a_, b_;
};

class Concatenation : public Expression {
	public:
		Concatenation(const expr_ptr& a, const expr_ptr& b) : a_(a), b_(b) {}

		bool f(Cursor& x) const;

	private:
		expr_ptr a_, b_;
};

class Exclusion : public Expression {
	public:
		Exclusion(const expr_ptr& a, const expr_ptr& b) : a_(a), b_(b) {}

		bool f(Cursor& x) const;

	private:
		expr_ptr a_, b_;
};

/* Methods */
expr_ptr special(const std::function<bool(Cursor&)>&);
expr_ptr equality(char c);
expr_ptr equality(const std::string& s);
expr_ptr range(char a, char b);

expr_ptr operator ++(const expr_ptr& expr, int unused); // Repetition
expr_ptr operator --(const expr_ptr& expr, int unused); // Option
expr_ptr operator |(const expr_ptr& a, const expr_ptr& b); // Alternation
expr_ptr operator +(const expr_ptr& a, const expr_ptr& b); // Concatenation
expr_ptr operator -(const expr_ptr& a, const expr_ptr& b); // Exclusion

#endif
