#include "semantic.h"
#include <iostream>

using namespace std;

// ============================================================
//  Constructor / Destructor
// ============================================================

AnalizadorSemantico::AnalizadorSemantico()
    : currentEnv(new SymbolTable()), hasError(false),
      currentReturnType(DataType::VOID), insideFunction(false) {}

AnalizadorSemantico::~AnalizadorSemantico() {
    while (currentEnv) {
        SymbolTable* parent = currentEnv->parent;
        delete currentEnv;
        currentEnv = parent;
    }
}

void AnalizadorSemantico::analizar(NodoAST* root) {
    if (root) root->aceptar(*this);
}

// ============================================================
//  Helpers
// ============================================================

bool AnalizadorSemantico::esTipoDe8Bits(DataType type) {
    return type == DataType::BYTE || type == DataType::CHAR;
}

bool AnalizadorSemantico::esFueraDeRangoByte(Expresion* expr) {
    ExprNumero* num = dynamic_cast<ExprNumero*>(expr);
    return num && (num->value < 0 || num->value > 255);
}

// ============================================================
//  Expresiones
// ============================================================

void AnalizadorSemantico::visitar(ExprNumero* node) {
    node->exprType = DataType::INT;
}

void AnalizadorSemantico::visitar(ExprCaracter* node) {
    node->exprType = DataType::CHAR;
}

void AnalizadorSemantico::visitar(ExprString* node) {
    // Un literal de cadena es un puntero de 16 bits (STRING)
    node->exprType = DataType::STRING;
}

void AnalizadorSemantico::visitar(ExprIdentificador* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Variable no declarada '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
    } else {
        node->exprType = sym->type;
    }
}

void AnalizadorSemantico::visitar(ExprLeerMemoria* node) {
    if (node->address) node->address->aceptar(*this);
    node->exprType = DataType::BYTE; // peek() devuelve un byte
}

void AnalizadorSemantico::visitar(ExprBinaria* node) {
    if (node->left)  node->left->aceptar(*this);
    if (node->right) node->right->aceptar(*this);

    DataType l = node->left  ? node->left->exprType  : DataType::UNKNOWN;
    DataType r = node->right ? node->right->exprType : DataType::UNKNOWN;

    if (l == DataType::UNKNOWN || r == DataType::UNKNOWN) {
        node->exprType = DataType::UNKNOWN;
        return;
    }

    const string& op = node->op;

    // Operadores relacionales y lógicos → resultado booleano (BYTE)
    if (op == "<" || op == ">" || op == "<=" || op == ">=" ||
        op == "==" || op == "!=" || op == "&&" || op == "||") {
        node->exprType = DataType::BYTE;
        return;
    }

    // Operadores bitwise → permitidos solo en tipos enteros
    if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
        if (l == DataType::STRING || r == DataType::STRING) {
            cerr << "Error semántico: Operación bitwise no válida con tipo STRING\n";
            hasError = true;
            node->exprType = DataType::UNKNOWN;
            return;
        }
        // El resultado tiene el tipo más ancho
        node->exprType = (l == DataType::INT || r == DataType::INT)
                          ? DataType::INT : DataType::BYTE;
        return;
    }

    // Aritméticos: +, -, *, /, %
    if (l == DataType::STRING || r == DataType::STRING) {
        cerr << "Error semántico: Operación aritmética no válida con tipo STRING\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }

    // Promoción: INT prevalece
    if (l == DataType::INT || r == DataType::INT) {
        node->exprType = DataType::INT;
    } else if (l == DataType::CHAR || r == DataType::CHAR) {
        node->exprType = DataType::CHAR;
    } else {
        node->exprType = DataType::BYTE;
    }
}

void AnalizadorSemantico::visitar(ExprUnaria* node) {
    if (node->operand) node->operand->aceptar(*this);

    DataType t = node->operand ? node->operand->exprType : DataType::UNKNOWN;

    if (node->op == "!") {
        // Negación lógica → BYTE (booleano)
        if (t == DataType::STRING) {
            cerr << "Error semántico: '!' no se puede aplicar a STRING\n";
            hasError = true;
            node->exprType = DataType::UNKNOWN;
        } else {
            node->exprType = DataType::BYTE;
        }
    } else if (node->op == "~") {
        // Complemento a nivel de bits
        if (t == DataType::STRING || t == DataType::UNKNOWN) {
            cerr << "Error semántico: '~' requiere tipo entero\n";
            hasError = true;
            node->exprType = DataType::UNKNOWN;
        } else {
            node->exprType = t; // Conserva el tipo
        }
    } else if (node->op == "-") {
        // Negación aritmética
        node->exprType = (t == DataType::INT) ? DataType::INT : DataType::BYTE;
    } else {
        node->exprType = t;
    }
}

