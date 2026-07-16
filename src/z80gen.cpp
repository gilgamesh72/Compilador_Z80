#include "z80gen.h"
#include <cctype>
#include <sstream>

using namespace std;

// ============================================================
//  Constructor
// ============================================================

GeneradorZ80::GeneradorZ80(const vector<TACInstr>& t)
    : tac(t), cmpCounter(0), inFunction(false), lastWasReturn(false) {
    extraerVariables();
}

void GeneradorZ80::registrarArreglo(const string& name, int bytes) {
    arrays[name] = bytes;
    variables.erase(name);
}

// ============================================================
//  Funciones auxiliares
// ============================================================

bool esNumero(const string& s) {
    if (s.empty()) return false;
    size_t start = (s[0] == '-') ? 1 : 0;
    for (size_t i = start; i < s.size(); i++)
        if (!isdigit(s[i])) return false;
    return true;
}

bool esEtiquetaFlujo(const string& s) {
    if (s.size() > 1 && s[0] == 'L' && isdigit(s[1])) return true;
    if (s.size() >= 4 && s.substr(0, 4) == "STR_") return true;
    if (s.size() >= 4 && s.substr(0, 4) == "CMP_") return true;
    return false;
}

string safeName(const string& s) {
    static const set<string> reserved = {
        "a","b","c","d","e","f","h","l","i","r",
        "af","bc","de","hl","ix","iy","sp","pc",
        "af'","ixl","ixh","iyl","iyh"
    };
    string lower = s;
    for (char& ch : lower) ch = tolower(ch);
    if (reserved.count(lower)) return "_v_" + s;
    return s;
}

// ============================================================
//  Extraer variables del TAC
// ============================================================

void GeneradorZ80::extraerVariables() {
    for (const auto& i : tac) {
        if (i.op == TACOp::PARAM_DECL) {
            allParamVars.insert(i.arg1);
            continue;
        }
        if (i.op == TACOp::STRING_DECL) { strings[i.arg1] = i.arg2; continue; }
        if (i.op == TACOp::ADDR_OF)     { continue; }
        if (i.op == TACOp::FUNC_BEGIN || i.op == TACOp::FUNC_END) { continue; }
        if (i.op == TACOp::PARAM)       { continue; }
        if (i.op == TACOp::CALL) {
            if (!i.result.empty() && !esNumero(i.result) && !esEtiquetaFlujo(i.result))
                variables.insert(i.result);
            continue;
        }
        if (i.op == TACOp::ARRAY_LOAD || i.op == TACOp::ARRAY_STORE) {
            if (!i.result.empty()) variables.insert(i.result);
            continue;
        }

        auto addVar = [&](const string& s) {
            if (!s.empty() && !esNumero(s) && !esEtiquetaFlujo(s)) {
                if (i.op == TACOp::LABEL || i.op == TACOp::JUMP || i.op == TACOp::JUMPF)
                    return;
                variables.insert(s);
            }
        };
        addVar(i.arg1);
        addVar(i.arg2);
        if (i.op != TACOp::LABEL && i.op != TACOp::JUMP && i.op != TACOp::JUMPF)
            addVar(i.result);
    }
    for (auto& kv : arrays)    variables.erase(kv.first);
    for (auto& kv : strings)   variables.erase(kv.first);
    for (const auto& p : allParamVars) variables.erase(p);
}

// ============================================================
//  rawParamOffset: busca offset IX manejando prefijo _v_
// ============================================================

int GeneradorZ80::rawParamOffset(const string& arg) const {
    auto it = currentParamOffsets.find(arg);
    if (it != currentParamOffsets.end()) return it->second;
    if (arg.size() > 3 && arg.substr(0, 3) == "_v_") {
        auto it2 = currentParamOffsets.find(arg.substr(3));
        if (it2 != currentParamOffsets.end()) return it2->second;
    }
    return -1;
}

// ============================================================
//  Helpers de carga / almacenamiento
// ============================================================

void GeneradorZ80::cargarHL(const string& arg, ostream& out) {
    if (esNumero(arg)) {
        out << "    LD HL, " << arg << "\n";
    } else {
        int offset = rawParamOffset(arg);
        if (offset >= 0) {
            out << "    LD L, (IX+" << offset     << ")\n";
            out << "    LD H, (IX+" << (offset+1) << ")\n";
        } else {
            out << "    LD HL, (" << safeName(arg) << ")\n";
        }
    }
}

