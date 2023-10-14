CFLAGS=-O2 -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wimplicit-function-declaration -pedantic -DNDEBUG \
			 -Icontrib/sds                                               \
			 -Icontrib/md4c                                               \
			 -I.                                               \
			 -fPIC                                                                         \
			 $(OPTFLAGS)                                                                   \
			 $(shell pkg-config lua$(LUA_VER) --cflags)

LDLIBS=-ldl -pedantic -lsqlite3 \
			 $(shell pkg-config lua$(LUA_VER) --libs)              \
       $(OPTLIBS)

PREFIX?=/usr/local

EXTERNAL_SRC=$(wildcard contrib/**/src/*.c contrib/**/*.c)
EXTERNAL_SRC_NO_TESTS=$(filter-out %test.c, $(EXTERNAL_SRC))
EXTERNAL=$(patsubst %.c,%.o,$(EXTERNAL_SRC_NO_TESTS))

all: $(BIN) dbw_extension.so

dev: CFLAGS := $(filter-out -O2,$(CFLAGS))
dev: CFLAGS := $(filter-out -DNDEBUG,$(CFLAGS))
dev: CFLAGS := $(filter-out -pedantic,$(CFLAGS))
dev: CFLAGS += -g
dev: all

EXTERNAL_SHARED_SRC=$(wildcard ../contrib/**/bstring/bstrlib.c contrib/md4c/src/*.c )
EXTERNAL_SHARED_SRC_NO_TESTS=$(filter-out %test.c, $(EXTERNAL_SHARED_SRC))
EXTERNAL_SHARED=$(patsubst %.c,%.o,$(EXTERNAL_SHARED_SRC_NO_TESTS))

$(EXTERNAL_SHARED): CFLAGS += -fPIC
$(EXTERNAL_SHARED):

LDFLAGS := $(CFLAGS)
dbw_extension.so: $(EXTERNAL_SHARED)
		$(CC) $(LDFLAGS) -shared -o $@ $(EXTERNAL_SRC) dbw_extension.c

OUT_DIR ?= .
TARGET_LIB=$(OUT_DIR)/libdbw.a

