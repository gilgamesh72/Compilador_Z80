#pragma once
#include "tac.h"
#include <vector>
#include <string>
#include <set>
#include <ostream>

using namespace std;

class Z80Generator {
private:
    vector<TACInstr> tac;
    set<string> variables;
    
    void extractVariables();
    bool isNumber(const string& s);
    
    // Helpers
    void loadHL(const string& arg, ostream& out);
    void loadDE(const string& arg, ostream& out);

public:
    Z80Generator(const vector<TACInstr>& t);
    void generate(ostream& out);
};
