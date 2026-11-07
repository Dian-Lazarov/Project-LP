#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>

using namespace std;

// Изброен тип за класовете памет
enum class SymbolClass {
	PRIVATE_CLASS,
	SHARED,
	SEM,
	CONST_CLASS,
	TEMP,
	UNDEFINED      // default
};

// Структура на записа в символната таблица
struct SymbolRecord {
	string symbol;
	int code;
	SymbolClass cls;
	int val;
	int addr;
	// Полета за следене на неизползвани променливи
	bool isUsed = false;
	int declLine = 0;
	int declCol = 0;
};

// ==========================================
// Символна таблица
// ==========================================
class SymbolTable {
private:

	// Векторът съхранява самите записи и осигурява целочислена позиция (индекс)
	vector<SymbolRecord> records;

	// Хеш-таблицата съпоставя лексемата (symbol) към нейната позиция във вектора
	unordered_map<string, int> hashTable;

public:

	// Основна функция: търси символ, добавя го при липса и връща позицията му
	int getOrInsert(const string& symbol, int code = 0, SymbolClass cls = SymbolClass::UNDEFINED, int val = 0, int addr = 0) {

		// 1. Търсене на символа в хеш-таблицата
		auto it = hashTable.find(symbol);
		if (it != hashTable.end()) {
			return it->second; // Връща позицията на вече съществуващия символ
		}

		// 2. Добавяне на нов символ, ако не е намерен
		int position = records.size();
		SymbolRecord newRecord = { symbol, code, cls, val, addr };

		records.push_back(newRecord);
		hashTable[symbol] = position; // Запис в хеш-таблицата с текущата позиция

		// 3. Връщане на позицията на новодобавения символ
		return position;
	}

	// Помощна функция за извличане на запис по неговата позиция (за нуждите на семантичния анализатор и генератора на обектен код)
	SymbolRecord& getRecord(int position) {
		if (position < 0 || position >= records.size()) {
			throw out_of_range("Невалидна позиция в символната таблица.");
		}
		return records[position];
	}

	// Помощна функция за обновяване на адрес на символ след фазата на семантичен анализ
	void updateAddress(int position, int newAddr) {
		if (position >= 0 && position < records.size()) {
			records[position].addr = newAddr;
		}
	}

	// Помощна функция за принтиране на таблицата
	void printTable() {
		cout << "\n=== Символна таблица ===\n";
		for (int i = 0; i < records.size(); i++) {
			cout << "Позиция: " << i << " | Символ: " << records[i].symbol
				<< " | Стойност: " << records[i].val << endl;
		}
		cout << "------------------------\n";
	}

	// Функция за извеждане на предупреждения (W01)
	void printUnusedWarnings() {
		for (const auto& rec : records) {
			if ((rec.cls == SymbolClass::PRIVATE_CLASS || rec.cls == SymbolClass::SHARED || rec.cls == SymbolClass::SEM) && !rec.isUsed) {
				cout << "Предупреждение (Ред " << rec.declLine << ", Колона " << rec.declCol
					<< "): Декларирана, но неизползвана променлива '" << rec.symbol << "'" << endl;
			}
		}
	}
};

// ==========================================
// Лексикален анализатор (Lexer)
// ==========================================
enum class TokenType {
	KEYWORD,
	IDENTIFIER,
	NUMBER,
	STRING_LITERAL,
	OPERATOR,
	DELIMITER,
	COMMENT,
	END_OF_FILE,
	ERROR_TOKEN
};

// Структура, описваща един разпознат токен
struct Token {
	TokenType type;
	string lexeme;
	int line;
	int col;
	int symTablePos; // Позиция в символната таблица (-1 ако не е приложимо)
};

class Lexer {
private:
	string source;
	int pos;
	int line;
	int col;
	SymbolTable& symTable;

	unordered_set<string> keywords = {
		"core", "int", "shared", "sem", "if", "else",
		"while", "for", "and", "or", "not", "xin",
		"xout", "init", "wait", "signal"
	};

	char peek(int offset = 0) {
		if (pos + offset >= source.length()) return '\0';
		return source[pos + offset];
	}

