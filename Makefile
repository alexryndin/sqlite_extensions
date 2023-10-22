CFLAGS=-O2 -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wimplicit-function-declaration -pedantic -DNDEBUG         \
			 -isystemcontrib/sds \
			 -isystemcontrib/md4c\
			 -I.           \
			 -fPIC         \
			 $(OPTFLAGS)   \

LDLIBS=-ldl -pedantic -lsqlite3 \
       $(OPTLIBS)

EXTERNAL_SRC=$(wildcard contrib/**/src/*.c contrib/**/*.c)
EXTERNAL_SRC_NO_TESTS=$(filter-out %test.c, $(EXTERNAL_SRC))
EXTERNAL=$(patsubst %.c,%.o,$(EXTERNAL_SRC_NO_TESTS))

all: dbw_extension.so

dev: CFLAGS := $(filter-out -O2,$(CFLAGS))
dev: CFLAGS := $(filter-out -DNDEBUG,$(CFLAGS))
dev: CFLAGS := $(filter-out -pedantic,$(CFLAGS))
dev: CFLAGS += -g
dev: all

LDFLAGS := $(CFLAGS)
dbw_extension.so: dbw_extension.c
		$(CC) $(LDFLAGS) -shared -o $@ $(EXTERNAL_SRC) dbw_extension.c
