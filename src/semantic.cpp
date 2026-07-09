#include "semantic.h"
#include <iostream>

using namespace std;

AnalizadorSemantico::AnalizadorSemantico()
    : currentEnv(new SymbolTable()), hasError(false),
      inFunction(false), currentFunctionReturnType(DataType::VOID) {}

AnalizadorSemantico::~AnalizadorSemantico() {
    while (currentEnv) {
        SymbolTable* p = currentEnv->parent;
        delete currentEnv;
        currentEnv = p;
    }
}

void AnalizadorSemantico::analizar(NodoAST* root) {
    if (root) root->aceptar(*this);
}

// ---------------------------------------------------------------------------
// Helpers estáticos / privados
// ---------------------------------------------------------------------------

bool AnalizadorSemantico::esExpresionBooleana(Expresion* expr) {
    if (auto b = dynamic_cast<ExprBinaria*>(expr)) {
        char op = b->op;
        return op == '<' || op == '>' || op == 'E' || op == 'N'
            || op == 'L' || op == 'G' || op == 'A' || op == 'O';
    }
    if (auto u = dynamic_cast<ExprUnaria*>(expr)) return u->op == '!';
    return false;
}

bool AnalizadorSemantico::sentenciaRetorna(Sentencia* stmt) {
    if (dynamic_cast<SentenciaRetorno*>(stmt)) return true;
    if (auto ifs = dynamic_cast<SentenciaSi*>(stmt)) {
        return ifs->elseBlock != nullptr
            && todosLosCaminosRetornan(ifs->thenBlock)
            && todosLosCaminosRetornan(ifs->elseBlock);
    }
    return false;
}

bool AnalizadorSemantico::todosLosCaminosRetornan(Bloque* block) {
    if (!block) return false;
    for (auto it = block->statements.rbegin(); it != block->statements.rend(); ++it)
        if (sentenciaRetorna(*it)) return true;
    return false;
}

// Pre-scan: asigna resolvedOffset a cada DeclaracionVariable/Arreglo dentro
// de la función (incluyendo los declarados en bloques if/while anidados).
// Todos los locales comparten el mismo frame continuo (2 bytes por variable).
void AnalizadorSemantico::asignarOffsetsLocales(Bloque* block, int& offset) {
    if (!block) return;
    for (auto stmt : block->statements) {
        if (auto d = dynamic_cast<DeclaracionVariable*>(stmt)) {
            d->resolvedOffset = offset;
            offset += 2;
        } else if (auto a = dynamic_cast<DeclaracionArreglo*>(stmt)) {
            a->resolvedOffset = offset;
            offset += 2 * a->size;
        } else if (auto ifs = dynamic_cast<SentenciaSi*>(stmt)) {
            asignarOffsetsLocales(ifs->thenBlock, offset);
            asignarOffsetsLocales(ifs->elseBlock, offset);
        } else if (auto ws = dynamic_cast<SentenciaMientras*>(stmt)) {
            asignarOffsetsLocales(ws->body, offset);
        }
    }
}

// ---------------------------------------------------------------------------
// Visitantes de Expresiones
// ---------------------------------------------------------------------------

void AnalizadorSemantico::visitar(ExprNumero* node) {
    node->exprType = DataType::INT;
}

void AnalizadorSemantico::visitar(ExprCaracter* node) {
    node->exprType = DataType::CHAR;
}

static bool esFueraDeRangoByte(Expresion* e) {
    auto n = dynamic_cast<ExprNumero*>(e);
    return n && (n->value < 0 || n->value > 255);
}
static bool esTipoDe8Bits(DataType t) {
    return t == DataType::BYTE || t == DataType::CHAR;
}

void AnalizadorSemantico::visitar(ExprIdentificador* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Variable no declarada '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }
    // Advertencia de variable sin inicializar (solo en scope de función, no globals/params)
    if (inFunction && !sym->isParam && !sym->initialized) {
        cerr << "Advertencia semántica: La variable '" << node->name
             << "' podría usarse sin haber sido inicializada\n";
    }
    node->exprType = sym->type;
}

void AnalizadorSemantico::visitar(ExprLeerMemoria* node) {
    if (node->address) node->address->aceptar(*this);
    node->exprType = DataType::BYTE;
}

void AnalizadorSemantico::visitar(ExprBinaria* node) {
    if (node->left)  node->left->aceptar(*this);
    if (node->right) node->right->aceptar(*this);

    DataType l = node->left->exprType, r = node->right->exprType;
    if (l == DataType::UNKNOWN || r == DataType::UNKNOWN) {
        node->exprType = DataType::UNKNOWN; return;
    }
    char op = node->op;
    // Comparaciones y lógicos → resultado booleano (BYTE)
    if (op == '<' || op == '>' || op == 'E' || op == 'N' ||
        op == 'L' || op == 'G' || op == 'A' || op == 'O') {
        node->exprType = DataType::BYTE;
    } else {
        // Aritmética: promover al tipo más ancho
        if (l == DataType::INT || r == DataType::INT) node->exprType = DataType::INT;
        else if (l == DataType::CHAR || r == DataType::CHAR) node->exprType = DataType::CHAR;
        else node->exprType = DataType::BYTE;
    }
}

