#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class Scope: std::unordered_map<std::string, std::vector<std::string>> {
	public:
		/* Methods */
		Scope& push(const std::string&, const std::string&);
		Scope& pop(const std::string&);
		bool contains(const std::string&) const;
		const std::string& get(const std::string&) const;
};

class Node; // forward declaration
class Program; // forward declaration

struct Error {
	Node* node;
	std::string msg;
};

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Fields */
		int line = 1, column = 1;

		/* Methods */
		virtual std::string to_string() const = 0;

		virtual Scope& increase(Scope& s) const { return s; }
		virtual Scope& decrease(Scope& s) const { return s; }

		virtual void check(Program*, Scope&, std::vector<Error>&) = 0;
		virtual std::string get_type(Program*, Scope&) const { return "node"; };
};

template <typename T>
class List: public std::vector<T*>, public Node {
	public:
		/* Constructors */
		List() {}
		List(std::initializer_list<T*> init): std::vector<T*>(init) {}

		/* Methods */
		List& add(T* t) {
			this->push_back(t);
			return *this;
		}

		virtual std::string to_string() const {
			if (this->empty())
				return "[]";

			std::string str = "[" + this->front()->to_string();

			for (auto it = this->begin() + 1; it != this->end(); ++it)
				str += "," + (*it)->to_string();

			return str + "]";
		}

		virtual Scope& increase(Scope& s) const {
			for (T* t: *this)
				t->increase(s);
			return s;
		}

		virtual Scope& decrease(Scope& s) const {
			for (T* t: *this)
				t->decrease(s);
			return s;
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			for (T* t: *this)
				t->check(p, s, errors);
		}
};

class Expr: public Node {
	public:
		/* Methods */
		virtual std::string to_string_aux() const = 0;
		virtual std::string to_string() const {
			std::string str = this->to_string_aux();
			if (not type_.empty())
				str += ":" + type_;
			return str;
		}

		virtual void check_aux(Program* p, Scope& s, std::vector<Error>& errors) {}
		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			this->check_aux(p, s, errors);
			type_ = this->get_type(p, s);
		}

	private:
		/* Fields */
		std::string type_;
};

