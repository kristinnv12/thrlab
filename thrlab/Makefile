.PHONY: all clean handin check

USER_1 = $(shell grep -E '^[ \t]*\* *User 1:' main.c | sed -e 's/\*//g' -e 's/ *User 1: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')
USER_2 = $(shell grep -E '^[ \t]*\* *User 2:' main.c | sed -e 's/\*//g' -e 's/ *User 2: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')

all: thrlab thrlab-asan thrlab-tsan

clean:
	rm -f help.o main.o thrlab thrlab-asan thrlab-tsan

handin:
	@echo "User 1: \"$(USER_1)\""
	@echo "User 2: \"$(USER_2)\""

	@if [ "$(USER_1)" != "NONE" ]; then getent passwd $(USER_1) > /dev/null; if [ $$? -ne 0 ]; then echo "User $(USER_1) does not exist on Skel."; exit 2; fi; fi
	@if [ "$(USER_2)" != "NONE" ]; then getent passwd $(USER_2) > /dev/null; if [ $$? -ne 0 ]; then echo "User $(USER_2) does not exist on Skel."; exit 3; fi; fi
	rutool handin -c sty15 -p thrlab *

check:
	rutool check -c sty15 -p thrlab

help.o: help.c help.h
	${CC} -std=gnu11 -Wall -Wextra -pedantic -ggdb3 -fPIE -c -o help.o help.c

main.o: main.c help.h
	${CC} -std=gnu11 -Wall -Wextra -pedantic -ggdb3 -fPIE -c -o main.o main.c

thrlab: help.o main.o
	${CC} -lpthread -o thrlab help.o main.o

thrlab-asan: help.o main.o
	${CC} -lpthread -fsanitize=address -ggdb3 -o thrlab-asan help.o main.o

thrlab-tsan: help.o main.o
	${CC} -lpthread -fsanitize=thread -ggdb3 -pie -o thrlab-tsan help.o main.o
