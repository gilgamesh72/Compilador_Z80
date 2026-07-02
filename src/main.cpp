#include <iostream>
#include <fstream>
#include "ast.h"
#include "semantic.h"
#include "tac.h"
#include "z80gen.h"

using namespace std;

// Variables y funciones externas de Flex/Bison
extern int yyparse();
extern Block* program_root;
extern FILE* yyin;
extern int lex_errors;

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
    cout << "[1/4] Ejecutando análisis léxico y sintáctico...\n";
    if (yyparse() != 0 || lex_errors > 0) {
        cerr << "Deteniendo compilación...\n";
        return 1;
    }

    cout << "[2/4] Ejecutando análisis semántico...\n";
    SemanticAnalyzer semantic;
    semantic.analyze(program_root);
    
    if (semantic.hasErrors()) {
        cerr << "Se encontraron errores semánticos. Deteniendo compilación.\n";
        return 1;
    }

    cout << "[3/4] Generando Código Intermedio (TAC)...\n";
    TACGenerator tacGen;
    tacGen.generate(program_root);
    


    cout << "[4/4] Generando Código Z80...\n";
    Z80Generator z80Gen(tacGen.getInstructions());
    
    string outFilename = "output.asm";
    ofstream outFile(outFilename);
    z80Gen.generate(outFile);
    outFile.close();

    cout << "Compilación finalizada exitosamente. Archivo generado: " << outFilename << "\n";

    delete program_root;
    fclose(file);
    return 0;
}