void AnalizadorSemantico::visitar(ExprAccesoArreglo* node) {
    // Verificar que el símbolo existe y es un arreglo
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Arreglo no declarado '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }
    if (!sym->isArray) {
        cerr << "Error semántico: '" << node->name << "' no es un arreglo\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }

    // Verificar el índice
    if (node->index) node->index->aceptar(*this);

    // Verificar límites si el índice es un literal numérico
    if (node->index) {
        ExprNumero* lit = dynamic_cast<ExprNumero*>(node->index);
        if (lit) {
            if (lit->value < 0 || lit->value >= sym->arraySize) {
                cerr << "Error semántico: Índice " << lit->value
                     << " fuera de rango [0, " << (sym->arraySize - 1)
                     << "] para arreglo '" << node->name << "'\n";
                hasError = true;
            }
        }
    }

    // El tipo del acceso es el tipo base del arreglo
    node->exprType = sym->baseType;
}

void AnalizadorSemantico::visitar(ExprIn* node) {
    if (node->port) node->port->aceptar(*this);
    node->exprType = DataType::BYTE; // IN devuelve un byte
}

// ============================================================
//  NUEVO: Llamada a función como expresión
// ============================================================
void AnalizadorSemantico::visitar(ExprLlamadaFuncion* node) {
    // Verificar que la función está declarada
    auto it = functionTable.find(node->name);
    if (it == functionTable.end()) {
        cerr << "Error semántico: Función no declarada '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }

    const FunctionInfo& info = it->second;

    // Verificar número de argumentos
    if (node->args.size() != info.params.size()) {
        cerr << "Error semántico: Función '" << node->name
             << "' espera " << info.params.size()
             << " argumento(s), se recibieron " << node->args.size() << "\n";
        hasError = true;
        node->exprType = info.returnType;
        return;
    }

    // Analizar cada argumento
    for (size_t i = 0; i < node->args.size(); i++) {
        node->args[i]->aceptar(*this);
    }

    node->exprType = info.returnType;
}

// ============================================================
//  Sentencias
// ============================================================

void AnalizadorSemantico::visitar(Bloque* node) {
    SymbolTable* newEnv = new SymbolTable(currentEnv);
    currentEnv = newEnv;

    for (auto stmt : node->statements) {
        stmt->aceptar(*this);
    }

    SymbolTable* old = currentEnv;
    currentEnv = currentEnv->parent;
    delete old;
}

void AnalizadorSemantico::visitar(DeclaracionVariable* node) {
    if (!currentEnv->insert(node->name, node->type)) {
        cerr << "Error semántico: Variable ya declarada '"
             << node->name << "' en el mismo scope\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(DeclaracionArreglo* node) {
    if (node->size <= 0) {
        cerr << "Error semántico: Tamaño de arreglo debe ser > 0 para '"
             << node->name << "'\n";
        hasError = true;
        return;
    }
    if (!currentEnv->insertArray(node->name, node->baseType, node->size)) {
        cerr << "Error semántico: Arreglo ya declarado '"
             << node->name << "' en el mismo scope\n";
        hasError = true;
    }
}

// ============================================================
//  NUEVO: Declaración de función
// ============================================================
void AnalizadorSemantico::visitar(DeclaracionFuncion* node) {
    // Verificar que no esté ya declarada
    if (functionTable.count(node->name)) {
        cerr << "Error semántico: Función ya declarada '" << node->name << "'\n";
        hasError = true;
        return;
    }

    // Registrar en la tabla de funciones
    FunctionInfo info;
    info.returnType = node->returnType;
    for (auto p : node->params) {
        info.params.push_back({p->type, p->name});
    }
    functionTable[node->name] = info;

    // Guardar contexto anterior y entrar en la función
    DataType prevReturnType = currentReturnType;
    bool prevInsideFunction  = insideFunction;
    currentReturnType = node->returnType;
    insideFunction    = true;

    // Crear scope nuevo para los parámetros
    SymbolTable* funcEnv = new SymbolTable(currentEnv);
    currentEnv = funcEnv;

    for (auto p : node->params) {
        if (!currentEnv->insert(p->name, p->type)) {
            cerr << "Error semántico: Parámetro duplicado '"
                 << p->name << "' en función '" << node->name << "'\n";
            hasError = true;
        }
    }

    // Analizar el cuerpo (Bloque no crea scope nuevo cuando ya existe)
    if (node->body) {
        for (auto stmt : node->body->statements) {
            stmt->aceptar(*this);
        }
    }

    // Restaurar scope y contexto
    SymbolTable* old = currentEnv;
    currentEnv = currentEnv->parent;
    delete old;

    currentReturnType = prevReturnType;
    insideFunction    = prevInsideFunction;
}

void AnalizadorSemantico::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);

    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Asignación a variable no declarada '"
             << node->name << "'\n";
        hasError = true;
        return;
    }
    if (sym->isArray) {
        cerr << "Error semántico: No se puede asignar directamente al arreglo '"
             << node->name << "' sin índice\n";
        hasError = true;
        return;
    }
    if (node->value && node->value->exprType == DataType::VOID) {
        cerr << "Error semántico: No se puede asignar VOID a '"
             << node->name << "'\n";
        hasError = true;
    }
    if (node->value && esTipoDe8Bits(sym->type) && esFueraDeRangoByte(node->value)) {
        ExprNumero* num = dynamic_cast<ExprNumero*>(node->value);
        cerr << "Error semántico: Valor " << num->value
             << " fuera del rango 8 bits para '" << node->name << "'\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(AsignacionArreglo* node) {
    // Verificar que el arreglo existe
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Arreglo no declarado '"
             << node->name << "'\n";
        hasError = true;
        return;
    }
    if (!sym->isArray) {
        cerr << "Error semántico: '"
             << node->name << "' no es un arreglo\n";
        hasError = true;
        return;
    }

    // Analizar índice y valor
    if (node->index) node->index->aceptar(*this);
    if (node->value) node->value->aceptar(*this);

    // Verificar límites si el índice es literal
    if (node->index) {
        ExprNumero* lit = dynamic_cast<ExprNumero*>(node->index);
        if (lit && (lit->value < 0 || lit->value >= sym->arraySize)) {
            cerr << "Error semántico: Índice " << lit->value
                 << " fuera de rango [0, " << (sym->arraySize - 1)
                 << "] para arreglo '" << node->name << "'\n";
            hasError = true;
        }
    }
}