void AnalizadorSemantico::visitar(ExprUnaria* node) {
    if (node->operand) node->operand->aceptar(*this);
    DataType t = node->operand->exprType;
    if (node->op == '!')      node->exprType = DataType::BYTE; // booleano
    else if (node->op == '-') node->exprType = t;
    else node->exprType = DataType::UNKNOWN;
}

void AnalizadorSemantico::visitar(ExprLlamadaFuncion* node) {
    // Visitar argumentos primero
    for (auto arg : node->args) arg->aceptar(*this);

    auto it = functionTable.find(node->funcName);
    if (it == functionTable.end()) {
        cerr << "Error semántico: Función no declarada '" << node->funcName << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
        return;
    }

    const FunctionInfo& info = it->second;
    node->exprType = info.returnType;

    // Verificar cantidad de argumentos
    if (node->args.size() != info.paramTypes.size()) {
        cerr << "Error semántico: La función '" << node->funcName << "' espera "
             << info.paramTypes.size() << " argumento(s), se dieron "
             << node->args.size() << "\n";
        hasError = true;
        return;
    }

    // Verificar tipos de argumentos (con conversión numérica implícita)
    for (size_t i = 0; i < node->args.size(); i++) {
        DataType at = node->args[i]->exprType;
        DataType pt = info.paramTypes[i];
        bool numeric = (at == DataType::BYTE || at == DataType::CHAR || at == DataType::INT)
                    && (pt == DataType::BYTE || pt == DataType::CHAR || pt == DataType::INT);
        if (!numeric && at != pt && at != DataType::UNKNOWN) {
            cerr << "Error semántico: Argumento " << (i+1) << " de '" << node->funcName
                 << "' tiene tipo incompatible\n";
            hasError = true;
        }
    }
}

void AnalizadorSemantico::visitar(ExprAccesoArreglo* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Arreglo no declarado '" << node->name << "'\n";
        hasError = true; node->exprType = DataType::UNKNOWN; return;
    }
    if (!sym->isArray) {
        cerr << "Error semántico: '" << node->name << "' no es un arreglo\n";
        hasError = true; node->exprType = DataType::UNKNOWN; return;
    }
    if (node->index) node->index->aceptar(*this);
    // Verificación estática de índice si es literal
    if (auto ni = dynamic_cast<ExprNumero*>(node->index)) {
        if (ni->value < 0 || ni->value >= sym->arraySize) {
            cerr << "Error semántico: Índice " << ni->value
                 << " fuera de rango del arreglo '" << node->name
                 << "' (tamaño " << sym->arraySize << ")\n";
            hasError = true;
        }
    }
    node->exprType = sym->type;
}

// ---------------------------------------------------------------------------
// Visitantes de Sentencias
// ---------------------------------------------------------------------------

void AnalizadorSemantico::visitar(Bloque* node) {
    SymbolTable* child = new SymbolTable(currentEnv);
    currentEnv = child;
    for (auto s : node->statements) s->aceptar(*this);
    currentEnv = child->parent;
    delete child;
}

void AnalizadorSemantico::visitar(DeclaracionVariable* node) {
    if (inFunction) {
        // resolvedOffset ya fue asignado en el pre-scan
        if (!currentEnv->insertLocal(node->name, node->type, node->resolvedOffset)) {
            cerr << "Error semántico: Variable ya declarada '" << node->name << "'\n";
            hasError = true;
        }
    } else {
        if (!currentEnv->insert(node->name, node->type)) {
            cerr << "Error semántico: Variable ya declarada '" << node->name << "'\n";
            hasError = true;
        }
    }
}

void AnalizadorSemantico::visitar(DeclaracionArreglo* node) {
    if (node->size <= 0) {
        cerr << "Error semántico: Tamaño de arreglo '" << node->name << "' debe ser > 0\n";
        hasError = true; return;
    }
    bool ok;
    if (inFunction) {
        ok = currentEnv->insertLocalArray(node->name, node->type, node->size, node->resolvedOffset);
    } else {
        ok = currentEnv->insertArray(node->name, node->type, node->size);
    }
    if (!ok) {
        cerr << "Error semántico: Variable ya declarada '" << node->name << "'\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);

    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Asignación a variable no declarada '" << node->name << "'\n";
        hasError = true; return;
    }
    if (sym->isArray) {
        cerr << "Error semántico: '" << node->name << "' es un arreglo, use el operador []\n";
        hasError = true; return;
    }
    if (node->value->exprType == DataType::VOID) {
        cerr << "Error semántico: No se puede asignar VOID a '" << node->name << "'\n";
        hasError = true; return;
    }
    if (esTipoDe8Bits(sym->type) && esFueraDeRangoByte(node->value)) {
        auto num = dynamic_cast<ExprNumero*>(node->value);
        cerr << "Error semántico: Valor " << num->value
             << " fuera del rango de 8 bits para '" << node->name << "'\n";
        hasError = true;
    }
    sym->initialized = true; // Marcar como inicializada
}