	char advance() {
		if (pos >= source.length()) return '\0';
		char current = source[pos++];
		if (current == '\n') {
			line++;
			col = 1;
		}
		else {
			col++;
		}
		return current;
	}

public:
	// Конструктор: зарежда файла и прави обръщение към символната таблица
	Lexer(const string& filename, SymbolTable& table) : pos(0), line(1), col(1), symTable(table) {
		ifstream file(filename);
		if (!file.is_open()) {
			// Хвърляме изключение при невъзможност за отваряне
			throw runtime_error("Файлът " + filename + " не може да бъде отворен!");
		}
		stringstream buffer;
		buffer << file.rdbuf();
		source = buffer.str();

		// Премахване на скрития маркер (UTF-8-BOM), ако съществува в началото
		if (source.size() >= 3 &&
			(unsigned char)source[0] == 0xEF &&
			(unsigned char)source[1] == 0xBB &&
			(unsigned char)source[2] == 0xBF) {
			source.erase(0, 3);
		}
	}

	Token getNextToken() {
		while (pos < source.length()) {
			char c = peek();

			// Прескачане на интервали, табулации и празни редове
			if (isspace((unsigned char)c)) {
				advance();
				continue;
			}

			int startCol = col;
			int startLine = line;

			// Състояния S1: Идентификатори и ключови/служебни думи
			if (isalpha((unsigned char)c)) {
				string word = "";
				while (isalnum((unsigned char)peek()) || peek() == '_') {
					word += advance();
				}

				// Проверка дали е ключова дума
				if (keywords.find(word) != keywords.end()) {
					return { TokenType::KEYWORD, word, startLine, startCol, -1 };
				}

				// Ако е идентификатор, добавяме го в символната таблица
				int symPos = symTable.getOrInsert(word, 100, SymbolClass::UNDEFINED, 0, 0);
				return { TokenType::IDENTIFIER, word, startLine, startCol, symPos };
			}

			// Състояния S2: Числови константи
			if (isdigit((unsigned char)c)) {
				string numStr = "";
				while (isdigit(peek())) {
					numStr += advance();
				}
				// Добавяме числото в символната таблица като константа
				int val = stoi(numStr);
				int symPos = symTable.getOrInsert(numStr, 101, SymbolClass::CONST_CLASS, val, 0);
				return { TokenType::NUMBER, numStr, startLine, startCol, symPos };
			}

			// Състояния S3: Текстови константи
			if (c == '#') {
				string strLit = "";
				strLit += advance(); // Консумираме началното #
				while (peek() != '#' && peek() != '\n' && peek() != '\0') {
					strLit += advance();
				}
				if (peek() == '#') {
					strLit += advance(); // Консумираме крайното #
					return { TokenType::STRING_LITERAL, strLit, startLine, startCol, -1 };
				}
				else {
					// Ако стигнем нов ред или край на файл без затварящ диез (#)
					throw runtime_error("Лексикална грешка (Ред " + to_string(startLine) + ", Колона " + to_string(startCol) + "): Незатворена текстова константа");
				}
			}

			// Състояния S4-S5: Едноредови коментари (~~)
			if (c == '~' && peek(1) == '~') {
				string comment = "";
				comment += advance(); // ~
				comment += advance(); // ~
				while (peek() != '\n' && peek() != '\0') {
					comment += advance();
				}
				return { TokenType::COMMENT, comment, startLine, startCol, -1 };
			}

			// Състояния S6-S8: Многоредови коментари (/~ ... ~/)
			if (c == '/' && peek(1) == '~') {
				string comment = "";
				comment += advance(); // /
				comment += advance(); // ~
				while (peek() != '\0') {
					if (peek() == '~' && peek(1) == '/') {
						comment += advance(); // ~
						comment += advance(); // /
						break;
					}
					comment += advance();
				}
				return { TokenType::COMMENT, comment, startLine, startCol, -1 };
			}

			// Разделители
			if (c == '{' || c == '}' || c == '(' || c == ')' || c == ';' || c == ',') {
				string delim(1, advance());
				return { TokenType::DELIMITER, delim, startLine, startCol, -1 };
			}

			// Оператор за присвоянане и аритметични оператори
			if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/') {
				string op(1, advance());
				return { TokenType::OPERATOR, op, startLine, startCol, -1 };
			}

			// Оператори за сравнение (<, <=, >, >=)
			if (c == '<' || c == '>') {
				string op = "";
				op += advance(); // Консумираме '<' или '>'

				// Проверяваме дали следващият символ е '='
				if (peek() == '=') {
					op += advance(); // Консумираме '=' и операторът става '<=' или '>='
				}
				return { TokenType::OPERATOR, op, startLine, startCol, -1 };
			}

			// Оператори за равенство/еквивалентност и различие (?=, @=)
			if (c == '?' || c == '@') {
				string op = "";
				op += advance(); // Консумираме '?' или '@'

				// Проверяваме дали следващият символ е '='
				if (peek() == '=') {
					op += advance(); // Консумираме '=' и операторът става '?=' или '@='
					return { TokenType::OPERATOR, op, startLine, startCol, -1 };
				}
				else {
					// Тъй като '?' и '@' не са самостоятелни оператори, липсата на '=' след тях е лексикална грешка.
					throw runtime_error("Лексикална грешка (Ред " + to_string(startLine) + ", Колона " + to_string(startCol) + "): Непознат символ '" + op + "'");
				}
			}

			// Непознат символ
			string err(1, advance());
			throw runtime_error("Лексикална грешка (Ред " + to_string(startLine) + ", Колона " + to_string(startCol) + "): Непознат символ '" + err + "'");
		}

