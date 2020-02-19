#ifndef REGEX_H
#define REGEX_H

#include <functional>
#include <memory>

/* Classes */

template <typename T>
class Expression {
	public:
		/* Constructors */
		Expression() {}
		Expression(const std::function<bool(T&)>& f) : f_(f) {}

		/* Operators */
		virtual bool operator ()(T& x) const { return f_(x); };

	private:
		/* Variables */
		std::function<bool(T&)> f_;
};

template <typename T>
using expr_ptr = typename std::shared_ptr<const Expression<T>>;

template <typename T>
class Repetition : public Expression<T> {
	public:
		Repetition(const expr_ptr<T>& expr) : expr_(expr) {}

		virtual bool operator ()(T& x) const {
			for (T y(x); (*expr_)(y); x = y);
			return true;
		}

	private:
		expr_ptr<T> expr_;
};

template <typename T>
class Option : public Expression<T> {
	public:
		Option(const expr_ptr<T>& expr) : expr_(expr) {}

		virtual bool operator ()(T& x) const {
			T y(x);
			if ((*expr_)(y))
				x = y;
			return true;
		}

	private:
		expr_ptr<T> expr_;
};

template <typename T>
class Alternation : public Expression<T> {
	public:
		Alternation(const expr_ptr<T>& a, const expr_ptr<T>& b) : a_(a), b_(b) {}

		virtual bool operator ()(T& x) const {
			T y(x), z(x);

			if ((*a_)(y)) {
				if ((*b_)(z) and y < z)
					x = z;
				else
					x = y;
			} else if ((*b_)(z))
				x = z;
			else {
				x = y < z ? z : y;
				return false;
			}

			return true;
		}
	private:
		expr_ptr<T> a_, b_;
};

template <typename T>
class Concatenation : public Expression<T> {
	public:
		Concatenation(const expr_ptr<T>& a, const expr_ptr<T>& b) : a_(a), b_(b) {}

		virtual bool operator ()(T& x) const {
			return (*a_)(x) and (*b_)(x);
		}

	private:
		expr_ptr<T> a_, b_;
};

template <typename T>
class Exclusion : public Expression<T> {
	public:
		Exclusion(const expr_ptr<T>& a, const expr_ptr<T>& b) : a_(a), b_(b) {}

		virtual bool operator ()(T& x) const {
			T y(x), z(x);

			if ((*a_)(y)) {
				if ((*b_)(z) and y == z)
					return false;

				x = y;
				return true;
			}

			x = y;
			return false;
		}

	private:
		expr_ptr<T> a_, b_;
};

/* Methods */

template <typename T>
static expr_ptr<T> special(const std::function<bool(T&)>& f) {
	return std::make_shared<Expression<T>>(f);
}

template <typename T>
static expr_ptr<T> equality(char c) {
	return std::make_shared<Expression<T>>(
		[c](T& x) {
			if (*x == c) {
				++x;
				return true;
			}

			return false;
		}
	);
}

template <typename T>
static expr_ptr<T> equality(const std::string& s) {
	return std::make_shared<Expression<T>>(
		[s](T& x) {
			for (size_t i = 0; i < s.length(); ++i, ++x)
				if (*x != s[i])
					return false;

			return true;
		}
	);
}

template <typename T>
static expr_ptr<T> range(char a, char b) {
	return std::make_shared<Expression<T>>(
		[a, b](T& x) {
			if (*x >= a and *x <= b) {
				++x;
				return true;
			}

			return false;
		}
	);
}

template <typename T>
static expr_ptr<T> operator ++(const expr_ptr<T>& expr, int unused) {
	return std::make_shared<Repetition<T>>(expr);
}

template <typename T>
static expr_ptr<T> operator --(const expr_ptr<T>& expr, int unused) {
	return std::make_shared<Option<T>>(expr);
}

template <typename T>
static expr_ptr<T> operator |(const expr_ptr<T>& a, const expr_ptr<T>& b) {
	return std::make_shared<Alternation<T>>(a, b);
}

template <typename T>
static expr_ptr<T> operator +(const expr_ptr<T>& a, const expr_ptr<T>& b) {
	return std::make_shared<Concatenation<T>>(a, b);
}

template <typename T>
static expr_ptr<T> operator -(const expr_ptr<T>& a, const expr_ptr<T>& b) {
	return std::make_shared<Exclusion<T>>(a, b);
}

#endif
