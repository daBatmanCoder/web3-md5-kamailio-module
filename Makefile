# Kamailio Web3 Authentication Module Makefile
# 
# This Makefile compiles the web3_auth module for Kamailio
# Make sure to have Kamailio development headers installed

include ../../Makefile.defs
auto_gen=
NAME=web3_auth.so
LIBS=-lcurl

DEFS+=-DKAMAILIO_MOD -DMOD_NAME='"web3_auth"'

# Enable optimized compilation
DEFS+=-O2 -g

# Additional include paths if needed
INCLUDES += -I../../
INCLUDES += -I../../lib/

# Source files
SOURCES = web3_auth_module.c

# Module export file (the main source)
$(NAME): web3_auth_module.o
	$(LD) $(LDFLAGS) $(INCLUDES) -shared -o $@ $< $(LIBS)

web3_auth_module.o: web3_auth_module.c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -fPIC -c $< -o $@

.PHONY: all clean install

all: $(NAME)

clean:
	rm -f *.o *.so

install: $(NAME)
	mkdir -p $(modules-prefix)/$(modules-dir)
	$(INSTALL-TOUCH) $(modules-prefix)/$(modules-dir)/$(NAME)
	$(INSTALL-MODULES) $(NAME) $(modules-prefix)/$(modules-dir)

# For standalone compilation (without full Kamailio build environment)
standalone:
	gcc -fPIC -shared -O2 -g \
		-DKAMAILIO_MOD -DMOD_NAME='"web3_auth"' \
		-I. \
		web3_auth_module.c \
		-lcurl \
		-o web3_auth_module.so

# Test compilation (creates object files but doesn't link)
test-compile:
	gcc -fPIC -O2 -g \
		-DKAMAILIO_MOD -DMOD_NAME='"web3_auth"' \
		-I. \
		-c web3_auth_module.c \
		-o web3_auth_module.o

# Help target
help:
	@echo "Kamailio Web3 Auth Module Build Targets:"
	@echo "  all          - Build the module (requires Kamailio build environment)"
	@echo "  standalone   - Build standalone module (basic compilation)"
	@echo "  test-compile - Test compilation without linking"
	@echo "  clean        - Remove built files"
	@echo "  install      - Install module to Kamailio modules directory"
	@echo "  help         - Show this help" 