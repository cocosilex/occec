CC = clang

FLAGS = -Wall -Wextra -Wshadow -Wpedantic -fsanitize=address

SRC = main.c
OUT = occec

build: 
	$(CC) $(SRC) -o $(OUT) $(FLAGS) -Og

prod:
	$(CC) $(SRC) -o $(OUT) -O2 -DNDEBUG
