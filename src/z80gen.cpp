#include "z80gen.h"
#include <cctype>
#include <sstream>

using namespace std;

// ---------------------------------------------------------------------------
GeneradorZ80::GeneradorZ80(const vector<TACInstr>& t)
    : tac(t), inFunction(false), currentFrameSize(0), currentFuncName(""),
      cmpCounter(0), notCounter(0), mulCounter(0)
{
    extraerVariables();
}

bool GeneradorZ80::esNumero(const string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    for (; i < s.size(); i++) if (!isdigit(s[i])) return false;
    return s.size() > (size_t)(s[0] == '-' ? 1 : 0);
}

string GeneradorZ80::fmtIX(int d) {
    return (d >= 0) ? "IX+" + to_string(d) : "IX" + to_string(d); // IX-N
}

// ---------------------------------------------------------------------------
void GeneradorZ80::extraerVariables() {
    set<string> frameVars;   // locales/params → no van a BSS
    set<string> funcNames;   // nombres de función → no van a BSS

    // Primera pasada: recolectar nombres de funciones, locales y arreglos
    for (const auto& i : tac) {
        if (i.op == TACOp::FUNC_BEGIN)  { funcNames.insert(i.arg1); continue; }
        if (i.op == TACOp::FUNC_END)    { continue; }
        if (i.op == TACOp::LOCAL_VAR || i.op == TACOp::PARAM_DECL) {
            frameVars.insert(i.arg1); continue;
        }
        if (i.op == TACOp::ARRAY_DECL) {
            arrayDecls[i.arg1] = {stoi(i.arg2), stoi(i.result)}; continue;
        }
        if (i.op == TACOp::NOP) continue;
    }

    // Segunda pasada: agregar al BSS solo los que no son función ni frame
    for (const auto& i : tac) {
        if (i.op == TACOp::FUNC_BEGIN || i.op == TACOp::FUNC_END ||
            i.op == TACOp::LOCAL_VAR  || i.op == TACOp::PARAM_DECL ||
            i.op == TACOp::ARRAY_DECL || i.op == TACOp::NOP) continue;

        auto add = [&](const string& s) {
            if (s.empty() || esNumero(s)) return;
            if (frameVars.count(s) || funcNames.count(s) || arrayDecls.count(s)) return;
            variables.insert(s);
        };
        add(i.arg1);
        add(i.arg2);
        // El resultado de CALL es un temporal, no el nombre de función
        if (i.op != TACOp::LABEL  && i.op != TACOp::JUMP   && i.op != TACOp::JUMPF &&
            i.op != TACOp::RETURN && i.op != TACOp::POKE   && i.op != TACOp::NOP)
            add(i.result);
    }
    for (auto& a : arrayDecls) variables.erase(a.first);
}

// ---------------------------------------------------------------------------
void GeneradorZ80::cargarHL(const string& arg, ostream& out) {
    if (esNumero(arg)) {
        out << "    LD HL, " << arg << "\n";
    } else if (inFunction && localVarMap.count(arg)) {
        int d = localVarMap[arg];
        out << "    LD L, (" << fmtIX(d)   << ")\n";
        out << "    LD H, (" << fmtIX(d+1) << ")\n";
    } else {
        out << "    LD HL, (" << arg << ")\n";
    }
}

void GeneradorZ80::cargarDE(const string& arg, ostream& out) {
    if (esNumero(arg)) {
        out << "    LD DE, " << arg << "\n";
    } else if (inFunction && localVarMap.count(arg)) {
        int d = localVarMap[arg];
        out << "    LD E, (" << fmtIX(d)   << ")\n";
        out << "    LD D, (" << fmtIX(d+1) << ")\n";
    } else {
        out << "    LD DE, (" << arg << ")\n";
    }
}

void GeneradorZ80::guardarHL(const string& dest, ostream& out) {
    if (inFunction && localVarMap.count(dest)) {
        int d = localVarMap[dest];
        out << "    LD (" << fmtIX(d)   << "), L\n";
        out << "    LD (" << fmtIX(d+1) << "), H\n";
    } else {
        out << "    LD (" << dest << "), HL\n";
    }
}

