PROJECT = EXAMPLE1

COMPILER_PATH = ../../pic32/compiler/pic32-tools/bin

CC = $(COMPILER_PATH)/pic32-gcc
CPP = $(COMPILER_PATH)/pic32-g++
LD = $(COMPILER_PATH)/pic32-g++
SIZE = $(COMPILER_PATH)/pic32-size
HEX = $(COMPILER_PATH)/pic32-bin2hex

OBJDIR = ./obj
BINDIR = ./bin

USERSRCDIR = .
LIBSRCDIR = ../../library

C_SRC = $(wildcard $(USERSRCDIR)/*.c) \
	$(wildcard $(LIBSRCDIR)/*.c)

CPP_SRC = $(wildcard $(USERSRCDIR)/*.cpp)\
	$(wildcard $(LIBSRCDIR)/*.cpp)
ASM_SRC = $(wildcard $(USERSRCDIR)/*.S) $(wildcard $(LIBSRCDIR)/*.S)
INC = -I ./Include -I $(LIBSRCDIR)/

OBJ = 	$(addprefix $(OBJDIR)/, $(notdir $(C_SRC:.c=.o)))\
	$(addprefix $(OBJDIR)/, $(notdir $(CPP_SRC:.cpp=.o)))\
	$(addprefix $(OBJDIR)/, $(notdir $(ASM_SRC:.S=.o)))

#LINKER_FILE = p32MX270F256B.ld
LINKER_FILE = p32MX370F512H.ld

LINKER_SCRIPT = $(LIBSRCDIR)/$(LINKER_FILE)

CPU = 32MX370F512H

COMPILE_FLAGS = -MMD -g -O3 -mprocessor=$(CPU) -c
COMPILE_CFLAGS = $(COMPILE_FLAGS) -x c -Wall
COMPILE_CPPFLAGS = $(COMPILE_FLAGS) -x c++ -Wall

LD_FLAGS = -O3 -mprocessor=$(CPU) -nostartfiles  -Wl,--defsym,_min_heap_size=512,-Map=$(BINDIR)/$(PROJECT).map,--gc-sections,--report-mem
LIBRARIES = $(wildcard $(COMPILER_PATH)/../lib/gcc/pic32mx/4.5.2/crt*.o)

debug: all

.PHONY: all clean size

all: $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).hex size

size:$(BINDIR)/$(PROJECT).elf
	$(SIZE) $(BINDIR)/EXAMPLE1.elf

clean:
	rm $(OBJDIR) $(BINDIR) -r

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(HEX) $(BINDIR)/EXAMPLE1.elf

$(OBJDIR)/%.o: $(LIBSRCDIR)/%.S
	-mkdir -p $(OBJDIR)
	$(CC) $(COMPILE_FLAGS) -x assembler-with-cpp -o $@ $<

$(OBJDIR)/%.o: $(USERSRCDIR)/%.cpp 
	-mkdir -p $(OBJDIR)
	$(CPP)  $(COMPILE_CPPFLAGS) $(INC) -o $@ $<

$(OBJDIR)/%.o: $(LIBSRCDIR)/%.cpp 
	-mkdir -p $(OBJDIR)
	$(CPP)  $(COMPILE_CPPFLAGS) $(INC) -o $@ $<

$(OBJDIR)/%.o: $(USERSRCDIR)/%.c 
	-mkdir -p $(OBJDIR)
	$(CC)  $(COMPILE_CFLAGS) $(INC) -o $@ $<

$(OBJDIR)/%.o: $(LIBSRCDIR)/%.c 
	-mkdir -p $(OBJDIR)
	$(CC)  $(COMPILE_CFLAGS) $(INC) -o $@ $<

$(BINDIR)/$(PROJECT).elf: $(OBJ)
	-mkdir -p $(BINDIR)
	$(LD) $(LD_FLAGS) -T $(LINKER_SCRIPT) -o $@ $^ $(LIBRARIES)

