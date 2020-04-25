#include "ast.hpp"
#include "tools.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

using namespace std;

/***** Scope *****/

/* Methods */
Scope& Scope::push(const string& name, const string& type) {
	if (this->contains(name))
		this->at(name).push_back(type);
	else
		this->insert({name, {type}});

	return *this;
}

Scope& Scope::pop(const string& name) {
	if (this->contains(name)) {
		if (this->at(name).size() > 1)
			this->at(name).pop_back();
		else
			this->erase(name);
	}

	return *this;
}

bool Scope::contains(const string& name) const {
	return this->find(name) != this->end();
}

const string& Scope::get(const string& name) const {
	if (this->contains(name))
		return this->at(name).back();
	else
		throw out_of_range("Unknown key " + name);
}

/***** Block *****/

/* Constructors */
Block::Block(const List<Expr>& exprs): exprs(exprs) {
	reverse(this->exprs.begin(), this->exprs.end());
}

/* Methods */
string Block::to_string_aux() const {
	if (exprs.size() == 1)
		return exprs.front()->to_string_aux();
	return exprs.to_string();
}

void Block::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	exprs.check(p, s, errors);
}

string Block::get_type(Program* p, Scope& s) const {
	return exprs.empty() ? "error" : exprs.back()->get_type(p, s);
}

/***** Field *****/

/* Methods */
string Field::to_string() const {
	string str = "Field(" + name + "," + type;
	if (init)
		str += "," + init->to_string();
	return str + ")";
}

Scope& Field::increase(Scope& s) const {
	return s.push(name, type);
}

Scope& Field::decrease(Scope& s) const {
	return s.pop(name);
}

void Field::check(Program* p, Scope& s, vector<Error>& errors) {
	if (not AST::is_primitive(p, type) and not AST::is_class(p, type))
		errors.push_back({this->line, this->column, "unknown type " + type});

	if (init) {
		init->check(p, s, errors);
		string init_t = init->get_type(p, s);

		if (not AST::conforms_to(p, init_t, type))
			errors.push_back({init->line, init->column, "expected type " + type + ", but got type " + init_t});
	}
}

string Field::get_type(Program* p, Scope& s) const {
	return type;
}

/***** Formal *****/

/* Methods */
string Formal::to_string() const {
	return name + ":" + type;
}

Scope& Formal::increase(Scope& s) const {
	return s.push(name, type);
}

Scope& Formal::decrease(Scope& s) const {
	return s.pop(name);
}

void Formal::check(Program* p, Scope& s, vector<Error>& errors) {
	if (not AST::is_primitive(p, type) and not AST::is_class(p, type))
		errors.push_back({this->line, this->column, "unknown type " + type});
}

string Formal::get_type(Program* p, Scope& s) const {
	return type;
}

/***** Method *****/

/* Constructors */
Method::Method(const string& name, const List<Formal>& formals, const string& type, Block* block): name(name), formals(formals), type(type), block(block) {
	reverse(this->formals.begin(), this->formals.end());
}

/* Methods */
string Method::to_string() const {
	return "Method(" + name + "," + formals.to_string() + "," + type + "," + block->to_string() + ")";
}

Scope& Method::increase(Scope& s) const {
	return formals.increase(s);
}

Scope& Method::decrease(Scope& s) const {
	return formals.decrease(s);
}

void Method::augment(vector<Error>& errors) {
	for (auto it = formals.begin(); it != formals.end(); ++it)
		if (formals_table.find((*it)->name) != formals_table.end()) {
			errors.push_back({(*it)->line, (*it)->column, "redefinition of formal " + (*it)->name});
			it = prev(formals.erase(it));
		} else
			formals_table[(*it)->name] = *it;
}