		return { TokenType::END_OF_FILE, "EOF", line, col, -1 };
	}
};

// ==========================================
// Синтактичен анализатор (Parser)
// ==========================================
class Parser {
private:
	Lexer& lexer;
	SymbolTable& symTable; // SEM: Обръщение към символната таблица
	Token currentToken;
	bool hasError = false; // Флаг за отчитане на грешки

	// SEM: Брояч за временните променливи (TEMP)
	int tempCount = 1;

	// Извлича следващия токен, като автоматично прескача коментарите
	void nextToken() {
		do {
			currentToken = lexer.getNextToken();
		} while (currentToken.type == TokenType::COMMENT);
	}

	// Проверява дали текущият токен съвпада с очаквания. Ако да - продължава напред.
	void match(TokenType expectedType, const string& expectedLexeme = "") {
		if (currentToken.type == expectedType &&
			(expectedLexeme == "" || currentToken.lexeme == expectedLexeme)) {
			nextToken();
		}
		else {
			string expected = expectedLexeme != "" ? expectedLexeme : "тип " + to_string((int)expectedType);
			throw runtime_error("Синтактична грешка (Ред " + to_string(currentToken.line) +
				", Колона " + to_string(currentToken.col) +
				"): Очаква се '" + expected + "', но е намерено '" + currentToken.lexeme + "'");
		}
	}

	// Метод за възстановяване от синтактична грешка
	void synchronize() {
		hasError = true;

		nextToken(); // Прескачаме проблемния токен

		while (currentToken.type != TokenType::END_OF_FILE) {
			// Ако сме стигнали край на оператор
			if (currentToken.lexeme == ";") {
				nextToken();
				return;
			}

			// Ако сме стигнали началото на нова декларация или оператор
			if (currentToken.type == TokenType::KEYWORD) {
				if (currentToken.lexeme == "int" || currentToken.lexeme == "shared" ||
					currentToken.lexeme == "sem" || currentToken.lexeme == "if" ||
					currentToken.lexeme == "while" || currentToken.lexeme == "for" ||
					currentToken.lexeme == "xin" || currentToken.lexeme == "xout" ||
					currentToken.lexeme == "init" || currentToken.lexeme == "wait" ||
					currentToken.lexeme == "signal") {
					return;
				}
			}
			nextToken();
		}
	}

	// ==========================================
	// СЕМАНТИЧНИ МЕТОДИ
	// ==========================================

	void semanticError(const Token& t, const string& msg) {
		throw runtime_error("Семантична грешка (Ред " + to_string(t.line) + ", Колона " + to_string(t.col) + "): " + msg);
	}

	// Деклариране на нов символ (проверява за M02)
	void declareSymbol(const Token& t, SymbolClass cls) {
		SymbolRecord& rec = symTable.getRecord(t.symTablePos);
		if (rec.cls != SymbolClass::UNDEFINED) {
			semanticError(t, "Повторна декларация на името '" + t.lexeme + "'");
		}
		else {
			rec.cls = cls; // Задаваме съответния клас памет (PRIVATE, SHARED, SEM)
			// Запазваме позицията на декларацията
			rec.declLine = t.line;
			rec.declCol = t.col;
		}
	}

