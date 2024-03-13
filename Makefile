CC = gcc
TeX-edit: TeX-editor.cpp
	$(CC) TeX-editor.cpp -o TeX-edit -Wall -Wextra -pedantic -lX11
