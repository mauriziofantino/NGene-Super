# Compilers 
CC	= gcc
MAKE = make

# flags for C compiler
# Add -fsanitize=undefined for more runtime error messages
C_FLAGS = -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse
DEBUG_FLAG = -g -DDEBUG_ENABLED
PROFILING_FLAG= -pg -DDEBUG_ENABLED
RELEASE_FLAG = -O3
LINKING_FLAG= -pthread

# SW RX root directory
NGENE_PATH	= ../..

# SW RX executable directory
OUTPUT_PATH	= .

# output
MAIN_NAME	= testCrypto	# executable name


############################################################
#             PATHS, SOURCES and OBJECTS
############################################################
# ----------------------------------------------------------
# secure folder
# ----------------------------------------------------------
SECURE_PATH	= $(NGENE_PATH)/secure
SECURE_SRC	= $(shell ls $(SECURE_PATH)/*.c 2>/dev/null)	# Source files in SECURE_PATH directory
SECURE_HDR	= $(shell ls $(SECURE_PATH)/*.h 2>/dev/null)	# Header files in SECURE_PATH directory
SECURE_OBJ	= $(SECURE_SRC:.c=.o)
PCI_PATH	= $(SECURE_PATH)/PCI_library
# ----------------------------------------------------------
# front-end folder
# ----------------------------------------------------------
FE_PATH	= $(NGENE_PATH)/frontEnd
FE_SRC	= $(shell ls $(FE_PATH)/*.c 2>/dev/null)	# Source files in FE_PATH directory
FE_HDR	= $(shell ls $(FE_PATH)/*.h 2>/dev/null)	# Header files in FE_PATH directory
FE_OBJ	= $(FE_SRC:.c=.o)
# ----------------------------------------------------------
# warm start folder
# ----------------------------------------------------------
WARM_PATH	= $(NGENE_PATH)/warmStart
WARM_SRC	= $(shell ls $(WARM_PATH)/*.c 2>/dev/null)	# Source files in WARM_PATH directory
WARM_HDR	= $(shell ls $(WARM_PATH)/*.h 2>/dev/null)	# Header files in WARM_PATH directory
WARM_OBJ	= $(WARM_SRC:.c=.o)
# ----------------------------------------------------------
# acquisition folder
# ----------------------------------------------------------
ACQ_PATH	= $(NGENE_PATH)/acquisition
ACQ_SRC		= $(shell ls $(ACQ_PATH)/*.c 2>/dev/null)	# Source files in ACQ_PATH directory
ACQ_HDR		= $(shell ls $(ACQ_PATH)/*.h 2>/dev/null)	# Header files in ACQ_PATH directory
ACQ_OBJ		= $(ACQ_SRC:.c=.o)
# ----------------------------------------------------------
# tracking folder
# ----------------------------------------------------------
TRK_PATH	= $(NGENE_PATH)/tracking
TRK_SRC		= $(shell ls $(TRK_PATH)/*.c 2>/dev/null)	# Source files in TRK_PATH directory
TRK_HDR		= $(shell ls $(TRK_PATH)/*.h 2>/dev/null)	# Header files in TRK_PATH directory
TRK_OBJ		= $(TRK_SRC:.c=.o)
# ----------------------------------------------------------
# PVT folder
# ----------------------------------------------------------
PVT_PATH	= $(NGENE_PATH)/pvt
PVT_SRC		= $(shell ls $(PVT_PATH)/*.c 2>/dev/null)	# Source files in PVT_PATH directory
PVT_HDR		= $(shell ls $(PVT_PATH)/*.h 2>/dev/null)	# Header files in PVT_PATH directory
PVT_OBJ		= $(PVT_SRC:.c=.o)
# ----------------------------------------------------------
# utility folder
# ----------------------------------------------------------
UTILITY_PATH	= $(NGENE_PATH)/utility
UTILITY_SRC	= $(shell ls $(UTILITY_PATH)/*.c 2>/dev/null)	# Source files in UTILITY_PATH directory
UTILITY_HDR	= $(shell ls $(UTILITY_PATH)/*.h 2>/dev/null)	# Header files in UTILITY_PATH directory
UTILITY_OBJ	= $(UTILITY_SRC:.c=.o)
# ----------------------------------------------------------
# utility GNSS folder
# ----------------------------------------------------------
UTL_GNSS_PATH	= $(NGENE_PATH)/utilityGnss
UTL_GNSS_SRC	= $(shell ls $(UTL_GNSS_PATH)/*.c 2>/dev/null)	# Source files in UTL_GNSS_PATH directory
UTL_GNSS_HDR	= $(shell ls $(UTL_GNSS_PATH)/*.h 2>/dev/null)	# Header files in UTL_GNSS_PATH directory
UTL_GNSS_OBJ	= $(UTL_GNSS_SRC:.c=.o)
# ----------------------------------------------------------
# Galileo OSNMA folder
# ----------------------------------------------------------
OSNMA_PATH	= $(NGENE_PATH)/osnma
OSNMA_SRC	= $(shell ls $(OSNMA_PATH)/*.c 2>/dev/null)	# Source files in OSNMA_PATH directory
OSNMA_HDR	= $(shell ls $(OSNMA_PATH)/*.h 2>/dev/null)	# Header files in OSNMA_PATH directory
OSNMA_OBJ	= $(OSNMA_SRC:.c=.o)
# ----------------------------------------------------------
# GPS Chimera folder
# ----------------------------------------------------------
CHIMERA_PATH	= $(NGENE_PATH)/chimera
CHIMERA_SRC	= $(shell ls $(CHIMERA_PATH)/*.c 2>/dev/null)	# Source files in CHIMERA_PATH directory
CHIMERA_HDR	= $(shell ls $(CHIMERA_PATH)/*.h 2>/dev/null)	# Header files in CHIMERA_PATH directory
CHIMERA_OBJ	= $(CHIMERA_SRC:.c=.o)
# ----------------------------------------------------------
# NeQuick folder
# ----------------------------------------------------------
NEQUICK_PATH	= $(NGENE_PATH)/utilityGnss/NeQuick
NEQUICK_SRC		= $(shell ls $(NEQUICK_PATH)/*.c 2>/dev/null)	# Source files in NEQUICK_PATH directory
NEQUICK_HDR		= $(shell ls $(NEQUICK_PATH)/*.h 2>/dev/null)	# Header files in NEQUICK_PATH directory
NEQUICK_OBJ		= $(NEQUICK_SRC:.c=.o)
# ----------------------------------------------------------
# GUI folder
# ----------------------------------------------------------
GUI_PATH	= $(NGENE_PATH)/guiSupport
GUI_SRC		= $(shell ls $(GUI_PATH)/*.c 2>/dev/null)	# Source files in GUI_PATH directory
GUI_HDR		= $(shell ls $(GUI_PATH)/*.h 2>/dev/null)	# Header files in GUI_PATH directory
GUI_OBJ		= $(GUI_SRC:.c=.o)
# ----------------------------------------------------------
# receiver
# ----------------------------------------------------------
RX_PATH	= $(NGENE_PATH)/receiver
RX_SRC	= $(shell ls $(RX_PATH)/*.c 2>/dev/null)	# Source files in RX_PATH directory
RX_HDR	= $(shell ls $(RX_PATH)/*.h 2>/dev/null)	# Header files in RX_PATH directory
RX_OBJ	= $(RX_SRC:.c=.o)
# ----------------------------------------------------------
# folder with assembly cources
# ----------------------------------------------------------
ASM_PATH = $(NGENE_PATH)/asmFolder
ASM_SRC	 = $(shell ls $(ASM_PATH)/*.c 2>/dev/null)	# Source files in ASM_PATH directory
ASM_HDR	 = $(shell ls $(ASM_PATH)/*.h 2>/dev/null)	# Header files in ASM_PATH directory
ASM_OBJ	 = $(ASM_SRC:.c=.o)
# ----------------------------------------------------------
# folder with test Crypto Functions
# ----------------------------------------------------------
TESTCRYPTO_PATH = $(OUTPUT_PATH)
TESTCRYPTO_SRC	 = $(shell ls $(TESTCRYPTO_PATH)/*.c 2>/dev/null)	# Source files in ASM_PATH directory
TESTCRYPTO_HDR	 = $(shell ls $(TESTCRYPTO_PATH)/*.h 2>/dev/null)	# Header files in ASM_PATH directory
TESTCRYPTO_OBJ	 = $(TESTCRYPTO_SRC:.c=.o)
# ----------------------------------------------------------


ALL_OBJ	= $(SECURE_OBJ) $(FE_OBJ) $(WARM_OBJ) $(ACQ_OBJ) $(TRK_OBJ) $(PVT_OBJ) $(UTILITY_OBJ) $(UTL_GNSS_OBJ) $(OSNMA_OBJ) $(CHIMERA_OBJ)	$(NEQUICK_OBJ) $(GUI_OBJ) $(RX_OBJ) $(ASM_OBJ) $(TESTCRYPTO_OBJ)
ALL_SRC = $(SECURE_SRC) $(FE_SRC) $(WARM_SRC) $(ACQ_SRC) $(TRK_SRC) $(PVT_SRC) $(UTILITY_SRC) $(UTL_GNSS_SRC) $(OSNMA_SRC) $(CHIMERA_SRC)	$(NEQUICK_SRC) $(GUI_SRC) $(RX_SRC) $(ASM_SRC) $(TESTCRYPTO_SRC)
ALL_HDR = $(SECURE_HDR) $(FE_HDR) $(WARM_HDR) $(ACQ_HDR) $(TRK_HDR) $(PVT_HDR) $(UTILITY_HDR) $(UTL_GNSS_HDR) $(OSNMA_HDR) $(CHIMERA_HDR)	$(NEQUICK_HDR) $(GUI_HDR) $(RX_HDR) $(ASM_HDR) $(TESTCRYPTO_HDR)

SW_INCLUDE_PATH	= -I$(SECURE_PATH) -I$(PCI_PATH) -I$(FE_PATH) -I$(WARM_PATH) -I$(ACQ_PATH) -I$(TRK_PATH) -I$(PVT_PATH) -I$(UTILITY_PATH) -I$(UTL_GNSS_PATH) -I$(OSNMA_PATH) -I$(CHIMERA_PATH)	-I$(NEQUICK_PATH) -I$(GUI_PATH) -I$(RX_PATH) -I$(ASM_PATH) -I$(TESTCRYPTO_PATH) 
# ----------------------------------------------------------

############################################################
#             RULES
############################################################
all: secure fe warm acq trk pvt utility utl_gnss osnma chimera neq gui rx assembly testCrypto $(MAIN_NAME)

debug: C_FLAGS += $(DEBUG_FLAG)
debug: all
#debug:
#	gcc -S -o asmFolder/prnTableDebug.asm -fverbose-asm $(C_FLAGS) $(SW_INCLUDE_PATH) asmFolder/prnTable.c
# Add the last command to get the assembly code

profiling: C_FLAGS += $(PROFILING_FLAG)
profiling: LINKING_FLAG += -pg
profiling: all

release: C_FLAGS += $(RELEASE_FLAG)
release: all
#release:
#	gcc -S -o asmFolder/prnTableRelease.asm -fverbose-asm $(C_FLAGS) $(SW_INCLUDE_PATH) asmFolder/prnTable.c
# Add the last command to get the assembly code

secure:		$(SECURE_OBJ)
fe:			$(FE_OBJ)
warm:		$(WARM_OBJ)
acq:		$(ACQ_OBJ)
trk:		$(TRK_OBJ)
pvt:		$(PVT_OBJ)
utility:	$(UTILITY_OBJ)
utl_gnss:   $(UTL_GNSS_OBJ)
osnma:      $(OSNMA_OBJ)
chimera:    $(CHIMERA_OBJ)
neq: 		$(NEQUICK_OBJ)
gui:		$(GUI_OBJ)
rx:			$(RX_OBJ)
assembly:   $(ASM_OBJ)
testCrypto:   $(TESTCRYPTO_OBJ)

#----------------------------------------------------------------------	
#   secure
#----------------------------------------------------------------------
$(SECURE_PATH)/%.o	: $(SECURE_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   front-end
#----------------------------------------------------------------------
$(FE_PATH)/%.o	: $(FE_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   warm start
#----------------------------------------------------------------------
$(WARM_PATH)/%.o	: $(WARM_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   acquisition
#----------------------------------------------------------------------
$(ACQ_PATH)/%.o	: $(ACQ_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   tracking
#----------------------------------------------------------------------
$(TRK_PATH)/%.o	: $(TRK_PATH)/%.c
	@echo Compiling $<
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   PVT
#----------------------------------------------------------------------
$(PVT_PATH)/%.o	: $(PVT_PATH)/%.c
	@echo Compiling $<	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   utility
#----------------------------------------------------------------------
$(UTILITY_PATH)/%.o	: $(UTILITY_PATH)/%.c
	@echo Compiling $< 
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   utility GNSS
#----------------------------------------------------------------------
$(UTL_GNSS_PATH)/%.o	: $(UTL_GNSS_PATH)/%.c
	@echo Compiling $< 	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   OSNMA
#----------------------------------------------------------------------
$(OSNMA_PATH)/%.o	: $(OSNMA_PATH)/%.c
	@echo Compiling $< 	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   CHIMERA
#----------------------------------------------------------------------
$(CHIMERA_PATH)/%.o	: $(CHIMERA_PATH)/%.c
	@echo Compiling $< 	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   NeQuick
#----------------------------------------------------------------------
$(NEQUICK_PATH)/%.o	: $(NEQUICK_PATH)/%.c
	@echo Compiling $< 	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@	
#----------------------------------------------------------------------	
#   support functions for GUI
#----------------------------------------------------------------------
$(GUI_PATH)/%.o	: $(GUI_PATH)/%.c
	@echo Compiling $< 	
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   receiver
#----------------------------------------------------------------------
$(RX_PATH)/%.o	: $(RX_PATH)/%.c
	@echo Compiling $<
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@
#----------------------------------------------------------------------	
#   folder with assembly sources
#----------------------------------------------------------------------
$(ASM_PATH)/%.o	: $(ASM_PATH)/%.c
	@echo Compiling $<
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@ 	
#----------------------------------------------------------------------	
#   folder with test Crypto Functions
#----------------------------------------------------------------------
$(TESTCRYPTO_PATH)/%.o	: $(TESTCRYPTO_PATH)/%.c
	@echo Compiling $<
	$(CC)  $(C_FLAGS) $(SW_INCLUDE_PATH) -c    $< -o $@ 	
#----------------------------------------------------------------------	


############################################################
#                     LIBRARIES
############################################################
# In order to compile, these libraries must be installed in
# the system:
# libpci-dev, libusb-1.0-0-dev, libncurses5-dev, libncursesw5-dev, openssl, libssl-dev
#  -lubsan : add when -fsanitize=undefined is used
LIBRARIES = -lssl -lcrypto -lpci -lcrypt -lusb-1.0 -lm -lncurses

############################################################
#                     LINKING
############################################################
$(MAIN_NAME)   : $(ALL_OBJ)
	@echo Linking...
	$(CC) $(ALL_OBJ) $(LIBRARIES) $(LINKING_FLAG) -o $(OUTPUT_PATH)/$(MAIN_NAME)

############################################################
#                     CLEANING
############################################################	
clean:
	@echo Cleaning the project...
	rm -f $(ALL_OBJ)
	rm -f $(SECURE_PATH)/*~
	rm -f $(FE_PATH)/*~
	rm -f $(WARM_PATH)/*~
	rm -f $(ACQ_PATH)/*~
	rm -f $(TRK_PATH)/*~
	rm -f $(PVT_PATH)/*~
	rm -f $(UTILITY_PATH)/*~
	rm -f $(UTL_GNSS_PATH)/*~
	rm -f $(OSNMA_PATH)/*~
	rm -f $(CHIMERA_PATH)/*~
	#rm -f $(CHIMERA_PATH)/testCryptoFunctions/*.txt
	rm -f $(CHIMERA_PATH)/testCryptoFunctions/*.csv
	rm -f $(NEQUICK_PATH)/*~
	rm -f $(GUI_PATH)/*~
	rm -f $(RX_PATH)/*~
	rm -f $(OUTPUT_PATH)/$(MAIN_NAME)
	rm -f $(NGENE_PATH)/*.txt 
	rm -f $(NGENE_PATH)/*.bin
	rm -f $(NGENE_PATH)/*.kml
	rm -f $(NGENE_PATH)/*.alm
	rm -f $(NGENE_PATH)/*.mat
	rm -f $(NGENE_PATH)/*~
	rm -f $(TESTCRYPTO_PATH)/*~


############################################################
#                    CLEAN & COMPILE
############################################################
redo:	clean release