// ---------------------------------------------------------------------------
void GeneradorZ80::generar(ostream& out) {
    out << ";; --- Z80 Generado por C-Mini Compiler ---\n";
    out << "    ORG 32768\n\n";

    bool hasFunctions = false;
    for (const auto& i : tac)
        if (i.op == TACOp::FUNC_BEGIN) { hasFunctions = true; break; }

    if (hasFunctions)
        out << "    JP _program_start\n\n";
    else
        out << "START:\n";

    // Usamos dos buffers: código de funciones y código global
    ostringstream funcBuf;
    ostringstream globalBuf;

    // El target cambia según si estamos dentro de una función o no
    ostream* target = &globalBuf;

    for (const auto& instr : tac) {
        switch (instr.op) {

        // ----- Inicio de función ----------------------------------------
        case TACOp::FUNC_BEGIN: {
            currentFrameSize = stoi(instr.result);
            currentFuncName  = instr.arg1;   // para el label de epílogo
            inFunction = true;
            localVarMap.clear();
            target = &funcBuf;
            funcBuf << "\n" << instr.arg1 << ":\n";
            funcBuf << "    PUSH IX\n";
            funcBuf << "    LD IX, 0\n";
            funcBuf << "    ADD IX, SP\n";
            if (currentFrameSize > 0) {
                funcBuf << "    LD HL, " << -currentFrameSize << "\n";
                funcBuf << "    ADD HL, SP\n";
                funcBuf << "    LD SP, HL\n";
            }
            break;
        }

        // ----- Variable local (info de frame) ---------------------------
        case TACOp::LOCAL_VAR: {
            int off = stoi(instr.arg2);
            // IX_disp del byte bajo = -(frameSize - offset)
            localVarMap[instr.arg1] = -(currentFrameSize - off);
            break;
        }

        // ----- Parámetro (info de frame) --------------------------------
        case TACOp::PARAM_DECL: {
            int idx = stoi(instr.arg2);
            // Parámetros encima de IX: IX+4 (1er), IX+6 (2do), ...
            localVarMap[instr.arg1] = 4 + 2 * idx;
            break;
        }

        // ----- Fin de función -------------------------------------------
        // Define el label de epílogo al que saltan los RETURN internos.
        // Restaura frame e IX, luego retorna.
        case TACOp::FUNC_END: {
            string epi = "_epi_" + instr.arg1;
            funcBuf << epi << ":\n";
            funcBuf << "    LD SP, IX\n";
            funcBuf << "    POP IX\n";
            funcBuf << "    RET\n";
            inFunction = false;
            currentFuncName = "";
            localVarMap.clear();
            target = &globalBuf;
            break;
        }

        // ----- Asignación -----------------------------------------------
        case TACOp::ASSIGN:
            cargarHL(instr.arg1, *target);
            guardarHL(instr.result, *target);
            break;

        // ----- Suma -----------------------------------------------------
        case TACOp::ADD:
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    ADD HL, DE\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Resta ----------------------------------------------------
        case TACOp::SUB:
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n";
            *target << "    SBC HL, DE\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Multiplicación (software: suma iterativa) ----------------
        case TACOp::MUL: {
            string lL = "_MUL_" + to_string(mulCounter);
            string lE = "_MUL_E" + to_string(mulCounter++);
            cargarHL(instr.arg1, *target);
            *target << "    LD B, H\n    LD C, L\n"; // BC = multiplicando
            cargarDE(instr.arg2, *target);            // DE = multiplicador
            *target << "    LD HL, 0\n";             // resultado = 0
            *target << lL << ":\n";
            *target << "    LD A, D\n    OR E\n";
            *target << "    JR Z, " << lE << "\n";
            *target << "    ADD HL, BC\n";
            *target << "    DEC DE\n";
            *target << "    JR " << lL << "\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- División (no implementada en hardware Z80) ---------------
        case TACOp::DIV:
            *target << "    ; [DIV] Division no implementada en Z80\n";
            *target << "    LD HL, 0\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Menor que (<) -------------------------------------------
        case TACOp::LT: {
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n    SBC HL, DE\n";
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            *target << "    JR C, " << lT << "\n"; // carry ↔ arg1 < arg2
            *target << "    LD HL, 0\n    JR " << lE << "\n";
            *target << lT << ":\n    LD HL, 1\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- Mayor que (>) -------------------------------------------
        case TACOp::GT: {
            // a > b  ≡  b < a  →  invertir operandos
            cargarHL(instr.arg2, *target); // DE = b
            cargarDE(instr.arg1, *target);
            // Swap: ahora HL=b, DE=a  →  b - a < 0  cuando b < a  →  carry si b < a
            *target << "    AND A\n    SBC HL, DE\n"; // HL = b - a
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            *target << "    JR C, " << lT << "\n"; // carry ↔ b < a  ↔  a > b
            *target << "    LD HL, 0\n    JR " << lE << "\n";
            *target << lT << ":\n    LD HL, 1\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- Igual (==) -----------------------------------------------
        case TACOp::EQ: {
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n    SBC HL, DE\n";
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            *target << "    JR Z, " << lT << "\n"; // zero ↔ arg1 == arg2
            *target << "    LD HL, 0\n    JR " << lE << "\n";
            *target << lT << ":\n    LD HL, 1\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- Distinto (!=) -------------------------------------------
        case TACOp::NEQ: {
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n    SBC HL, DE\n";
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            *target << "    JR NZ, " << lT << "\n"; // not-zero ↔ a != b
            *target << "    LD HL, 0\n    JR " << lE << "\n";
            *target << lT << ":\n    LD HL, 1\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- Menor o igual (<=) ----------------------------------------
        case TACOp::LEQ: {
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n    SBC HL, DE\n"; // HL = a - b
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            // carry (a<b) OR zero (a==b) → a<=b
            *target << "    JR C, " << lT << "\n";
            *target << "    JR Z, " << lT << "\n";
            *target << "    LD HL, 0\n    JR " << lE << "\n";
            *target << lT << ":\n    LD HL, 1\n";
            *target << lE << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- Mayor o igual (>=) ----------------------------------------
        case TACOp::GEQ: {
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    AND A\n    SBC HL, DE\n"; // HL = a - b
            string lT = "CMP_T_" + to_string(cmpCounter);
            string lE = "CMP_E_" + to_string(cmpCounter++);
            // no-carry (a>=b) → resultado 1
            *target << "    JR C, " << lE << "\n"; // si carry → a < b → 0
            *target << "    LD HL, 1\n    JR " << lT << "\n";
            *target << lE << ":\n    LD HL, 0\n";
            *target << lT << ":\n";
            guardarHL(instr.result, *target);
            break;
        }

        // ----- AND lógico (&& = producto) --------------------------------
        case TACOp::AND:
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    LD A, H\n    OR L\n"; // HL != 0?
            *target << "    JR Z, _AND_F_" << cmpCounter << "\n";
            *target << "    LD A, D\n    OR E\n"; // DE != 0?
            *target << "    JR Z, _AND_F_" << cmpCounter << "\n";
            *target << "    LD HL, 1\n    JR _AND_E_" << cmpCounter << "\n";
            *target << "_AND_F_" << cmpCounter << ":\n    LD HL, 0\n";
            *target << "_AND_E_" << cmpCounter++ << ":\n";
            guardarHL(instr.result, *target);
            break;

        // ----- OR lógico (|| = suma booleana) ----------------------------
        case TACOp::OR:
            cargarHL(instr.arg1, *target);
            cargarDE(instr.arg2, *target);
            *target << "    LD A, H\n    OR L\n";
            *target << "    JR NZ, _OR_T_" << cmpCounter << "\n";
            *target << "    LD A, D\n    OR E\n";
            *target << "    JR NZ, _OR_T_" << cmpCounter << "\n";
            *target << "    LD HL, 0\n    JR _OR_E_" << cmpCounter << "\n";
            *target << "_OR_T_" << cmpCounter << ":\n    LD HL, 1\n";
            *target << "_OR_E_" << cmpCounter++ << ":\n";
            guardarHL(instr.result, *target);
            break;

        // ----- NOT lógico (!) -------------------------------------------
        case TACOp::NOT:
            cargarHL(instr.arg1, *target);
            *target << "    LD A, H\n    OR L\n";
            *target << "    JR Z, _NOT_T_" << notCounter << "\n";
            *target << "    LD HL, 0\n    JR _NOT_E_" << notCounter << "\n";
            *target << "_NOT_T_" << notCounter << ":\n    LD HL, 1\n";
            *target << "_NOT_E_" << notCounter++ << ":\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Negación aritmética (-x = 0 - x) -------------------------
        case TACOp::NEG:
            *target << "    LD HL, 0\n";
            cargarDE(instr.arg1, *target);
            *target << "    AND A\n    SBC HL, DE\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Saltos y etiquetas ----------------------------------------
        case TACOp::LABEL:
            *target << instr.result << ":\n";
            break;
        case TACOp::JUMP:
            *target << "    JP " << instr.result << "\n";
            break;
        case TACOp::JUMPF:
            cargarHL(instr.arg1, *target);
            *target << "    LD A, H\n    OR L\n";
            *target << "    JP Z, " << instr.result << "\n";
            break;

        // ----- Poke / Peek ----------------------------------------------
        case TACOp::POKE:
            cargarHL(instr.arg1, *target);
            *target << "    PUSH HL\n";
            cargarHL(instr.arg2, *target);
            *target << "    LD A, L\n";
            *target << "    POP HL\n";
            *target << "    LD (HL), A\n";
            break;
        case TACOp::PEEK:
            cargarHL(instr.arg1, *target);
            *target << "    LD A, (HL)\n    LD L, A\n    LD H, 0\n";
            guardarHL(instr.result, *target);
            break;

        // ----- Return ---------------------------------------------------
        // Carga el valor de retorno en HL y salta al epílogo de la función.
        // El epílogo (_epi_funcname) restaura IX y SP antes del RET real.
        case TACOp::RETURN:
            if (!instr.arg1.empty())
                cargarHL(instr.arg1, *target); // Valor de retorno en HL
            if (inFunction) {
                *target << "    JP _epi_" << currentFuncName << "\n";
            } else {
                *target << "    RET\n";
            }
            break;

        // ----- Llamada a función ----------------------------------------
        case TACOp::PARAM_PUSH:
            cargarHL(instr.arg1, *target);
            *target << "    PUSH HL\n";
            break;

        case TACOp::CALL: {
            int argc = instr.arg2.empty() ? 0 : stoi(instr.arg2);
            *target << "    CALL " << instr.arg1 << "\n";
            // Resultado en HL → guardar en temporal
            if (!instr.result.empty())
                guardarHL(instr.result, *target);
            // Limpiar argumentos del stack (caller cleans)
            for (int i = 0; i < argc; i++)
                *target << "    POP BC\n";
            break;
        }

        // ----- Arreglos --------------------------------------------------
        case TACOp::ARRAY_LOAD: {
            // tmp = arr[idx]
            const string& arrName  = instr.arg1;
            const string& idxVar   = instr.arg2;
            bool isArr = arrayDecls.count(arrName) > 0;
            int elemSz = isArr ? arrayDecls[arrName].elemSize : 2;

            *target << "    LD HL, " << arrName << "\n"; // base
            cargarDE(idxVar, *target);                    // índice en DE
            if (elemSz == 2) {
                *target << "    ADD HL, DE\n    ADD HL, DE\n"; // *2
            } else {
                *target << "    ADD HL, DE\n";
            }
            if (elemSz == 1) {
                *target << "    LD A, (HL)\n    LD L, A\n    LD H, 0\n";
            } else {
                *target << "    LD E, (HL)\n    INC HL\n    LD D, (HL)\n";
                *target << "    EX DE, HL\n"; // HL = valor leído
            }
            guardarHL(instr.result, *target);
            break;
        }

        case TACOp::ARRAY_STORE: {
            // arr[idx] = val
            const string& arrName = instr.arg1;
            const string& idxVar  = instr.arg2;
            const string& valVar  = instr.result;
            bool isArr = arrayDecls.count(arrName) > 0;
            int elemSz = isArr ? arrayDecls[arrName].elemSize : 2;

            *target << "    LD HL, " << arrName << "\n";
            cargarDE(idxVar, *target);
            if (elemSz == 2) {
                *target << "    ADD HL, DE\n    ADD HL, DE\n";
            } else {
                *target << "    ADD HL, DE\n";
            }
            *target << "    PUSH HL\n"; // guardar dirección
            cargarHL(valVar, *target);
            if (elemSz == 1) {
                *target << "    LD A, L\n    POP HL\n    LD (HL), A\n";
            } else {
                *target << "    LD A, L\n    POP HL\n    LD (HL), A\n";
                *target << "    INC HL\n    LD (HL), H\n"; // byte alto
            }
            break;
        }

        case TACOp::ARRAY_DECL:
        case TACOp::NOP:
        default:
            break;
        }
    }

    // ---- Ensamblar salida final ----------------------------------------
    if (hasFunctions) {
        out << funcBuf.str();
        out << "\n_program_start:\n";
    }
    out << globalBuf.str();
    out << "    RET\n\n";

    out << ";; --- Variables Globales (BSS) ---\n";
    for (const auto& arr : arrayDecls) {
        out << arr.first << ":";
        string def = (arr.second.elemSize == 1) ? " DEFB 0" : " DEFW 0";
        for (int i = 0; i < arr.second.count; i++) out << def;
        out << "\n";
    }
    for (const auto& var : variables) {
        out << var << ": DEFW 0\n";
    }
    out << "    END " << (hasFunctions ? "_program_start" : "START") << "\n";
}
