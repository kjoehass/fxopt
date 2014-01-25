GCC= gcc
RM= rm -rf
UTHASH_DIR= /home/kjh016/include/uthash/src

PLUGIN_SOURCE_FILES= fxopt_affine.c fxopt_range.c fxopt_utils.c fxopt_stmts.c fxopt_plugin.c
PLUGIN_OBJECT_FILES= $(patsubst %.c,%.o,$(PLUGIN_SOURCE_FILES))
GCCPLUGINS_DIR= $(shell $(GCC) -print-file-name=plugin)
CFLAGS+= -Wall -pedantic -std=c99 -ggdb -I$(GCCPLUGINS_DIR)/include -I$(UTHASH_DIR) -fPIC

fxopt.so: $(PLUGIN_OBJECT_FILES)
	$(GCC) -lm -shared $^ -o $@

$(PLUGIN_OBJECT_FILES) : fxopt_plugin.h

clean:
	-$(RM) $(PLUGIN_OBJECT_FILES) *.i fxopt.so
	-@echo ' '

