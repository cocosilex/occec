CC = clang

FLAGS = -Wall -Wextra -fsanitize=address

SRC = main.c
OUT = occec

build: 
	$(CC) $(SRC) -o $(OUT) $(FLAGS)

prod:
	$(CC) $(SRC) -o $(OUT)
