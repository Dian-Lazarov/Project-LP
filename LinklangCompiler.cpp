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
};

// --- СИМВОЛНА ТАБЛИЦА ---
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
};

// --- ЛЕКСИКАЛЕН АНАЛИЗАТОР ---
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
            cerr << "Грешка: Файлът " << filename << " не може да бъде отворен!" << endl;
            return;
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
            if (c == '"') {
                string strLit = "";
                strLit += advance(); // Консумираме началното "
                while (peek() != '"' && peek() != '\0') {
                    strLit += advance();
                }
                if (peek() == '"') {
                    strLit += advance(); // Консумираме крайното "
                }
                return { TokenType::STRING_LITERAL, strLit, startLine, startCol, -1 };
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

            // Непознат символ
            string err(1, advance());
            return { TokenType::ERROR_TOKEN, err, startLine, startCol, -1 };
        }

        return { TokenType::END_OF_FILE, "EOF", line, col, -1 };
    }
};

int main() {
    system("chcp 1251 > nul");

    SymbolTable symTable;

    // Подаваме името на текстовия файл и правим обръщение към символната таблица
    Lexer lexer("Linklang_Test.txt", symTable);

    cout << "=== Токени ===" << endl;
    Token token;
    do {
        token = lexer.getNextToken();

        cout << "Ред " << token.line << ", Колона " << token.col << " \t";

        switch (token.type) {
        case TokenType::KEYWORD: cout << "КЛЮЧОВА ДУМА | " << token.lexeme; break;
        case TokenType::IDENTIFIER: cout << "ИДЕНТИФИКАТОР | " << token.lexeme << " (Поз. в табл: " << token.symTablePos << ")"; break;
        case TokenType::NUMBER: cout << "ЧИСЛО | " << token.lexeme << " (Поз. в табл: " << token.symTablePos << ")"; break;
        case TokenType::STRING_LITERAL: cout << "ТЕКСТ | " << token.lexeme; break;
        case TokenType::OPERATOR: cout << "ОПЕРАТОР | " << token.lexeme; break;
        case TokenType::DELIMITER: cout << "РАЗДЕЛИТЕЛ | " << token.lexeme; break;
        case TokenType::COMMENT: cout << "КОМЕНТАР | " << token.lexeme; break;
        case TokenType::ERROR_TOKEN: cout << "ГРЕШКА | Непознат символ: " << token.lexeme; break;
        case TokenType::END_OF_FILE: cout << "КРАЙ НА ФАЙЛА"; break;
        }
        cout << endl;

    } while (token.type != TokenType::END_OF_FILE);

    // Отпечатваме състоянието на символната таблица след лексикалния анализ
    symTable.printTable();

    return 0;
}