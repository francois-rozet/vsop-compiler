#include "lexer.hpp"

#include <fstream>
#include <iostream>
#include <string>

using namespace std;

string read(const string& filename) {
	char c;
	string input;
	ifstream file;

	file.open(filename);

	if (!file.is_open())
		throw runtime_error("error: " + filename + ": No such file or directory");

	while (file.get(c))
		input += c;

	file.close();

	return input;
}

int lex(const string& filename) {
	string input;

	try {
		input = read(filename);
	} catch (runtime_error& e) {
		cerr << "lex: " << e.what() << endl;
		return -1;
	}

	Lexer lexer(input);
	int err = 0;

	while (lexer) {
		try {
			lexer.next(true);
		} catch (LexicalError& e) {
			cerr << filename << ':' << e.what() << endl;
			err++;
		}
	}

	return err;
}

int main(int argc, char* argv[]) {
	int err = 0;

	for (int i = 1, j; i < argc; i = j) {
		if (string(argv[i]) == "-lex") {
			for (j = i + 1; j < argc; j++)
				err = lex(argv[j]);
		} else {
			break;
		}

		if (j == i + 1)
			cerr << "vsopc " << argv[i] << ": error: no input file" << endl;
	}

	return err;
}
