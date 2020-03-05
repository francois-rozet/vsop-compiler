#ifndef AST_H
#define AST_H

#include "tools.hpp"

#include <string>
#include <vector>

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Methods */
		virtual std::string to_string() const = 0;
};

template <typename T>
class List: public Node {
	public:
		/* Constructors */
		List() {}

		/* Methods */
		List& push(Node* t) { if (t) stack_.push_back(*static_cast<T*>(t)); return *this; }

		virtual std::string to_string() const {
			if (stack_.empty())
				return "[]";

			std::string s = stack_.back().to_string();

			for (auto it = stack_.rbegin() + 1; it != stack_.rend(); ++it)
				s += "," + it->to_string();

			return "[" + s + "]";
		}

		/* Accessors */
		size_t size() const { return stack_.size(); }
		const T& back() const { return stack_.back(); }

	protected:
		/* Variables */
		std::vector<T> stack_;
};

class iExpr: public Node {
	public:
		/* Constructors */
		iExpr(Node* expr) : expr_(expr) {}

		/* Methods */
		virtual std::string to_string() const {
			return expr_->to_string();
		}
	private:
		/* Variables */
		Node* expr_;
};

class Args: public List<iExpr> {
	public:
		/* Methods */
		List& push(Node* t) { if (t) stack_.push_back(iExpr(t)); return *this; }
};

class Block: public Node {
	public:
		/* Constructors */
		Block(const Args& exprs) : exprs_(exprs) {}

		/* Methods */
		virtual std::string to_string() const {
			if (exprs_.size() == 1)
				return exprs_.back().to_string();
			else
				return exprs_.to_string();
		}

	private:
		/* Variables */
		Args exprs_;
};

class Field: public Node {
	public:
		/* Constructors */
		Field(const std::string& name, const std::string& type, Node* init):
			name_(name), type_(type), init_(init) {}

		/* Methods */
		virtual std::string to_string() const {
			if (init_)
				return "Field(" + name_ + "," + type_ + "," + init_->to_string() + ")";
			else
				return "Field(" + name_ + "," + type_ + ")";
		}

	private:
		/* Variables */
		std::string name_, type_;
		Node* init_;
};

class Formal: public Node {
	public:
		/* Constructors */
		Formal(const std::string& name, const std::string& type): name_(name), type_(type) {}

		/* Methods */
		virtual std::string to_string() const {
			return name_ + ":" + type_;
		}

	private:
		/* Variables */
		std::string name_, type_;
};

class Method: public Node {
	public:
		/* Constructors */
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, const Block& block):
			name_(name), formals_(formals), type_(type), block_(block) {}

		/* Methods */
		virtual std::string to_string() const {
			return "Method(" + name_ + "," + formals_.to_string() + "," + type_ + "," + block_.to_string() + ")";
		}

	private:
		/* Variables */
		std::string name_, type_;
		List<Formal> formals_;
		Block block_;
};

struct Declaration {
	List<Field> fields;
	List<Method> methods;
};

class Class: public Node {
	public:
		/* Constructors */
		Class(const std::string& name, const std::string& parent, const List<Field>& fields, const List<Method>& methods):
			name_(name), parent_(parent), fields_(fields), methods_(methods) {}

		/* Methods */
		virtual std::string to_string() const {
			return "Class(" + name_ + "," + parent_ + "," + fields_.to_string() + "," + methods_.to_string() + ")";
		}

	private:
		/* Variables */
		std::string name_, parent_;
		List<Field> fields_;
		List<Method> methods_;
};

class If: public Node {
	public:
		/* Constructors */
		If(Node* cond, Node* then, Node* els):
			cond_(cond), then_(then), else_(els) {}

		/* Methods */
		virtual std::string to_string() const {
			if (else_)
				return "If(" + cond_->to_string() + "," + then_->to_string() + "," + else_->to_string() + ")";
			else
				return "If(" + cond_->to_string() + "," + then_->to_string() + ")";
		}

	private:
		/* Variables */
		Node* cond_, * then_, * else_;
};

class While: public Node {
	public:
		/* Constructors */
		While(Node* cond, Node* body): cond_(cond), body_(body) {}

		/* Methods */
		virtual std::string to_string() const {
			return "While(" + cond_->to_string() + "," + body_->to_string() + ")";
		}

	private:
		/* Variables */
		Node* cond_, * body_;
};

class Let: public Node {
	public:
		/* Constructors */
		Let(const std::string& name, const std::string& type, Node* init, Node* scope):
			name_(name), type_(type), init_(init), scope_(scope) {}

