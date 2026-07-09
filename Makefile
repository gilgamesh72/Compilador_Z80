CXX = g++
CXXFLAGS = -std=c++14 -Wall -Isrc -Ibuild
FLEX = flex
BISON = bison

# Directorios
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Objetos
OBJS = $(BUILD_DIR)/lex.yy.o $(BUILD_DIR)/parser.tab.o \
       $(BUILD_DIR)/semantic.o $(BUILD_DIR)/tac.o \
       $(BUILD_DIR)/z80gen.o $(BUILD_DIR)/main.o

TARGET = $(BIN_DIR)/c-mini-compiler

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/parser.tab.cpp $(BUILD_DIR)/parser.tab.h: $(SRC_DIR)/parser.y
	$(BISON) --defines=$(BUILD_DIR)/parser.tab.h -o $(BUILD_DIR)/parser.tab.cpp $(SRC_DIR)/parser.y

$(BUILD_DIR)/lex.yy.cpp: $(SRC_DIR)/lexer.l $(BUILD_DIR)/parser.tab.h
	$(FLEX) -o $(BUILD_DIR)/lex.yy.cpp $(SRC_DIR)/lexer.l

$(BUILD_DIR)/lex.yy.o: $(BUILD_DIR)/lex.yy.cpp
	$(CXX) $(CXXFLAGS) -c $(BUILD_DIR)/lex.yy.cpp -o $@

$(BUILD_DIR)/parser.tab.o: $(BUILD_DIR)/parser.tab.cpp
	$(CXX) $(CXXFLAGS) -c $(BUILD_DIR)/parser.tab.cpp -o $@

$(BUILD_DIR)/semantic.o: $(SRC_DIR)/semantic.cpp $(SRC_DIR)/semantic.h \
                          $(SRC_DIR)/ast.h $(SRC_DIR)/visitor.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/semantic.cpp -o $@

$(BUILD_DIR)/tac.o: $(SRC_DIR)/tac.cpp $(SRC_DIR)/tac.h \
                     $(SRC_DIR)/ast.h $(SRC_DIR)/visitor.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/tac.cpp -o $@

$(BUILD_DIR)/z80gen.o: $(SRC_DIR)/z80gen.cpp $(SRC_DIR)/z80gen.h $(SRC_DIR)/tac.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/z80gen.cpp -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp $(SRC_DIR)/ast.h \
                      $(SRC_DIR)/semantic.h $(SRC_DIR)/tac.h $(SRC_DIR)/z80gen.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/main.cpp -o $@

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/parser.tab.cpp \
	      $(BUILD_DIR)/parser.tab.h $(BUILD_DIR)/lex.yy.cpp \
	      $(TARGET) output.asm