void GeneradorZ80::cargarDE(const string& arg, ostream& out) {
    if (esNumero(arg)) {
        out << "    LD DE, " << arg << "\n";
    } else {
        int offset = rawParamOffset(arg);
        if (offset >= 0) {
            out << "    LD E, (IX+" << offset     << ")\n";
            out << "    LD D, (IX+" << (offset+1) << ")\n";
        } else {
            out << "    LD DE, (" << safeName(arg) << ")\n";
        }
    }
}

void GeneradorZ80::cargarA(const string& arg, ostream& out) {
    if (esNumero(arg)) {
        out << "    LD A, " << arg << "\n";
    } else {
        int offset = rawParamOffset(arg);
        if (offset >= 0) {
            out << "    LD A, (IX+" << offset << ")\n";
        } else {
            out << "    LD A, (" << safeName(arg) << ")\n";
        }
    }
}

void GeneradorZ80::almacenarHL(const string& result, ostream& out) {
    if (result.empty()) return;
    int offset = rawParamOffset(result);
    if (offset >= 0) {
        out << "    LD (IX+" << offset     << "), L\n";
        out << "    LD (IX+" << (offset+1) << "), H\n";
    } else {
        out << "    LD (" << safeName(result) << "), HL\n";
    }
}

// ============================================================
//  Generación principal — DOS PASADAS
//  mainCode: código del programa principal (START:)
//  funcCode: cuerpos de funciones (debajo del programa principal)
//
//  Estructura del ASM de salida:
//    START:
//      [mainCode]    ← código global se ejecuta primero
//      RET
//      [funcCode]    ← funciones llamadas con CALL desde mainCode
//      [rutinas math]
//      [sección de datos]
//    END START
// ============================================================