		/* Methods */
		virtual std::string to_string() const {
			if (init_)
				return "Let(" + name_ + "," + type_ + "," + init_->to_string() + "," + scope_->to_string() + ")";
			else
				return "Let(" + name_ + "," + type_ + "," + scope_->to_string() + ")";
		}

	private:
		/* Variables */
		std::string name_, type_;
		Node* init_, * scope_;
};

class Assign: public Node {
	public:
		/* Constructors */
		Assign(const std::string& name, Node* value):
			name_(name), value_(value) {}

		/* Methods */
		virtual std::string to_string() const {
			return "Assign(" + name_ + "," + value_->to_string() + ")";
		}

	private:
		/* Variables */
		std::string name_;
		Node* value_;
};

class Unary: public Node {
	public:
		enum Type { NOT, MINUS, ISNULL };

		/* Constructors */
		Unary(Type type, Node* value): type_(type), value_(value) {}

		/* Methods */
		virtual std::string to_string() const {
			std::string op;

			switch (type_) {
				case NOT: op = "not"; break;
				case MINUS: op = "-"; break;
				case ISNULL: op = "isnull"; break;
			}

			return "UnOp(" + op + "," + value_->to_string() + ")";
		}

	private:
		/* Variables */
		Type type_;
		Node* value_;
};

class Binary: public Node {
	public:
		enum Type { AND, EQUAL, LOWER, LOWER_EQUAL, PLUS, MINUS, TIMES, DIV, POW };

		/* Constructors */
		Binary(Type type, Node* left, Node* right): type_(type), left_(left), right_(right) {}

		/* Methods */
		virtual std::string to_string() const {
			std::string op;

			switch (type_) {
				case AND: op = "and"; break;
				case EQUAL: op = "="; break;
				case LOWER: op = "<"; break;
				case LOWER_EQUAL: op = "<="; break;
				case PLUS: op = "+"; break;
				case MINUS: op = "-"; break;
				case TIMES: op = "*"; break;
				case DIV: op = "/"; break;
				case POW: op = "^"; break;
			}

			return "BinOp(" + op + "," + left_->to_string() + "," + right_->to_string() + ")";
		}

	private:
		/* Variables */
		Type type_;
		Node* left_, * right_;
};

class Call: public Node {
	public:
		/* Constructors */
		Call(Node* scope, const std::string& name, const Args& args):
			scope_(scope), name_(name), args_(args) {}

		/* Methods */
		virtual std::string to_string() const {
			if (scope_)
				return "Call(" + scope_->to_string() + "," + name_ + "," + args_.to_string() + ")";
			else
				return "Call(self," + name_ + "," + args_.to_string() + ")";
		}

	private:
		/* Variables */
		Node* scope_;
		std::string name_;
		Args args_;
};

class New: public Node {
	public:
		/* Constructors */
		New(const std::string& type): type_(type) {}

		/* Methods */
		virtual std::string to_string() const {
			return "New(" + type_ + ")";
		}

	private:
		/* Variables */
		std::string type_;
};

class Identifier: public Node {
	public:
		/* Constructors */
		Identifier(const std::string& id): id_(id) {}

		/* Methods */
		virtual std::string to_string() const {
			return id_;
		}

	private:
		/* Variables */
		std::string id_;
};

class Integer: public Node {
	public:
		/* Constructors */
		Integer(int value): value_(value) {}

		/* Methods */
		virtual std::string to_string() const {
			return std::to_string(value_);
		}

	private:
		/* Variables */
		int value_;
};

class String: public Node {
	public:
		/* Constructors */
		String(const std::string& str): str_(str) {}

		/* Methods */
		virtual std::string to_string() const {
			std::string temp;

			for (const char& c: str_)
				switch (c) {
					case '\"':
					case '\\': temp += char2hex(c); break;
					default:
						if (c >= 32 and c <= 126)
							temp += c;
						else
							temp += char2hex(c);
				}

			return "\"" + temp + "\"";
		}

	private:
		/* Variables */
		std::string str_;
};

class Boolean: public Node {
	public:
		/* Constructors */
		Boolean(bool b): b_(b) {}

		/* Methods */
		virtual std::string to_string() const {
			return b_ ? "true" : "false";
		}

	private:
		/* Variables */
		bool b_;
};

class Unit: public Node {
	public:
		/* Methods */
		virtual std::string to_string() const {
			return "()";
		}
};

#endif