class Block: public Expr {
	public:
		/* Constructors */
		Block() {}
		Block(const List<Expr>&);

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Field: public Node {
	public:
		/* Constructors */
		Field(const std::string& name, const std::string& type, Expr* init):
			name(name), type(type), init(init) {}

		/* Fields */
		std::string name, type;
		Expr* init;

		/* Methods */
		virtual std::string to_string() const;
		virtual Scope& increase(Scope&) const;
		virtual Scope& decrease(Scope&) const;
		virtual void check(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Formal: public Node {
	public:
		/* Constructors */
		Formal(const std::string& name, const std::string& type): name(name), type(type) {}

		/* Fields */
		std::string name, type;

		/* Methods */
		virtual std::string to_string() const;
		virtual Scope& increase(Scope&) const;
		virtual Scope& decrease(Scope&) const;
		virtual void check(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Method: public Node {
	public:
		/* Constructors */
		Method(const std::string&, const List<Formal>&, const std::string&, const Block&);

		/* Fields */
		std::string name, type;

		List<Formal> formals;
		std::unordered_map<std::string, Formal*> formals_table;

		Block block;

		/* Methods */
		virtual std::string to_string() const;
		virtual Scope& increase(Scope&) const;
		virtual Scope& decrease(Scope&) const;
		void augment(std::vector<Error>&);
		virtual void check(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

struct Declaration {
	List<Field> fields;
	List<Method> methods;
};

class Class: public Node {
	public:
		/* Constructors */
		Class(const std::string&, const std::string&, const List<Field>&, const List<Method>&);

		/* Fields */
		std::string name, parent_name;

		List<Field> fields;
		std::unordered_map<std::string, Field*> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, Method*> methods_table;

		Class* parent;

		/* Methods */
		virtual std::string to_string() const;

		virtual Scope& increase(Scope&) const;
		virtual Scope& decrease(Scope&) const;
		void augment(std::vector<Error>&);
		virtual void check(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Program: public Node {
	public:
		/* Constructors */
		Program(const List<Class>&);

		/* Fields */
		List<Class> classes;
		std::unordered_map<std::string, Class*> classes_table;

		/* Methods */
		virtual std::string to_string() const;
		void augment(std::vector<Error>&);
		virtual void check(Program*, Scope&, std::vector<Error>&);
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}

		/* Fields */
		Expr* cond, * then, * els;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class While: public Expr {
	public:
		/* Constructors */
		While(Expr* cond, Expr* body): cond(cond), body(body) {}

		/* Fields */
		Expr* cond, * body;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Let: public Expr {
	public:
		/* Constructors */
		Let(const std::string& name, const std::string& type, Expr* init, Expr* scope):
			name(name), type(type), init(init), scope(scope) {}

		/* Fields */
		std::string name, type;
		Expr* init, * scope;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual Scope& increase(Scope&) const;
		virtual Scope& decrease(Scope&) const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Assign: public Expr {
	public:
		/* Constructors */
		Assign(std::string name, Expr* value): name(name), value(value) {}

		/* Fields */
		std::string name;
		Expr* value;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Unary: public Expr {
	public:
		enum Type { NOT, MINUS, ISNULL };

		/* Constructors */
		Unary(Type type, Expr* value): type(type), value(value) {}

		/* Fields */
		Type type;
		Expr* value;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Binary: public Expr {
	public:
		enum Type { AND, EQUAL, LOWER, LOWER_EQUAL, PLUS, MINUS, TIMES, DIV, POW };

		/* Constructors */
		Binary(Type type, Expr* left, Expr* right): type(type), left(left), right(right) {}

		/* Fields */
		Type type;
		Expr* left, * right;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Call: public Expr {
	public:
		/* Constructors */
		Call(Expr*, const std::string&, const List<Expr>&);

		/* Fields */
		Expr* scope;
		std::string name;
		List<Expr> args;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class New: public Expr {
	public:
		/* Constructors */
		New(const std::string& type): type(type) {}

		/* Fields */
		std::string type;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Identifier: public Expr {
	public:
		/* Constructors */
		Identifier(const std::string& id): id(id) {}

		/* Fields */
		std::string id;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual void check_aux(Program*, Scope&, std::vector<Error>&);
		virtual std::string get_type(Program*, Scope&) const;
};

class Self: public Identifier {
	public:
		/* Constructors */
		Self(): Identifier("self") {}
};

class Integer: public Expr {
	public:
		/* Constructors */
		Integer(int value): value(value) {}

		/* Fields */
		int value;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual std::string get_type(Program*, Scope&) const;
};

class String: public Expr {
	public:
		/* Constructors */
		String(const std::string& str): str(str) {}

		/* Fields */
		std::string str;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual std::string get_type(Program*, Scope&) const;
};

class Boolean: public Expr {
	public:
		/* Constructors */
		Boolean(bool b): b(b) {}

		/* Fields */
		bool b;

		/* Methods */
		virtual std::string to_string_aux() const;
		virtual std::string get_type(Program*, Scope&) const;
};

class Unit: public Expr {
	public:
		/* Methods */
		virtual std::string to_string_aux() const;
		virtual std::string get_type(Program*, Scope&) const;
};

namespace AST {
	static bool is_class(Program* p, const std::string& type) {
		return p->classes_table.find(type) != p->classes_table.end();
	}

	static bool is_primitive(Program* p, const std::string& type) {
		return type == "int32" or type == "string" or type == "bool" or type == "unit";
	}

	static bool conforms_to(Program* p, const std::string& a, const std::string& b) {
		if (is_class(p, a))
			for (Class* c = p->classes_table[a]; c != NULL; c = c->parent)
				if (c->name == b)
					return true;

		return a == b;
	}

	static std::string common_ancestor(Program* p, const std::string& a, const std::string& b) {
		Class* c = p->classes_table[a];
		Class* d = p->classes_table[b];

		std::unordered_set<std::string> C;
		std::unordered_set<std::string> D;

		while (c != NULL and d != NULL)
			if (c->name == d->name)
				return c->name;
			else if (C.find(d->name) == C.end())
				return d->name;
			else if (D.find(c->name) == D.end())
				return c->name;
			else {
				C.insert(c->name);
				D.insert(d->name);
				c = c->parent;
				d = d->parent;
			}

		return "Object";
	}
};

#endif