void AnalizadorSemantico::visitar(AsignacionArreglo* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Arreglo no declarado '" << node->name << "'\n";
        hasError = true; return;
    }
    if (!sym->isArray) {
        cerr << "Error semántico: '" << node->name << "' no es un arreglo\n";
        hasError = true; return;
    }
    if (node->index) node->index->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
    if (auto ni = dynamic_cast<ExprNumero*>(node->index)) {
        if (ni->value < 0 || ni->value >= sym->arraySize) {
            cerr << "Error semántico: Índice " << ni->value << " fuera de rango en '"
                 << node->name << "'\n";
            hasError = true;
        }
    }
}

void AnalizadorSemantico::visitar(SentenciaSi* node) {
    if (node->condition) {
        node->condition->aceptar(*this);
        if (!esExpresionBooleana(node->condition)) {
            cerr << "Error semántico: La condición del 'if' debe ser una comparación"
                    " o expresión lógica (no una expresión aritmética simple)\n";
            hasError = true;
        }
    }
    if (node->thenBlock) node->thenBlock->aceptar(*this);
    if (node->elseBlock) node->elseBlock->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaMientras* node) {
    if (node->condition) {
        node->condition->aceptar(*this);
        if (!esExpresionBooleana(node->condition)) {
            cerr << "Error semántico: La condición del 'while' debe ser una comparación"
                    " o expresión lógica (no una expresión aritmética simple)\n";
            hasError = true;
        }
    }
    if (node->body) node->body->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value)   node->value->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaRetorno* node) {
    DataType retType = DataType::VOID;
    if (node->value) { node->value->aceptar(*this); retType = node->value->exprType; }

    if (!inFunction) return; // return global — no chequeamos

    if (currentFunctionReturnType == DataType::VOID && retType != DataType::VOID) {
        cerr << "Error semántico: La función '" << currentFunctionName
             << "' es void pero intenta retornar un valor\n";
        hasError = true;
    } else if (currentFunctionReturnType != DataType::VOID && retType == DataType::VOID) {
        cerr << "Error semántico: La función '" << currentFunctionName
             << "' debe retornar un valor\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(SentenciaExpresion* node) {
    if (node->expr) node->expr->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaNula* node) { /* no-op */ }

void AnalizadorSemantico::visitar(DefinicionFuncion* node) {
    // Registrar la función ANTES de procesar el cuerpo (permite recursión)
    FunctionInfo info;
    info.name = node->name;
    info.returnType = node->returnType;
    for (auto& p : node->params) {
        info.paramTypes.push_back(p.type);
        info.paramNames.push_back(p.name);
    }
    functionTable[node->name] = info;

    // ---- Pre-scan: calcular frameSize y asignar resolvedOffset ----
    int localOffset = 0;
    asignarOffsetsLocales(node->body, localOffset);
    node->frameSize = localOffset;

    // ---- Crear scope de la función ----
    SymbolTable* funcScope = new SymbolTable(currentEnv);
    SymbolTable* savedEnv = currentEnv;
    currentEnv = funcScope;

    // Insertar parámetros con su índice como offset
    for (int i = 0; i < (int)node->params.size(); i++) {
        auto& p = node->params[i];
        funcScope->insertParam(p.name, p.type, i);
        const_cast<Parametro&>(node->params[i]).frameOffset = i;
    }

    // Guardar y actualizar contexto de función
    DataType savedRetType = currentFunctionReturnType;
    string   savedFuncName = currentFunctionName;
    bool     savedInFunc   = inFunction;
    currentFunctionReturnType = node->returnType;
    currentFunctionName = node->name;
    inFunction = true;

    // Visitar sentencias del cuerpo directamente (sin crear scope extra de Bloque)
    for (auto stmt : node->body->statements)
        stmt->aceptar(*this);

    // ---- Verificar que todos los caminos retornan (si no es void) ----
    if (node->returnType != DataType::VOID) {
        if (!todosLosCaminosRetornan(node->body)) {
            cerr << "Error semántico: La función '" << node->name
                 << "' no garantiza 'return' en todas las rutas de ejecución\n";
            hasError = true;
        }
    }

    // Restaurar contexto
    currentEnv = savedEnv;
    delete funcScope;
    currentFunctionReturnType = savedRetType;
    currentFunctionName = savedFuncName;
    inFunction = savedInFunc;
}