void Method::check(Program* p, Scope& s, vector<Error>& errors) {
	formals.check(p, s, errors);

	if (not AST::is_primitive(p, type) and not AST::is_class(p, type))
		errors.push_back({this->line, this->column, "unknown type " + type});

	this->increase(s);
	block->check(p, s, errors);
	string block_t = block->get_type(p, s);
	this->decrease(s);

	if (not AST::conforms_to(p, block_t, type))
		errors.push_back({block->line, block->column, "expected type " + type + ", but got type " + block_t});
}

string Method::get_type(Program* p, Scope& s) const {
	return type;
}

/***** Class *****/

/* Constructors */
Class::Class(const string& name, const string& parent, const List<Field>& fields, const List<Method>& methods):
	name(name), parent_name(parent), fields(fields), methods(methods) {
		reverse(this->fields.begin(), this->fields.end());
		reverse(this->methods.begin(), this->methods.end());
	}

/* Methods */
string Class::to_string() const {
	return "Class(" + name + "," + parent_name + "," + fields.to_string() + "," + methods.to_string() + ")";
}

Scope& Class::increase(Scope& s) const {
	for (auto it = fields_table.begin(); it != fields_table.end(); ++it)
		it->second->increase(s);
	return s.push("self", name);
}

Scope& Class::decrease(Scope& s) const {
	for (auto it = fields_table.begin(); it != fields_table.end(); ++it)
		it->second->decrease(s);
	return s.pop("self");
}

void Class::augment(vector<Error>& errors) {
	// Fields
	for (auto it = fields.begin(); it != fields.end(); ++it)
		if (fields_table.find((*it)->name) != fields_table.end()) {
			errors.push_back({(*it)->line, (*it)->column, "redefinition of field " + (*it)->name});
			it = prev(fields.erase(it));
		} else if (parent->fields_table.find((*it)->name) != parent->fields_table.end()) {
			errors.push_back({(*it)->line, (*it)->column, "overriding field " + (*it)->name});
			it = prev(fields.erase(it));
		} else
			fields_table[(*it)->name] = *it;

	fields_table.insert(parent->fields_table.begin(), parent->fields_table.end());

	// Methods
	for (shared_ptr<Method>& m: methods)
		m->augment(errors);

	for (auto it = methods.begin(); it != methods.end(); ++it)
		if (methods_table.find((*it)->name) != methods_table.end()) {
			errors.push_back({(*it)->line, (*it)->column, "redefinition of method " + (*it)->name});
			it = prev(methods.erase(it));
		} else if (parent->methods_table.find((*it)->name) != parent->methods_table.end()) {
			shared_ptr<Method> m = parent->methods_table[(*it)->name];

			bool same = (*it)->type == m->type; // output type
			same = same and (*it)->formals.size() == m->formals.size(); // number of formals

			for (int i = 0; same and i < (*it)->formals.size(); ++i)
				same = (*it)->formals[i]->type == m->formals[i]->type; // formal types

			if (same)
				methods_table[(*it)->name] = *it;
			else {
				errors.push_back({(*it)->line, (*it)->column, "overriding method " + (*it)->name + " with different signature"});
				it = prev(methods.erase(it));
			}
		} else
			methods_table[(*it)->name] = *it;

	for (auto it = parent->methods_table.begin(); it != parent->methods_table.end(); ++it)
		if (methods_table.find(it->first) == methods_table.end())
			methods_table[it->first] = it->second;
}

void Class::check(Program* p, Scope& s, vector<Error>& errors) {
	fields.check(p, s, errors);

	this->increase(s);
	methods.check(p, s, errors);
	this->decrease(s);
}

string Class::get_type(Program* p, Scope& s) const {
	return name;
}

/***** Program *****/

/* Constructors */
Program::Program(const List<Class>& classes): classes(classes) {
	reverse(this->classes.begin(), this->classes.end());
}

/* Methods */
string Program::to_string() const {
	return classes.to_string();
}