void GeneradorZ80::generar(ostream& out) {
    out << ";; ============================================================\n";
    out << ";; Z80 Ensamblador - Z80-Mini Compiler (con funciones)\n";
    out << ";; ============================================================\n";
    out << "    ORG 32768\n\n";
    out << "START:\n";

    ostringstream mainCode;
    ostringstream funcCode;

    for (const auto& instr_raw : tac) {

        // FUNC_BEGIN: siempre va a funcCode, activa el modo función
        if (instr_raw.op == TACOp::FUNC_BEGIN) {
            inFunction    = true;
            lastWasReturn = false;
            currentParamOffsets.clear();
            funcCode << "\n;; ---- función: " << instr_raw.arg1 << " ----\n";
            funcCode << instr_raw.arg1 << ":\n";
            funcCode << "    PUSH IX\n";
            funcCode << "    LD IX, 0\n";
            funcCode << "    ADD IX, SP\n";
            continue;
        }

        // Seleccionar flujo de salida según contexto
        ostream& cur = inFunction ? (ostream&)funcCode : (ostream&)mainCode;

        // Normalizar nombres (evitar colisión con registros Z80)
        TACInstr instr = instr_raw;
        if (instr.op != TACOp::LABEL && instr.op != TACOp::JUMP &&
            instr.op != TACOp::JUMPF && instr.op != TACOp::STRING_DECL &&
            instr.op != TACOp::ADDR_OF && instr.op != TACOp::FUNC_END &&
            instr.op != TACOp::CALL && instr.op != TACOp::PARAM_DECL) {
            if (!esNumero(instr.arg1) && !esEtiquetaFlujo(instr.arg1))
                instr.arg1 = safeName(instr.arg1);
            if (!esNumero(instr.arg2) && !esEtiquetaFlujo(instr.arg2))
                instr.arg2 = safeName(instr.arg2);
            if (!esNumero(instr.result) && !esEtiquetaFlujo(instr.result))
                instr.result = safeName(instr.result);
        }

        switch (instr.op) {

        case TACOp::ASSIGN:
            cargarHL(instr.arg1, cur);
            almacenarHL(instr.result, cur);
            break;

        case TACOp::ADD:
            cargarHL(instr.arg1, cur);
            cargarDE(instr.arg2, cur);
            cur << "    ADD HL, DE\n";
            almacenarHL(instr.result, cur);
            break;

        case TACOp::SUB:
            cargarHL(instr.arg1, cur);
            cargarDE(instr.arg2, cur);
            cur << "    AND A\n";
            cur << "    SBC HL, DE\n";
            almacenarHL(instr.result, cur);
            break;

        case TACOp::MUL:
            cargarHL(instr.arg1, cur);
            cargarDE(instr.arg2, cur);
            cur << "    CALL __MUL16\n";
            almacenarHL(instr.result, cur);
            break;

        case TACOp::DIV:
            cargarHL(instr.arg1, cur);
            cargarDE(instr.arg2, cur);
            cur << "    CALL __DIV16\n";
            almacenarHL(instr.result, cur);
            break;

        case TACOp::MOD:
            cargarHL(instr.arg1, cur);
            cargarDE(instr.arg2, cur);
            cur << "    CALL __DIV16\n";
            cur << "    EX DE, HL\n";
            almacenarHL(instr.result, cur);
            break;

        case TACOp::LT: {
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR C, " << lt << "\n";
            cur << "    LD HL, 0\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 1\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::GT: {
            cargarHL(instr.arg2, cur); cargarDE(instr.arg1, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR C, " << lt << "\n";
            cur << "    LD HL, 0\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 1\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::LE: {
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR Z, " << lt << "\n"; cur << "    JR C, " << lt << "\n";
            cur << "    LD HL, 0\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 1\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::GE: {
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR C, " << lt << "\n";
            cur << "    LD HL, 1\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 0\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::EQ: {
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR Z, " << lt << "\n";
            cur << "    LD HL, 0\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 1\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::NEQ: {
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    AND A\n"; cur << "    SBC HL, DE\n";
            string lt = "CMP_T_" + to_string(cmpCounter);
            string le = "CMP_E_" + to_string(cmpCounter++);
            cur << "    JR NZ, " << lt << "\n";
            cur << "    LD HL, 0\n"; cur << "    JR " << le << "\n";
            cur << lt << ":\n"; cur << "    LD HL, 1\n"; cur << le << ":\n";
            almacenarHL(instr.result, cur); break;
        }

        case TACOp::AND:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n"; cur << "    PUSH AF\n";
            cargarHL(instr.arg2, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n"; cur << "    POP BC\n";
            cur << "    AND B\n"; cur << "    LD L, A\n"; cur << "    LD H, 0\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::OR:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n"; cur << "    PUSH AF\n";
            cargarHL(instr.arg2, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n"; cur << "    POP BC\n";
            cur << "    OR B\n"; cur << "    LD L, A\n"; cur << "    LD H, 0\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::NOT:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n";
            cur << "    LD HL, 0\n"; cur << "    JR NZ, $+4\n"; cur << "    LD HL, 1\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::BAND:
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    LD A, H\n"; cur << "    AND D\n"; cur << "    LD H, A\n";
            cur << "    LD A, L\n"; cur << "    AND E\n"; cur << "    LD L, A\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::BOR:
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    LD A, H\n"; cur << "    OR D\n"; cur << "    LD H, A\n";
            cur << "    LD A, L\n"; cur << "    OR E\n"; cur << "    LD L, A\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::BXOR:
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    LD A, H\n"; cur << "    XOR D\n"; cur << "    LD H, A\n";
            cur << "    LD A, L\n"; cur << "    XOR E\n"; cur << "    LD L, A\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::BNOT:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, H\n"; cur << "    CPL\n"; cur << "    LD H, A\n";
            cur << "    LD A, L\n"; cur << "    CPL\n"; cur << "    LD L, A\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::LSHIFT:
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    CALL __LSHIFT16\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::RSHIFT:
            cargarHL(instr.arg1, cur); cargarDE(instr.arg2, cur);
            cur << "    CALL __RSHIFT16\n";
            almacenarHL(instr.result, cur); break;

        // POKE / PEEK
        case TACOp::POKE:
            cargarHL(instr.arg1, cur);
            cur << "    PUSH HL\n";
            cargarHL(instr.arg2, cur);
            cur << "    LD A, L\n";
            cur << "    POP HL\n";
            cur << "    LD (HL), A\n";
            break;

        case TACOp::PEEK:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, (HL)\n";
            cur << "    LD L, A\n";
            cur << "    LD H, 0\n";
            almacenarHL(instr.result, cur);
            break;

        // Arreglos
        case TACOp::ARRAY_LOAD: {
            cur << "    LD HL, " << instr.arg1 << "_BASE\n";
            cargarDE(instr.arg2, cur);
            cur << "    ADD HL, DE\n"; cur << "    ADD HL, DE\n";
            cur << "    LD E, (HL)\n"; cur << "    INC HL\n";
            cur << "    LD D, (HL)\n"; cur << "    EX DE, HL\n";
            almacenarHL(instr.result, cur); break;
        }
        case TACOp::ARRAY_STORE: {
            cur << "    LD HL, " << instr.arg1 << "_BASE\n";
            cargarDE(instr.arg2, cur);
            cur << "    ADD HL, DE\n"; cur << "    ADD HL, DE\n"; cur << "    PUSH HL\n";
            cargarDE(instr.result, cur);
            cur << "    POP HL\n"; cur << "    LD (HL), E\n";
            cur << "    INC HL\n"; cur << "    LD (HL), D\n"; break;
        }
        case TACOp::ADDR_OF:
            break;

        // Puertos
        case TACOp::PORT_IN:
            cargarA(instr.arg1, cur);
            cur << "    LD C, A\n"; cur << "    IN A, (C)\n";
            cur << "    LD L, A\n"; cur << "    LD H, 0\n";
            almacenarHL(instr.result, cur); break;

        case TACOp::PORT_OUT:
            cargarA(instr.arg1, cur);
            cur << "    LD C, A\n";
            cargarA(instr.arg2, cur);
            cur << "    OUT (C), A\n"; break;

        // Control de flujo
        case TACOp::LABEL:
            cur << instr.result << ":\n"; break;

        case TACOp::JUMP:
            cur << "    JP " << instr.result << "\n"; break;

        case TACOp::JUMPF:
            cargarHL(instr.arg1, cur);
            cur << "    LD A, H\n"; cur << "    OR L\n";
            cur << "    JP Z, " << instr.result << "\n"; break;

        // RETURN — distingue main vs. función (marco IX)
        case TACOp::RETURN:
            lastWasReturn = true;
            if (inFunction) {
                if (!instr.arg1.empty()) { cargarHL(instr.arg1, cur); }
                else                     { cur << "    LD HL, 0\n"; }
                cur << "    LD SP, IX\n";
                cur << "    POP IX\n";
                cur << "    RET\n";
            } else {
                cur << "    RET\n";
            }
            break;

        // Funciones
        case TACOp::PARAM_DECL:
            currentParamOffsets[instr_raw.arg1] = stoi(instr_raw.arg2);
            break;

        case TACOp::FUNC_END:
            if (!lastWasReturn) {
                cur << "    LD SP, IX\n";
                cur << "    POP IX\n";
                cur << "    RET\n";
            }
            inFunction    = false;
            lastWasReturn = false;
            currentParamOffsets.clear();
            break;

        case TACOp::PARAM:
            cargarHL(instr.arg1, cur);
            cur << "    PUSH HL\n";
            break;

        case TACOp::CALL: {
            cur << "    CALL " << instr_raw.arg1 << "\n";
            if (!instr_raw.result.empty()) almacenarHL(instr_raw.result, cur);
            int numArgs = stoi(instr_raw.arg2);
            for (int i = 0; i < numArgs; i++) cur << "    POP BC\n";
            break;
        }

        case TACOp::STRING_DECL:
            break;

        default:
            cur << "    ; TAC sin implementar: op=" << (int)instr.op << "\n";
            break;
        }
    }

    // ── Emitir código principal → RET → funciones ─────────────────────────
    out << mainCode.str();
    out << "    RET\n\n";
    out << funcCode.str();

    emitirRutinasMath(out);
    emitirSeccionDatos(out);
    out << "    END START\n";
}

// ============================================================
//  Rutinas software de matemáticas
// ============================================================

void GeneradorZ80::emitirRutinasMath(ostream& out) {
    out << "\n;; ============================================================\n";
    out << ";; Rutinas de software matemático\n";
    out << ";; ============================================================\n\n";

    out << "__MUL16:\n";
    out << "    LD B, 16\n";
    out << "    LD C, L\n";
    out << "    LD A, H\n";
    out << "    LD HL, 0\n";
    out << "__MUL16_LOOP:\n";
    out << "    SRL A\n";
    out << "    RR C\n";
    out << "    JR NC, __MUL16_SKIP\n";
    out << "    ADD HL, DE\n";
    out << "__MUL16_SKIP:\n";
    out << "    SLA E\n";
    out << "    RL D\n";
    out << "    DJNZ __MUL16_LOOP\n";
    out << "    RET\n\n";

    out << "__DIV16:\n";
    out << "    LD A, L\n";
    out << "    LD (__DIV16_NL), A\n";
    out << "    LD A, H\n";
    out << "    LD (__DIV16_NH), A\n";
    out << "    XOR A\n";
    out << "    LD (__DIV16_RL), A\n";
    out << "    LD (__DIV16_RH), A\n";
    out << "    LD HL, 0\n";
    out << "    LD B, 16\n";
    out << "__DIV16_LOOP:\n";
    out << "    LD A, (__DIV16_NH)\n"; out << "    RLCA\n"; out << "    LD (__DIV16_NH), A\n";
    out << "    LD A, (__DIV16_NL)\n"; out << "    RLCA\n"; out << "    LD (__DIV16_NL), A\n";
    out << "    LD A, (__DIV16_RH)\n"; out << "    RLA\n";  out << "    LD (__DIV16_RH), A\n";
    out << "    LD A, (__DIV16_RL)\n"; out << "    RLA\n";  out << "    LD (__DIV16_RL), A\n";
    out << "    PUSH HL\n";
    out << "    LD A, (__DIV16_RH)\n"; out << "    LD H, A\n";
    out << "    LD A, (__DIV16_RL)\n"; out << "    LD L, A\n";
    out << "    AND A\n";
    out << "    SBC HL, DE\n";
    out << "    JR C, __DIV16_SKIP\n";
    out << "    LD A, L\n"; out << "    LD (__DIV16_RL), A\n";
    out << "    LD A, H\n"; out << "    LD (__DIV16_RH), A\n";
    out << "    POP HL\n"; out << "    SCF\n"; out << "    JR __DIV16_SH\n";
    out << "__DIV16_SKIP:\n"; out << "    POP HL\n"; out << "    AND A\n";
    out << "__DIV16_SH:\n"; out << "    ADC HL, HL\n"; out << "    DJNZ __DIV16_LOOP\n";
    out << "    LD A, (__DIV16_RH)\n"; out << "    LD D, A\n";
    out << "    LD A, (__DIV16_RL)\n"; out << "    LD E, A\n";
    out << "    RET\n";
    out << "__DIV16_NL: DEFB 0\n";
    out << "__DIV16_NH: DEFB 0\n";
    out << "__DIV16_RL: DEFB 0\n";
    out << "__DIV16_RH: DEFB 0\n\n";

    out << "__LSHIFT16:\n";
    out << "    LD B, E\n"; out << "    INC B\n"; out << "    JR __LSHIFT16_CHECK\n";
    out << "__LSHIFT16_LOOP:\n"; out << "    ADD HL, HL\n";
    out << "__LSHIFT16_CHECK:\n"; out << "    DJNZ __LSHIFT16_LOOP\n"; out << "    RET\n\n";

    out << "__RSHIFT16:\n";
    out << "    LD B, E\n"; out << "    INC B\n"; out << "    JR __RSHIFT16_CHECK\n";
    out << "__RSHIFT16_LOOP:\n"; out << "    SRL H\n"; out << "    RR L\n";
    out << "__RSHIFT16_CHECK:\n"; out << "    DJNZ __RSHIFT16_LOOP\n"; out << "    RET\n\n";
}

// ============================================================
//  Sección de datos
// ============================================================

void GeneradorZ80::emitirSeccionDatos(ostream& out) {
    out << ";; ============================================================\n";
    out << ";; Sección de datos\n";
    out << ";; ============================================================\n\n";

    if (!variables.empty()) {
        out << ";; Variables simples\n";
        for (const auto& var : variables) {
            if (allParamVars.count(var)) continue;
            out << safeName(var) << ": DEFW 0\n";
        }
        out << "\n";
    }

    if (!arrays.empty()) {
        out << ";; Arreglos\n";
        for (const auto& kv : arrays) {
            out << kv.first << ": DEFS " << kv.second << "\n";
            out << kv.first << "_BASE EQU " << kv.first << "\n";
        }
        out << "\n";
    }

    if (!strings.empty()) {
        out << ";; Literales de cadena\n";
        for (const auto& kv : strings) {
            out << kv.first << ": DEFM \"" << kv.second << "\", 0\n";
        }
        out << "\n";
    }
}