void AnalizadorSemantico::visitar(SentenciaSi* node) {
    if (node->condition) node->condition->aceptar(*this);
    if (node->thenBlock) node->thenBlock->aceptar(*this);
    if (node->elseBlock) node->elseBlock->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaMientras* node) {
    if (node->condition) node->condition->aceptar(*this);
    if (node->body)      node->body->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaPara* node) {
    // El init puede declarar variables → crear un scope nuevo
    SymbolTable* newEnv = new SymbolTable(currentEnv);
    currentEnv = newEnv;

    if (node->init)      node->init->aceptar(*this);
    if (node->condition) node->condition->aceptar(*this);
    if (node->step)      node->step->aceptar(*this);
    if (node->body)      node->body->aceptar(*this);

    SymbolTable* old = currentEnv;
    currentEnv = currentEnv->parent;
    delete old;
}

void AnalizadorSemantico::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value)   node->value->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaOut* node) {
    if (node->port)  node->port->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
    // OUT (C), A usa solo el byte bajo — el truncamiento es implícito y correcto en Z80
}

void AnalizadorSemantico::visitar(SentenciaRetorno* node) {
    if (node->value) node->value->aceptar(*this);

    // Validar tipo de retorno si estamos dentro de una función
    if (insideFunction) {
        if (currentReturnType == DataType::VOID && node->value) {
            cerr << "Error semántico: Función void no puede retornar un valor\n";
            hasError = true;
        } else if (currentReturnType != DataType::VOID && !node->value) {
            cerr << "Error semántico: Función no-void debe retornar un valor\n";
            hasError = true;
        }
    }
}

// ============================================================
//  NUEVO: Llamada a función como sentencia
// ============================================================
void AnalizadorSemantico::visitar(SentenciaLlamadaFuncion* node) {
    // Verificar que la función está declarada
    auto it = functionTable.find(node->name);
    if (it == functionTable.end()) {
        cerr << "Error semántico: Función no declarada '" << node->name << "'\n";
        hasError = true;
        return;
    }

    const FunctionInfo& info = it->second;

    // Verificar número de argumentos
    if (node->args.size() != info.params.size()) {
        cerr << "Error semántico: Función '" << node->name
             << "' espera " << info.params.size()
             << " argumento(s), se recibieron " << node->args.size() << "\n";
        hasError = true;
        return;
    }

    // Analizar cada argumento
    for (size_t i = 0; i < node->args.size(); i++) {
        node->args[i]->aceptar(*this);
    }
}
