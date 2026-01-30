override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-const-variable -g -O0 -fsanitize=address,undefined,leak

ifdef CI
override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter -Wno-unused-const-variable
endif

NAME=ops-lidl

.PHONY: clean all

all: ${NAME}

${NAME}: ${NAME}.c
	$(CC) $(CFLAGS) -o ${NAME} ${NAME}.c

clean:
	rm -f ${NAME}