void Program::augment(vector<Error>& errors) {
	// Object
	classes_table["Object"] = shared_ptr<Class>(new Class("Object", "Object", {},
		List<Method>({
			new Method("print", {new Formal("s", "string")}, "Object", new Block()),
			new Method("printBool", {new Formal("b", "bool")}, "Object", new Block()),
			new Method("printInt32", {new Formal("i", "int32")}, "Object", new Block()),
			new Method("inputLine", {}, "string", new Block()),
			new Method("inputBool", {}, "bool", new Block()),
			new Method("inputInt32", {}, "int32", new Block())
		})
	));

	for (shared_ptr<Method>& m: classes_table["Object"]->methods)
		classes_table["Object"]->methods_table[m->name] = m;

	// Redefinition and overriding
	int size;

	do {
		size = classes_table.size();

		for (auto it = classes.begin(); it != classes.end(); ++it)
			if ((*it)->parent)
				continue;
			else if (classes_table.find((*it)->name) != classes_table.end()) {
				errors.push_back({(*it)->line, (*it)->column, "redefinition of class " + (*it)->name});
				it = prev(classes.erase(it));
			} else if (classes_table.find((*it)->parent_name) != classes_table.end()) {
				classes_table[(*it)->name] = *it;
				(*it)->parent = classes_table[(*it)->parent_name];
				(*it)->augment(errors);
			}

	} while (size < classes_table.size());

	for (auto it = classes.begin(); it != classes.end(); ++it)
		if (not (*it)->parent) {
			errors.push_back({(*it)->line, (*it)->column, "class " + (*it)->name + " cannot extend class " + (*it)->parent_name});
			it = prev(classes.erase(it));
		}

	// Main
	if (classes_table.find("Main") == classes_table.end())
		errors.push_back({this->line, this->column, "undefined class Main"});
	else {
		shared_ptr<Class> c = classes_table["Main"];

		if (c->methods_table.find("main") == c->methods_table.end())
			errors.push_back({c->line, c->column, "undefined method main"});
		else {
			shared_ptr<Method> m = c->methods_table["main"];

			if (m->formals.size() > 0 or m->type != "int32")
				errors.push_back({m->line, m->column, "main method of class Main defined with wrong signature"});
		}
	}
}

void Program::check(Program* p, Scope& s, vector<Error>& errors) {
	classes.check(p, s, errors);
}

/***** If *****/

/* Methods */
string If::to_string_aux() const {
	string str = "If(" + cond->to_string() + "," + then->to_string();
	if (els)
		str += "," + els->to_string();
	return str + ")";
}

void If::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	cond->check(p, s, errors);
	string cond_t = cond->get_type(p, s);

	if (cond_t != "bool")
		errors.push_back({this->line, this->column, "expected type bool, but got type " + cond_t});

	then->check(p, s, errors);
	if (els)
		els->check(p, s, errors);

	if (this->get_type(p, s) == "error")
		errors.push_back({this->line, this->column, "types " + then->get_type(p, s) + " and " + els->get_type(p, s) + " don't agree"});
}

string If::get_type(Program* p, Scope& s) const {
	string then_t = then->get_type(p, s);
	string els_t = els ? els->get_type(p, s) : "unit";

	if (then_t == "unit" or els_t == "unit")
		return "unit";
	else if (AST::is_primitive(p, then_t) and then_t == els_t)
		return then_t;
	else if (AST::is_class(p, then_t) and AST::is_class(p, els_t))
		return AST::common_ancestor(p, then_t, els_t);
	else
		return "error";
}

/***** While *****/

/* Methods */
string While::to_string_aux() const {
	return "While(" + cond->to_string() + "," + body->to_string() + ")";
}

void While::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	cond->check(p, s, errors);
	string cond_t = cond->get_type(p, s);

	if (cond_t != "bool")
		errors.push_back({cond->line, cond->column, "expected type bool, but got type " + cond_t});

	body->check(p, s, errors);
}

string While::get_type(Program* p, Scope& s) const {
	return "unit";
}

