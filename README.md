# Ocaml C Compile Execute Clean - a simple tool for building/executing single-file projects easily
This project is a small program I use to compile my single file projects without writing a makefile or just typing commands.

This is assumed to be kinda useless, the only purpose of that script is to save 5 sec at each compilation or like 30 sec per makefile. 

## How to install
1. Clone the repo
2. run `make build` - because writing one entire command is so long
3. finally copy `occec` to `/usr/local/bin` with `cp occec /usr/local/bin` - programs in this folder can be called from everywhere, you need admin perms to copy to that folder

You're all set! You can now compile any .c or .ml file using `occec`

*PS : you can change the #define if for example you want to use `gcc` instead of `clang`* 
