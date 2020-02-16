#include "bnf.hpp"

using namespace std;

bool Repetition::f(Cursor& x) const {
	for (Cursor y(x); expr_->f(y); x = y);

	return true;
}

bool Option::f(Cursor& x) const {
	Cursor y(x);

	if (expr_->f(y))
		x = y;

	return true;
}

bool Alternation::f(Cursor& x) const {
	Cursor y(x), z(x);
	bool A = a_->f(y), B = b_->f(z);

	if (y < z) {
		x = z;
		return B;
	} else if (z < y) {
		x = y;
		return A;
	} else {
		x = y;
		return A or B;
	}
}

bool Concatenation::f(Cursor& x) const {
	if (a_->f(x) and b_->f(x))
		return true;

	return false;
}

bool Exclusion::f(Cursor& x) const {
	Cursor y(x), z(x);

	if (a_->f(y)) {
		if (b_->f(z) and y == z)
			return false;

		x = y;
		return true;
	}

	x = y;
	return false;
}

expr_ptr special(const std::function<bool(Cursor&)>& f) {
	return std::make_shared<Expression>(f);
}

expr_ptr equality(char c) {
	return std::make_shared<Expression>(
		[c](Cursor& x) {
			if (x.c() == c) {
				++x;
				return true;
			}

			return false;
		}
	);
}

expr_ptr equality(const std::string& s) {
	return std::make_shared<Expression>(
		[s](Cursor& x) {
			for (size_t i = 0; i < s.length(); i++, ++x)
				if (x.c() != s[i])
					return false;

			return true;
		}
	);
}

expr_ptr range(char a, char b) {
	return std::make_shared<Expression>(
		[a, b](Cursor& x) {
			if (x.c() >= a and x.c() <= b) {
				++x;
				return true;
			}

			return false;
		}
	);
}

expr_ptr operator ++(const expr_ptr& expr, int unused) {
	return std::make_shared<Repetition>(expr);
}

expr_ptr operator --(const expr_ptr& expr, int unused) {
	return std::make_shared<Option>(expr);
}

expr_ptr operator |(const expr_ptr& a, const expr_ptr& b) {
	return std::make_shared<Alternation>(a, b);
}

expr_ptr operator +(const expr_ptr& a, const expr_ptr& b) {
	return std::make_shared<Concatenation>(a, b);
}

expr_ptr operator -(const expr_ptr& a, const expr_ptr& b) {
	return std::make_shared<Exclusion>(a, b);
}
