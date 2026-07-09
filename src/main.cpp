#include <iostream>
#include <fstream>
#include "ast.h"
#include "semantic.h"
#include "tac.h"
#include "z80gen.h"

using namespace std;

extern int yyparse();
extern Bloque* program_root;
extern FILE* yyin;
extern int lex_errors;
extern int parse_errors; // Contador de errores sintácticos recuperados

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <archivo.cmini>\n";
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        cerr << "No se pudo abrir el archivo " << argv[1] << "\n";
        return 1;
    }

    yyin = file;
    cout << "[1/4] Análisis léxico y sintáctico...\n";
    yyparse(); // No abortamos de inmediato: el error recovery permite continuar

    fclose(file);

    if (lex_errors > 0 || parse_errors > 0) {
        cerr << "Se encontraron " << (lex_errors + parse_errors)
             << " error(es) léxico/sintáctico(s). Deteniendo compilación.\n";
        return 1;
    }

    cout << "[2/4] Análisis semántico...\n";
    AnalizadorSemantico analizador;
    analizador.analizar(program_root);
    if (analizador.tieneErrores()) {
        cerr << "Se encontraron errores semánticos. Deteniendo compilación.\n";
        return 1;
    }

    cout << "[3/4] Generando Código Intermedio (TAC)...\n";
    GeneradorTAC generadorTac;
    generadorTac.generar(program_root);

    cout << "[4/4] Generando Código Z80 con Stack Frames...\n";
    GeneradorZ80 generadorZ80(generadorTac.getInstructions());

    string outFilename = "output.asm";
    ofstream outFile(outFilename);
    generadorZ80.generar(outFile);
    outFile.close();

    cout << "Compilación exitosa. Archivo generado: " << outFilename << "\n";

    delete program_root;
    return 0;
}