/***** Let *****/

/* Methods */
string Let::to_string_aux() const {
	string str = "Let(" + name + "," + type + ",";
	if (init)
		str += init->to_string() + ",";
	return str + scope->to_string() + ")";
}

Scope& Let::increase(Scope& s) const {
	return s.push(name, type);
}

Scope& Let::decrease(Scope& s) const {
	return s.pop(name);
}

void Let::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	if (not AST::is_primitive(p, type) and not AST::is_class(p, type))
		errors.push_back({this->line, this->column, "unknown type " + type});

	if (init) {
		init->check(p, s, errors);
		string init_t = init->get_type(p, s);

		if (not AST::conforms_to(p, init_t, type))
			errors.push_back({init->line, init->column, "expected type " + type + ", but got type " + init_t});
	}

	this->increase(s);
	scope->check(p, s, errors);
	this->decrease(s);
}

string Let::get_type(Program* p, Scope& s) const {
	this->increase(s);
	string temp = scope->get_type(p, s);
	this->decrease(s);

	return temp;
}

/***** Assign *****/

/* Methods */
string Assign::to_string_aux() const {
	return "Assign(" + name + "," + value->to_string() + ")";
}

void Assign::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	value->check(p, s, errors);
	string value_t = value->get_type(p, s);

	if (s.contains(name)) {
		if (not AST::conforms_to(p, value_t, s.get(name)))
			errors.push_back({value->line, value->column, "expected type " + s.get(name) + ", but got type " + value_t});
	} else
		errors.push_back({this->line, this->column, "assignation to undefined " + name});
}

string Assign::get_type(Program* p, Scope& s) const {
	return s.contains(name) ? s.get(name) : "error";
}

/***** Unary *****/

/* Methods */
string Unary::to_string_aux() const {
	string str = "UnOp(";
	switch (type) {
		case NOT: str += "not"; break;
		case MINUS: str += "-"; break;
		case ISNULL: str += "isnull"; break;
	}
	return str + "," + value->to_string() + ")";
}

void Unary::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	string expected;
	switch (type) {
		case NOT: expected = "bool"; break;
		case MINUS: expected = "int32"; break;
		case ISNULL: expected = "Object"; break;
	}

	value->check(p, s, errors);
	string value_t = value->get_type(p, s);

	if (not AST::conforms_to(p, value_t, expected))
		errors.push_back({value->line, value->column, "expected type " + expected + ", but got type " + value_t});
}

string Unary::get_type(Program* p, Scope& scope) const {
	switch (type) {
		case NOT: return "bool";
		case MINUS: return "int32";
		case ISNULL: return "bool";
	}
	return "error";
}

/***** Binary *****/

/* Methods */
string Binary::to_string_aux() const {
	string str = "BinOp(";
	switch (type) {
		case AND: str += "and"; break;
		case EQUAL: str += "="; break;
		case LOWER: str += "<"; break;
		case LOWER_EQUAL: str += "<="; break;
		case PLUS: str += "+"; break;
		case MINUS: str += "-"; break;
		case TIMES: str += "*"; break;
		case DIV: str += "/"; break;
		case POW: str += "^"; break;
	}
	return str + "," + left->to_string() + "," + right->to_string() + ")";
}

void Binary::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	string expected;
	switch (type) {
		case AND: expected = "bool"; break;
		case EQUAL: break;
		case LOWER:
		case LOWER_EQUAL:
		case PLUS:
		case MINUS:
		case TIMES:
		case DIV:
		case POW: expected = "int32"; break;
	}

	left->check(p, s, errors);
	right->check(p, s, errors);

	string left_t = left->get_type(p, s);
	string right_t = right->get_type(p, s);

	bool cond = not expected.empty() or (expected.empty() and (AST::is_primitive(p, left_t) or AST::is_primitive(p, right_t)));

	if (cond and left_t != right_t)
		errors.push_back({this->line, this->column, "types " + left_t + " and " + right_t + " don't agree"});

	if (not expected.empty() and left_t != expected)
		errors.push_back({left->line, left->column, "expected type " + expected + ", but got type " + left_t});

	if (not expected.empty() and right_t != expected)
		errors.push_back({right->line, right->column, "expected type " + expected + ", but got type " + right_t});
}