	// Проверка при използване на променлива (проверява за M01 и M03)
	void checkVarUsage(const Token& t) {
		SymbolRecord& rec = symTable.getRecord(t.symTablePos);
		rec.isUsed = true; // Маркираме променливата като използвана

		if (rec.cls == SymbolClass::UNDEFINED) {
			semanticError(t, "Използване на недекларирана променлива '" + t.lexeme + "'");
			rec.cls = SymbolClass::PRIVATE_CLASS; // Потискане на повтарящи се грешки
		}
		else if (rec.cls == SymbolClass::SEM) {
			semanticError(t, "Семафорът '" + t.lexeme + "' не може да се използва като променлива");
		}
	}

	// Проверка при използване на семафор в init/wait/signal (проверява за M01 и M04)
	void checkSemUsage(const Token& t) {
		SymbolRecord& rec = symTable.getRecord(t.symTablePos);
		rec.isUsed = true; // Маркираме семафора като използван

		if (rec.cls == SymbolClass::UNDEFINED) {
			semanticError(t, "Използване на недекларирана променлива '" + t.lexeme + "'");
			rec.cls = SymbolClass::SEM;
		}
		else if (rec.cls != SymbolClass::SEM) {
			semanticError(t, "Аргументът '" + t.lexeme + "' в синхронизиращ примитив не е семафор");
		}
	}

	// Създаване на временна променлива (TEMP) за междинни изчисления
	int createTemp() {
		string tempName = "&" + to_string(tempCount++);
		return symTable.getOrInsert(tempName, 0, SymbolClass::TEMP, 0, 0);
	}

public:
	Parser(Lexer& lex, SymbolTable& st) : lexer(lex), symTable(st) {
		nextToken();
	}

	bool hadError() const {
		return hasError;
	}

	// program -> "core" "{" decl_part stm_list "}" ";"
	void parseProgram() {
		match(TokenType::KEYWORD, "core");
		match(TokenType::DELIMITER, "{");

		parseDeclPart();
		parseStmList();

		match(TokenType::DELIMITER, "}");
		match(TokenType::DELIMITER, ";");

		if (currentToken.type != TokenType::END_OF_FILE) {
			throw runtime_error("Синтактична грешка (Ред " + to_string(currentToken.line) +
				"): Излишни символи след края на програмата.");
		}

		// След като всичко е парснато, извеждаме предупрежденията за неизползваните декларации
		symTable.printUnusedWarnings();
	}

private:
	// decl_part -> { decl }
	void parseDeclPart() {
		while (currentToken.type == TokenType::KEYWORD &&
			(currentToken.lexeme == "int" || currentToken.lexeme == "shared" || currentToken.lexeme == "sem")) {
			try {
				parseDecl();
			}
			catch (const runtime_error& e) {
				cerr << e.what() << endl;
				synchronize();
			}
		}
	}

	// decl -> "int" idlist ";" | "shared" idlist ";" | "sem" idlist ";"
	void parseDecl() {
		SymbolClass currentClass = SymbolClass::UNDEFINED;
		if (currentToken.lexeme == "int") {
			currentClass = SymbolClass::PRIVATE_CLASS;
			match(TokenType::KEYWORD, "int");
		}
		else if (currentToken.lexeme == "shared") {
			currentClass = SymbolClass::SHARED;
			match(TokenType::KEYWORD, "shared");
		}
		else if (currentToken.lexeme == "sem") {
			currentClass = SymbolClass::SEM;
			match(TokenType::KEYWORD, "sem");
		}

		parseIdList(currentClass); // Предаваме класа памет надолу
		match(TokenType::DELIMITER, ";");
	}

	// idlist -> Ident { "," Ident }
	void parseIdList(SymbolClass currentClass) {
		Token idToken = currentToken;
		match(TokenType::IDENTIFIER);
		declareSymbol(idToken, currentClass); // Семантика: Декларираме символа

		while (currentToken.type == TokenType::DELIMITER && currentToken.lexeme == ",") {
			match(TokenType::DELIMITER, ",");
			idToken = currentToken;
			match(TokenType::IDENTIFIER);
			declareSymbol(idToken, currentClass); // Семантика: Декларираме следващия
		}
	}

	// stm_list -> { stm }
	void parseStmList() {
		while (currentToken.type == TokenType::IDENTIFIER ||
			(currentToken.type == TokenType::KEYWORD &&
				(currentToken.lexeme == "if" || currentToken.lexeme == "while" ||
					currentToken.lexeme == "for" || currentToken.lexeme == "xin" ||
					currentToken.lexeme == "xout" || currentToken.lexeme == "init" ||
					currentToken.lexeme == "wait" || currentToken.lexeme == "signal"))) {
			try {
				parseStm();
			}
			catch (const runtime_error& e) {
				cerr << e.what() << endl;
				synchronize();
			}
		}
	}

