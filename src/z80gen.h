#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <ostream>
#include "tac.h"

using namespace std;

class GeneradorZ80 {
private:
    const vector<TACInstr>& tac;

    // Conjuntos de variables/arreglos/strings descubiertos en el TAC
    set<string>         variables;   // Variables simples (DEFW)
    map<string,int>     arrays;      // nombre → bytes totales  (DEFS)
    map<string,string>  strings;     // etiqueta → contenido (DEFM)

    // ── Soporte de funciones ────────────────────────────────
    set<string>     allParamVars;        // Nombres de params (no van a DEFW global)
    map<string,int> currentParamOffsets; // nombre → offset IX de la función actual
    bool            inFunction;          // true si estamos generando código de función
    bool            lastWasReturn;       // true si la última instrucción fue RETURN (evita doble epilogo)

    int cmpCounter;  // Para etiquetas de comparación únicas

    void extraerVariables();

    // Helpers de carga
    void cargarHL(const string& arg, ostream& out);
    void cargarDE(const string& arg, ostream& out);
    void cargarA (const string& arg, ostream& out); // Carga byte bajo en A

    // Helper de almacenamiento (soporta vars globales y params via IX)
    void almacenarHL(const string& result, ostream& out);

    // Busca offset IX para un param, manejando el prefijo _v_ de safeName()
    int  rawParamOffset(const string& arg) const;

    // Emite las rutinas software de MUL/DIV/MOD al final del ASM
    void emitirRutinasMath(ostream& out);
    // Emite la sección de datos (variables, arreglos, strings)
    void emitirSeccionDatos(ostream& out);

public:
    GeneradorZ80(const vector<TACInstr>& t);

    // Informa al generador el tamaño en bytes de cada arreglo
    // (llamado desde main() tras el análisis semántico)
    void registrarArreglo(const string& name, int bytes);

    void generar(ostream& out);
};
