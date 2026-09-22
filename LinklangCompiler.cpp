#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

// Изброен тип за класовете памет
enum class SymbolClass {
    PRIVATE_CLASS,
    SHARED,
    SEM,
    CONST_CLASS,
    TEMP,
    UNDEFINED      // default value
};

// Структура на записа в символната таблица
struct SymbolRecord {
    string symbol;
    int code;
    SymbolClass cls;
    int val;
    int addr;
};

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

    // Помощна функция за извличане на запис по неговата позиция (за нуждите на семантичния анализатор и генератора на код)
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
};

// Пример за използване
int main() {
    system("chcp 1251 > nul");

    SymbolTable symTable;

    // Добавяне на променлива var1 от тип PRIVATE
    int pos1 = symTable.getOrInsert("var1", 101, SymbolClass::PRIVATE_CLASS, 0, 1);

    // Търсене на същата променлива var1
    int pos2 = symTable.getOrInsert("var1");

    // Добавяне на константа const1
    int pos3 = symTable.getOrInsert("const1", 102, SymbolClass::CONST_CLASS, 123, 3);

    // Търсене на същата константа const1
    int pos4 = symTable.getOrInsert("const1");

    // Добавяне на променлива var2 от тип PRIVATE
    int pos5 = symTable.getOrInsert("var2", 103, SymbolClass::PRIVATE_CLASS, 0, 5);

    cout << "Позиция на променливата var1: " << pos1 << endl;
    cout << "Позиция при повторно търсене на var1: " << pos2 << endl;
    cout << "Позиция на константата const1: " << pos3 << endl;
    cout << "Позиция при повторно търсене на const1: " << pos4 << endl;
    cout << "Позиция на променливата var2: " << pos5 << endl;

    return 0;
}