string Binary::get_type(Program* p, Scope& s) const {
	switch (type) {
		case AND:
		case EQUAL:
		case LOWER:
		case LOWER_EQUAL: return "bool";
		case PLUS:
		case MINUS:
		case TIMES:
		case DIV:
		case POW: return "int32";
	}
	return "error";
}

/***** Call *****/

/* Constructors */
Call::Call(Expr* scope, const string& name, const List<Expr>& args):
	scope(scope), name(name), args(args) {
		reverse(this->args.begin(), this->args.end());
	}

/* Methods */
string Call::to_string_aux() const {
	return "Call(" + scope->to_string() + "," + name + "," + args.to_string() + ")";
}

void Call::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	scope->check(p, s, errors);
	string scope_t = scope->get_type(p, s);

	args.check(p, s, errors);

	if (AST::is_class(p, scope_t)) {
		shared_ptr<Class> c = p->classes_table[scope_t];

		if (c->methods_table.find(name) == c->methods_table.end())
			errors.push_back({this->line, this->column, "unknown method " + name});
		else {
			shared_ptr<Method> m = c->methods_table[name];

			if (args.size() != m->formals.size())
				errors.push_back({this->line, this->column, "call of method " + m->name + " with wrong number of arguments"});
			else
				for (int i = 0; i < args.size(); ++i)
					if (not AST::conforms_to(p, args[i]->get_type(p, s), m->formals[i]->type))
						errors.push_back({args[i]->line, args[i]->column, "expected type " + m->formals[i]->type + ", but got type " + args[i]->get_type(p, s)});
		}
	} else
		errors.push_back({scope->line, scope->column, "invalid class type " + scope_t});
}

string Call::get_type(Program* p, Scope& s) const {
	string scope_t = scope->get_type(p, s);

	if (AST::is_class(p, scope_t)) {
		shared_ptr<Class> c = p->classes_table[scope_t];

		if (c->methods_table.find(name) != c->methods_table.end())
			return c->methods_table[name]->type;
	}

	return "error";
}

/***** New *****/

/* Methods */
string New::to_string_aux() const {
	return "New(" + type + ")";
}

void New::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	if (not AST::is_primitive(p, type) and not AST::is_class(p, type))
		errors.push_back({this->line, this->column, "unknown type " + type});
}

string New::get_type(Program* p, Scope& s) const {
	return type;
}

/***** Identifier *****/

/* Methods */
string Identifier::to_string_aux() const {
	return id;
}

void Identifier::check_aux(Program* p, Scope& s, vector<Error>& errors) {
	if (not s.contains(id))
		errors.push_back({this->line, this->column, "undefined " + id});
}

string Identifier::get_type(Program* p, Scope& s) const {
	return s.contains(id) ? s.get(id) : "error";
}

/***** Integer *****/

/* Methods */
string Integer::to_string_aux() const {
	return std::to_string(value);
}

string Integer::get_type(Program* p, Scope& s) const {
	return "int32";
}

/***** String *****/

/* Methods */
string String::to_string_aux() const {
	string temp;
	for (const char& c: str)
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

string String::get_type(Program* p, Scope& s) const {
	return "string";
}

/***** Boolean ****/

/* Methods */
string Boolean::to_string_aux() const {
	return b ? "true" : "false";
}

string Boolean::get_type(Program* p, Scope& s) const {
	return "bool";
}

/***** Unit *****/

/* Methods */
string Unit::to_string_aux() const {
	return "()";
}

string Unit::get_type(Program* p, Scope& s) const {
	return "unit";
}
