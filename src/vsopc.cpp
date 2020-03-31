#include "vsop.tab.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

/* global variables */
int yymode;
List<Class> yyprogram;

/* bison global variables */
extern int yyerrs;

/* bison global functions */
extern int yyparse(void);
extern void yyrelocate(const Node* n);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(char*);
extern void yyclose();

int lexer(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_LEXER;
	yyparse();

	yyclose();
	return yyerrs;
}

int parser(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_PARSER;
	yyparse();

	if (yyprogram.empty()) {
		yyclose();
		return yyerrs;
	}

	Program* p = new Program(yyprogram);
	cout << p->to_string() << endl;

	yyclose();
	return yyerrs;
}

int checker(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_PARSER;
	yyparse();

	if (yyprogram.empty()) {
		yyclose();
		return yyerrs;
	}

	Program* p = new Program(yyprogram);

	/***** OBJECT *****/

	Class object = Class(
		"Object",
		"Object",
		List<Field>(),
		List<Method>().add(
			new Method("print", List<Formal>().add(new Formal("s", "string")), "Object", Block())).add(
			new Method("printBool", List<Formal>().add(new Formal("b", "bool")), "Object", Block())).add(
			new Method("printInt32", List<Formal>().add(new Formal("i", "int32")), "Object", Block())).add(
			new Method("inputLine", List<Formal>(), "string", Block())).add(
			new Method("inputBool", List<Formal>(), "bool", Block())).add(
			new Method("inputInt32", List<Formal>(), "int32", Block())
		)
	);

	p->classes_table["Object"] = &object;

	for (Method* m: object.methods)
		object.methods_table[m->name] = m;

	/***** REDEFINITION *****/

	/* Class redefinition */
	for (auto it = p->classes.begin(); it != p->classes.end();)
		if (p->classes_table.find((*it)->name) == p->classes_table.end()) {
			p->classes_table[(*it)->name] = *it; ++it;
		} else {
			yyrelocate(*it);
			yyerror("semantic error, redefinition of class " + (*it)->name);
			it = p->classes.erase(it);
		}

	for (Class* c: p->classes) {
		/* Field redefinition */
		for (auto it = c->fields.begin(); it != c->fields.end();)
			if ((*it)->name == "self") {
				yyrelocate(*it);
				yyerror("semantic error, forbidden field name self");
				it = c->fields.erase(it);
			} else if (c->fields_table.find((*it)->name) == c->fields_table.end()) {
				c->fields_table[(*it)->name] = *it; ++it;
			} else {
				yyrelocate(*it);
				yyerror("semantic error, redefinition of field " + (*it)->name + " of class " + c->name);
				it = c->fields.erase(it);
			}

		/* Method redefinition */
		for (auto it = c->methods.begin(); it != c->methods.end();)
			if ((*it)->name == "self") {
				yyrelocate(*it);
				yyerror("semantic error, forbidden method name self");
				it = c->methods.erase(it);
			} else if (c->methods_table.find((*it)->name) == c->methods_table.end()) {
				c->methods_table[(*it)->name] = *it; ++it;
			} else {
				yyrelocate(*it);
				yyerror("semantic error, redefinition of method " + (*it)->name + " of class " + c->name);
				it = c->methods.erase(it);
			}

		for (Method* m : c->methods)
			/* Formal redefinition */
			for (auto it = m->formals.begin(); it != m->formals.end();)
				if ((*it)->name == "self") {
					yyrelocate(*it);
					yyerror("semantic error, forbidden formal name self");
					it = m->formals.erase(it);
				} else if (m->formals_table.find((*it)->name) == m->formals_table.end()) {
					m->formals_table[(*it)->name] = *it; ++it;
				} else {
					yyrelocate(*it);
					yyerror("semantic error, redefinition of formal " + (*it)->name + " of method " + m->name + " of class " + c->name);
					it = m->formals.erase(it);
				}
	}

	/***** INHERITANCE *****/

	/* Class inheritance */
	unordered_set<string> genealogy;

	for (auto it = p->classes.begin(); it != p->classes.end(); genealogy.clear()) {
		Class* c = *it;

		for (; c->parent != "Object"; c = p->classes_table[c->parent])
			if (genealogy.find(c->parent) != genealogy.end())
				break;
			else if (p->classes_table.find(c->parent) == p->classes_table.end())
				break;
			else
				genealogy.insert(c->parent);

		if (c->parent == "Object")
			++it;
		else {
			yyrelocate(*it);
			yyerror("semantic error, class " + (*it)->name + " cannot extend class " + c->parent);
			p->classes_table.erase((*it)->name);
			it = p->classes.erase(it);
		}
	}

	/***** OVERRIDE *****/

	for (Class* c: p->classes) {
		/* Field override */
		for (auto it = c->fields.begin(); it != c->fields.end();) {
			Class* k = c;

			while (k->name != "Object") {
				k = p->classes_table[c->parent];
				if (k->fields_table.find((*it)->name) != k->fields_table.end())
					break;
			}

			if (k->name == "Object")
				++it;
			else {
				yyrelocate(*it);
				yyerror("semantic error, overriding field " + (*it)->name + " of class " + c->name);
				c->fields_table.erase((*it)->name);
				it = c->fields.erase(it);
			}
		}

		/* Method override */
		for (auto it = c->methods.begin(); it != c->methods.end();) {
			Class* k = c;

			while (k->name != "Object") {
				k = p->classes_table[k->parent];

				if (k->methods_table.find((*it)->name) == k->methods_table.end())
					continue;

				Method* m = k->methods_table[(*it)->name];

				if ((*it)->type != m->type)
					break;
				else if ((*it)->formals.size() != m->formals.size())
					break;
				else {
					int i = 0;
					for (; i < (*it)->formals.size(); ++i)
						if ((*it)->formals[i]->name != m->formals[i]->name)
							break;
						else if ((*it)->formals[i]->type != m->formals[i]->type)
							break;

					if (i < (*it)->formals.size())
						break;
				}
			}

			if (k->name == "Object")
				++it;
			else {
				yyrelocate(*it);
				yyerror("semantic error, overriding method " + (*it)->name + " of class " + c->name + " with different type");
				c->methods_table.erase((*it)->name);
				it = c->methods.erase(it);
			}
		}
	}

	/***** TYPE *****/

	vector<Error> errors;

	p->check(p, {}, errors);

	for (Error& e: errors) {
		yyrelocate(e.node);
		yyerror("semantic error, " + e.msg);
	}

	cout << p->to_string() << endl;

	yyclose();
	return yyerrs;
}

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;
	else if (argc < 3) {
		cerr << "vsopc " << argv[1] << ": error: no input file" << std::endl;
		return 1;
	}

	string action = argv[1];

	if (action == "-lex")
		return lexer(argv[2]);
	else if (action == "-parse")
		return parser(argv[2]);
	else if (action == "-check")
		return checker(argv[2]);

	return 0;
}
