# sammallus
Minimalistic flavor of LISP built according to `http://buildyourownlisp.com/`

### Building
MacOS
`cc -std=c99 -Wall parsing.c mpc.c -ledit -o bin/parsing`

### Using
The binary is built to `bin/parsing` with the above command, so run `./bin/parsing` on your terminal.

### What is the parser you are using?
`https://github.com/orangeduck/mpc`
