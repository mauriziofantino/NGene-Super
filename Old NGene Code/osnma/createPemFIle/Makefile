# Compilers 
CC	= gcc
MAKE = make

# flags for C compiler
# Add -fsanitize=undefined for more runtime error messages
C_FLAGS = -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse
DEBUG_FLAG = -g -DDEBUG_ENABLED
RELEASE_FLAG = -O3

# SW RX root directory
NGENE_PATH	= ../

# SW RX executable directory
OUTPUT_PATH	= .

# output
MAIN_NAME	= createPemFile	# executable name


############################################################
#             PATHS, SOURCES and OBJECTS
############################################################
# ----------------------------------------------------------
# BASE64 folder
# ----------------------------------------------------------
BASE64_PATH	= $(NGENE_PATH)
BASE64_SRC	= $(shell ls $(BASE64_PATH)/base64.c 2>/dev/null)	# Source files in SECURE_PATH directory
BASE64_HDR	= $(shell ls $(BASE64_PATH)/base64.h 2>/dev/null)	# Header files in SECURE_PATH directory
BASE64_OBJ	= $(BASE64_SRC:.c=.o)
# ----------------------------------------------------------
# ----------------------------------------------------------
# folder with main
# ----------------------------------------------------------
CREATEPEM_PATH = $(OUTPUT_PATH)
CREATEPEM_SRC	 = $(shell ls $(CREATEPEM_PATH)/*.c 2>/dev/null)	# Source files in ASM_PATH directory
CREATEPEM_HDR	 = $(shell ls $(CREATEPEM_PATH)/*.h 2>/dev/null)	# Header files in ASM_PATH directory
CREATEPEM_OBJ	 = $(CREATEPEM_SRC:.c=.o)
# ----------------------------------------------------------


ALL_OBJ	= $(BASE64_OBJ) $(CREATEPEM_OBJ)
ALL_SRC = $(BASE64_SRC) $(CREATEPEM_SRC)
ALL_HDR = $(BASE64_HDR) $(CREATEPEM_HDR)

SW_INCLUDE_PATH	= -I$(BASE64_PATH) -I$(ARRAY_PATH) -I$(CREATEPEM_PATH)
# ----------------------------------------------------------

############################################################
#             RULES
############################################################
all: base64 createpem $(MAIN_NAME)

debug: C_FLAGS += $(DEBUG_FLAG)
debug: all
#debug:
#	gcc -S -o asmFolder/prnTableDebug.asm -fverbose-asm $(C_FLAGS) $(SW_INCLUDE_PATH) asmFolder/prnTable.c
# Add the last command to get the assembly code

release: C_FLAGS += $(RELEASE_FLAG)
release: all
#release:
#	gcc -S -o asmFolder/prnTableRelease.asm -fverbose-asm $(C_FLAGS) $(SW_INCLUDE_PATH) asmFolder/prnTable.c
# Add the last command to get the assembly code

base64:		$(BASE64_OBJ)
createpem:   $(CREATEPEM_OBJ)

#----------------------------------------------------------------------	
#   base64
#----------------------------------------------------------------------
$(BASE64_PATH)/%.o	: $(BASE64_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#----------------------------------------------------------------------	
#   folder with main function
#----------------------------------------------------------------------
$(CREATEPEM_PATH)/%.o	: $(CREATEPEM_PATH)/%.c
	@echo Compiling $<
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@ 	
#----------------------------------------------------------------------	


############################################################
#                     LIBRARIES
############################################################
# In order to compile, these libraries must be installed in
# the system:
LIBRARIES = -lssl -lcrypto -lpci -lcrypt -lusb-1.0 -lm -lncurses

############################################################
#                     LINKING
############################################################
$(MAIN_NAME)   : $(ALL_OBJ)
	@echo Linking...
	$(CC) $(ALL_OBJ) $(LIBRARIES) -pthread -o $(OUTPUT_PATH)/$(MAIN_NAME)

############################################################
#                     CLEANING
############################################################	
clean:
	@echo Cleaning the project...
	rm -f $(ALL_OBJ)
	rm -f $(BASE64_PATH)/base64.o
	rm -f $(CREATEPEM_PATH)/*~


############################################################
#                    CLEAN & COMPILE
############################################################
redo:	clean release
