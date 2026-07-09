#pragma once
#include "tac.h"
#include <vector>
#include <string>
#include <set>
#include <ostream>

using namespace std;

class GeneradorZ80 {
private:
    vector<TACInstr> tac;
    set<string> variables;
    
    void extraerVariables();
    bool esNumero(const string& s);
    
    // Helpers
    void cargarHL(const string& arg, ostream& out);
    void cargarDE(const string& arg, ostream& out);

public:
    GeneradorZ80(const vector<TACInstr>& t);
    void generar(ostream& out);
};