	// stm -> assign_stm ";" | if_stm | while_stm | for_stm | io_stm | sync_stm ";"
	void parseStm() {
		if (currentToken.type == TokenType::IDENTIFIER) {
			parseAssignStm();
			match(TokenType::DELIMITER, ";");
		}
		else if (currentToken.lexeme == "if") {
			parseIfStm();
		}
		else if (currentToken.lexeme == "while") {
			parseWhileStm();
		}
		else if (currentToken.lexeme == "for") {
			parseForStm();
		}
		else if (currentToken.lexeme == "xin" || currentToken.lexeme == "xout") {
			parseIoStm();
			match(TokenType::DELIMITER, ";");
		}
		else if (currentToken.lexeme == "init" || currentToken.lexeme == "wait" || currentToken.lexeme == "signal") {
			parseSyncStm();
			match(TokenType::DELIMITER, ";");
		}
	}

	// assign_stm -> Ident "=" expr
	void parseAssignStm() {
		Token idToken = currentToken;
		match(TokenType::IDENTIFIER);
		checkVarUsage(idToken); // Семантика: Проверка дали променливата може да бъде записвана

		match(TokenType::OPERATOR, "=");
		parseExpr();
	}

	// if_stm -> "if" "(" expr ")" "{" stm_list "}" [ "else" "{" stm_list "}" ]
	void parseIfStm() {
		match(TokenType::KEYWORD, "if");
		match(TokenType::DELIMITER, "(");
		parseExpr();
		match(TokenType::DELIMITER, ")");

		match(TokenType::DELIMITER, "{");
		parseStmList();
		match(TokenType::DELIMITER, "}");

		if (currentToken.type == TokenType::KEYWORD && currentToken.lexeme == "else") {
			match(TokenType::KEYWORD, "else");
			match(TokenType::DELIMITER, "{");
			parseStmList();
			match(TokenType::DELIMITER, "}");
		}
	}

	// while_stm -> "while" "(" expr ")" "{" stm_list "}"
	void parseWhileStm() {
		match(TokenType::KEYWORD, "while");
		match(TokenType::DELIMITER, "(");
		parseExpr();
		match(TokenType::DELIMITER, ")");

		match(TokenType::DELIMITER, "{");
		parseStmList();
		match(TokenType::DELIMITER, "}");
	}

	// for_stm -> "for" "(" assign_stm ";" expr ";" assign_stm ")" "{" stm_list "}"
	void parseForStm() {
		match(TokenType::KEYWORD, "for");
		match(TokenType::DELIMITER, "(");
		parseAssignStm();
		match(TokenType::DELIMITER, ";");
		parseExpr();
		match(TokenType::DELIMITER, ";");
		parseAssignStm();
		match(TokenType::DELIMITER, ")");

		match(TokenType::DELIMITER, "{");
		parseStmList();
		match(TokenType::DELIMITER, "}");
	}

	// io_stm -> "xin" "(" Ident ")" | "xout" "(" expr ")" | "xout" "(" String ")"
	void parseIoStm() {
		if (currentToken.lexeme == "xin") {
			match(TokenType::KEYWORD, "xin");
			match(TokenType::DELIMITER, "(");
			Token idToken = currentToken;
			match(TokenType::IDENTIFIER);
			checkVarUsage(idToken); // Семантика: Проверка на аргумента на xin
			match(TokenType::DELIMITER, ")");
		}
		else if (currentToken.lexeme == "xout") {
			match(TokenType::KEYWORD, "xout");
			match(TokenType::DELIMITER, "(");
			if (currentToken.type == TokenType::STRING_LITERAL) {
				match(TokenType::STRING_LITERAL);
			}
			else {
				parseExpr();
			}
			match(TokenType::DELIMITER, ")");
		}
	}

	// Изразите (Expression parsing rules)
	void parseExpr() {
		parseAndExpr();
		while (currentToken.lexeme == "or") {
			match(TokenType::KEYWORD, "or");
			parseAndExpr();
		}
	}

	void parseAndExpr() {
		parseRelExpr();
		while (currentToken.lexeme == "and") {
			match(TokenType::KEYWORD, "and");
			parseRelExpr();
		}
	}

