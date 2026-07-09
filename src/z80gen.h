#pragma once
#include "tac.h"
#include <vector>
#include <string>
#include <set>
#include <map>
#include <ostream>
#include <sstream>

using namespace std;

struct ArrayInfo { int elemSize; int count; };

class GeneradorZ80 {
private:
    vector<TACInstr> tac;

    // BSS
    set<string>          variables;  // Variables globales escalares
    map<string,ArrayInfo> arrayDecls; // Arreglos globales

    // Estado del frame de función actual
    bool             inFunction;
    int              currentFrameSize;
    string           currentFuncName;  // nombre de la función activa (para epílogo)
    map<string,int>  localVarMap; // varName → desplazamiento IX (byte bajo)

    // Contadores para etiquetas únicas internas
    int cmpCounter, notCounter, mulCounter;

    void extraerVariables();
    bool esNumero(const string& s);
    string fmtIX(int d);           // Formatea "IX+d" o "IX-d"

    // Carga arg en HL (maneja literales, locales IX y globales)
    void cargarHL(const string& arg, ostream& out);
    // Carga arg en DE
    void cargarDE(const string& arg, ostream& out);
    // Guarda HL en dest (maneja locales IX y globales)
    void guardarHL(const string& dest, ostream& out);

public:
    GeneradorZ80(const vector<TACInstr>& t);
    void generar(ostream& out);
};
