#include <iostream>
#include <fstream>
#include "ast.h"
#include "semantic.h"
#include "tac.h"
#include "z80gen.h"

using namespace std;

// Variables y funciones externas de Flex/Bison
extern int yyparse();
extern Bloque* program_root;
extern FILE* yyin;
extern int lex_errors;

// Registra los arreglos en el generador Z80
static void registrarArreglos(Bloque* bloque, GeneradorZ80& gen) {
    if (!bloque) return;
    for (Sentencia* s : bloque->statements) {
        // Arreglo directo
        if (auto* da = dynamic_cast<DeclaracionArreglo*>(s)) {
            int elemBytes = (da->baseType == DataType::BYTE ||
                             da->baseType == DataType::CHAR) ? 1 : 2;
            gen.registrarArreglo(da->name, da->size * elemBytes);
        }
        // Buscar dentro de cuerpo de función
        else if (auto* df = dynamic_cast<DeclaracionFuncion*>(s)) {
            registrarArreglos(df->body, gen);
        }
        // Buscar dentro de bloques anidados (if/while/for)
        else if (auto* si = dynamic_cast<SentenciaSi*>(s)) {
            registrarArreglos(si->thenBlock, gen);
            registrarArreglos(si->elseBlock, gen);
        }
        else if (auto* sw = dynamic_cast<SentenciaMientras*>(s)) {
            registrarArreglos(sw->body, gen);
        }
        else if (auto* sp = dynamic_cast<SentenciaPara*>(s)) {
            registrarArreglos(sp->body, gen);
        }
    }
}

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
        cerr << "Deteniendo compilación por errores léxico/sintácticos.\n";
        fclose(file);
        return 1;
    }

    cout << "[2/4] Ejecutando análisis semántico...\n";
    AnalizadorSemantico analizador;
    analizador.analizar(program_root);

    if (analizador.tieneErrores()) {
        cerr << "Se encontraron errores semánticos. Deteniendo compilación.\n";
        delete program_root;
        fclose(file);
        return 1;
    }

    cout << "[3/4] Generando Código Intermedio (TAC)...\n";
    GeneradorTAC generadorTac;
    generadorTac.generar(program_root);

    cout << "[4/4] Generando Código Z80...\n";
    GeneradorZ80 generadorZ80(generadorTac.getInstructions());

    // Registrar arreglos para que el generador reserve el espacio correcto
    registrarArreglos(program_root, generadorZ80);

    // Determinar nombre del archivo de salida
    string outFilename = "output.asm";
    if (argc >= 3) outFilename = argv[2];

    ofstream outFile(outFilename);
    if (!outFile) {
        cerr << "No se pudo crear el archivo de salida " << outFilename << "\n";
        delete program_root;
        fclose(file);
        return 1;
    }

    generadorZ80.generar(outFile);
    outFile.close();

    cout << "Compilación finalizada exitosamente.\n";
    cout << "Archivo generado: " << outFilename << "\n";

    delete program_root;
    fclose(file);
    return 0;
}
