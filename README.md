# Ocaml C Compile Execute Clean: A Simple Tool for Building/Executing Single-File Projects Easily
This project is a small program I use to compile my single file projects without writing a Makefile or just manually typing commands.

This is assumed to be kinda useless, the only purpose of that tool is to save 5 sec at each compilation or like 30 sec per Makefile. 

## How to install
1. Clone the repo.
2. run `make build`, because writing one entire command is so long.
3. finally copy `occec` to `/usr/local/bin` with `cp occec /usr/local/bin`, programs in this folder can be called from everywhere, you'll need admin perms to copy there.

You're all set! You can now compile any `.c` or `.ml` file using `occec`.

If you want to not clean the compiled code, use the `--no-clear` flag. Also note that all flags are passed to the compiler, however the `-o` flag is not supported.

*PS : you can change the #define if for example you want to use `gcc` instead of `clang`* 