	void parseRelExpr() {
		parseAddExpr();
		if (currentToken.type == TokenType::OPERATOR &&
			(currentToken.lexeme == "<" || currentToken.lexeme == "<=" ||
				currentToken.lexeme == ">" || currentToken.lexeme == ">=" ||
				currentToken.lexeme == "?=" || currentToken.lexeme == "@=")) {
			match(TokenType::OPERATOR);
			parseAddExpr();
		}
	}

	void parseAddExpr() {
		parseTerm();
		while (currentToken.lexeme == "+" || currentToken.lexeme == "-") {
			match(TokenType::OPERATOR);
			parseTerm();
		}
	}

	void parseTerm() {
		parseFactor();
		while (currentToken.lexeme == "*" || currentToken.lexeme == "/") {
			match(TokenType::OPERATOR);
			parseFactor();
		}
	}

	void parseFactor() {
		if (currentToken.type == TokenType::IDENTIFIER) {
			Token idToken = currentToken;
			match(TokenType::IDENTIFIER);
			checkVarUsage(idToken); // Семантика: Проверка при четене
		}
		else if (currentToken.type == TokenType::NUMBER) {
			match(TokenType::NUMBER);
		}
		else if (currentToken.lexeme == "(") {
			match(TokenType::DELIMITER, "(");
			parseExpr();
			match(TokenType::DELIMITER, ")");
		}
		else if (currentToken.lexeme == "not") {
			match(TokenType::KEYWORD, "not");
			parseFactor();
		}
		else if (currentToken.lexeme == "-") {
			match(TokenType::OPERATOR, "-");
			parseFactor();
		}
		else {
			throw runtime_error("Синтактична грешка (Ред " + to_string(currentToken.line) +
				", Колона " + to_string(currentToken.col) + "): Невалиден израз.");
		}
	}

	void parseSyncStm() {
		if (currentToken.lexeme == "init") {
			match(TokenType::KEYWORD, "init");
			match(TokenType::DELIMITER, "(");
			Token idToken = currentToken;
			match(TokenType::IDENTIFIER);
			checkSemUsage(idToken); // Семантика: Проверка дали е семафор
			match(TokenType::DELIMITER, ",");
			parseExpr();
			match(TokenType::DELIMITER, ")");
		}
		else if (currentToken.lexeme == "wait") {
			match(TokenType::KEYWORD, "wait");
			match(TokenType::DELIMITER, "(");
			Token idToken = currentToken;
			match(TokenType::IDENTIFIER);
			checkSemUsage(idToken); // Семантика: Проверка дали е семафор
			match(TokenType::DELIMITER, ")");
		}
		else if (currentToken.lexeme == "signal") {
			match(TokenType::KEYWORD, "signal");
			match(TokenType::DELIMITER, "(");
			Token idToken = currentToken;
			match(TokenType::IDENTIFIER);
			checkSemUsage(idToken); // Семантика: Проверка дали е семафор
			match(TokenType::DELIMITER, ")");
		}
	}
};

int main() {
	system("chcp 1251 > nul");

	string filename;

	while (true) {
		cout << "\nLinklang Compiler" << endl;
		cout << "--------------------------------------------" << endl << endl;
		cout << "Enter <source.txt> (empty or exit to quit): ";

		getline(cin, filename);

		// Премахване на евентуални излишни интервали в началото и края
		filename.erase(0, filename.find_first_not_of(" \t\r\n"));
		filename.erase(filename.find_last_not_of(" \t\r\n") + 1);

		// Условие за изход от програмата
		if (filename.empty() || filename == "exit") {
			break;
		}

		// Инстанцираме таблицата ТУК, за да е празна за всеки нов файл
		SymbolTable symTable;

		try {
			// Създаване на Lexer и Parser вътре в try блока
			Lexer lexer(filename, symTable);
			Parser parser(lexer, symTable);

			cout << "Стартиране на синтактичен анализ..." << endl;
			parser.parseProgram();

			if (parser.hadError()) {
				cout << "\nГРЕШКА: Синтактичният анализ приключи с грешки. Междинен код няма да се генерира." << endl;
			}
			else {
				cout << "\nУСПЕХ: Синтактичният анализ приключи без грешки!" << endl;
				symTable.printUnusedWarnings(); // Проверка за неизползвани променливи
				symTable.printTable();
			}

		}
		catch (const runtime_error& e) {
			// Прихваща критични грешки (напр. липсващ файл) и продължава цикъла
			cerr << "\nКРИТИЧНО ПРЕКЪСВАНЕ: " << e.what() << endl;
		}
	}

	return 0;
}