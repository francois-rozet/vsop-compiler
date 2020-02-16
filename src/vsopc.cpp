#include "lexer.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

vector<string> read(const string& filename) {
	string line;
	ifstream file;

	vector<string> input;

	file.open(filename);

	if (!file.is_open())
		throw runtime_error("error: " + filename + ": No such file or directory");

	while (getline(file, line))
		input.push_back(line);

	file.close();

	return input;
}

int lex(const string& filename) {
	vector<string> input;

	try {
		input = read(filename);
	} catch (runtime_error& e) {
		cerr << "lex: " << e.what() << endl;
		return -1;
	}

	Lexer lexer(input);

	unsigned line, column;
	int err = 0;

	for (Token t; not lexer.end_of_file();) {
		line = lexer.line() + 1;
		column = lexer.column() + 1;

		try {
			t = lexer.next();
		} catch (LexicalError& e) {
			cerr << filename << ':' << e.what() << endl;
			err++;
			continue;
		}

		if (t.type == "end-of-file" or t.type == "whitespace" or t.type == "comment")
			continue;

		cout << line << ',' << column;

		if (t.type == "type-identifier" or t.type == "object-identifier" or t.type == "integer-literal" or t.type == "string-literal")
			cout << ',' << t.type << ',' << t.value;
		else if (t.type == "keyword")
			cout << ',' << t.value;
		else
			cout << ',' << t.type;

		cout << endl;
	}

	return err;
}

int main(int argc, char* argv[]) {
	string option;
	vector<string> input;
	unsigned errors = 0;

	for (int i = 1, j; i < argc; i = j) {
		option = argv[i];

		if (option == "-lex") {
			for (j = i + 1; j < argc; j++)
				errors += lex(argv[j]);
		} else {
			break;
		}

		if (j == i + 1)
			cerr << "vsopc " << option << ": error: no input file" << endl;
	}

	return errors;
}
