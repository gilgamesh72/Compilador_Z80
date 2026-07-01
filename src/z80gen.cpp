#include "z80gen.h"
#include <cctype>

using namespace std;

Z80Generator::Z80Generator(const vector<TACInstr>& t) : tac(t) {
    extractVariables();
}

bool Z80Generator::isNumber(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isdigit(c) && c != '-') return false;
    }
    return true;
}

void Z80Generator::extractVariables() {
    for (const auto& i : tac) {
        if (!i.arg1.empty() && !isNumber(i.arg1)) variables.insert(i.arg1);
        if (!i.arg2.empty() && !isNumber(i.arg2)) variables.insert(i.arg2);
        if (!i.result.empty() && i.op != TACOp::LABEL && i.op != TACOp::JUMP && i.op != TACOp::JUMPF) {
            variables.insert(i.result);
        }
    }
}

// Carga arg en HL (como 16 bits)
void Z80Generator::loadHL(const string& arg, ostream& out) {
    if (isNumber(arg)) {
        out << "    LD HL, " << arg << "\n";
    } else {
        out << "    LD HL, (" << arg << ")\n";
    }
}

// Carga arg en DE (como 16 bits)
void Z80Generator::loadDE(const string& arg, ostream& out) {
    if (isNumber(arg)) {
        out << "    LD DE, " << arg << "\n";
    } else {
        out << "    LD DE, (" << arg << ")\n";
    }
}

void Z80Generator::generate(ostream& out) {
    out << ";; --- Z80 Generado por C-Mini Compiler ---\n";
    out << "    ORG 32768 ; Origen comun para homebrew\n\n";
    out << "START:\n";
    
    int cmpCounter = 0; // Para etiquetas de comparacion unicas

    for (const auto& instr : tac) {
        switch (instr.op) {
            case TACOp::ASSIGN:
                loadHL(instr.arg1, out);
                out << "    LD (" << instr.result << "), HL\n";
                break;
                
            case TACOp::ADD:
                loadHL(instr.arg1, out);
                loadDE(instr.arg2, out);
                out << "    ADD HL, DE\n";
                out << "    LD (" << instr.result << "), HL\n";
                break;
                
            case TACOp::SUB:
                loadHL(instr.arg1, out);
                loadDE(instr.arg2, out);
                out << "    AND A ; clear carry\n";
                out << "    SBC HL, DE\n";
                out << "    LD (" << instr.result << "), HL\n";
                break;
                
            case TACOp::LT: {
                loadHL(instr.arg1, out);
                loadDE(instr.arg2, out);
                out << "    AND A ; clear carry\n";
                out << "    SBC HL, DE\n";
                string lTrue = "CMP_TRUE_" + to_string(cmpCounter);
                string lEnd = "CMP_END_" + to_string(cmpCounter++);
                out << "    JR C, " << lTrue << "\n"; // Si hay acarreo, arg1 < arg2
                out << "    LD HL, 0\n";
                out << "    JR " << lEnd << "\n";
                out << lTrue << ":\n";
                out << "    LD HL, 1\n";
                out << lEnd << ":\n";
                out << "    LD (" << instr.result << "), HL\n";
                break;
            }
                
            case TACOp::EQ: {
                loadHL(instr.arg1, out);
                loadDE(instr.arg2, out);
                out << "    AND A\n";
                out << "    SBC HL, DE\n";
                string lTrue = "CMP_TRUE_" + to_string(cmpCounter);
                string lEnd = "CMP_END_" + to_string(cmpCounter++);
                out << "    JR Z, " << lTrue << "\n"; // Si Zero, arg1 == arg2
                out << "    LD HL, 0\n";
                out << "    JR " << lEnd << "\n";
                out << lTrue << ":\n";
                out << "    LD HL, 1\n";
                out << lEnd << ":\n";
                out << "    LD (" << instr.result << "), HL\n";
                break;
            }
                
            case TACOp::LABEL:
                out << instr.result << ":\n";
                break;
                
            case TACOp::JUMP:
                out << "    JP " << instr.result << "\n";
                break;
                
            case TACOp::JUMPF:
                // arg1 == 0 jump
                loadHL(instr.arg1, out);
                out << "    LD A, H\n";
                out << "    OR L\n";
                out << "    JP Z, " << instr.result << "\n";
                break;
                
            case TACOp::POKE:
                // poke(addr, val)
                loadHL(instr.arg1, out); // addr
                out << "    PUSH HL\n";
                loadHL(instr.arg2, out); // val
                out << "    LD A, L\n";   // asumimos 8 bits para poke
                out << "    POP HL\n";
                out << "    LD (HL), A\n";
                break;
                
            case TACOp::PEEK:
                // val = peek(addr)
                loadHL(instr.arg1, out);
                out << "    LD A, (HL)\n";
                out << "    LD L, A\n";
                out << "    LD H, 0\n";
                out << "    LD (" << instr.result << "), HL\n";
                break;
                
            case TACOp::RETURN:
                out << "    RET\n";
                break;
                
            default:
                out << "    ; TACOp No implementado completamente en dummy Z80\n";
                break;
        }
    }
    
    out << "    RET\n\n"; // Fin seguro
    out << ";; --- Variables (BSS) ---\n";
    for (const auto& var : variables) {
        out << var << ": DEFW 0\n"; // Define Word (16 bits) inicializada en 0
    }
    out << "    END START\n";
}
