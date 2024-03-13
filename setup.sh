#!/bin/bash
make
cp TeX-edit /usr/local/bin/
chmod a+x /usr/local/bin/TeX-edit
cp tools/TeX-edit-compile /usr/local/bin/
chmod a+x /usr/local/bin/TeX-edit-compile
cp tools/TeX-edit-extractPDF /usr/local/bin/
chmod a+x /usr/local/bin/TeX-edit-extractPDF
cp tools/TeX-edit-make /usr/local/bin/
chmod a+x /usr/local/bin/TeX-edit-make
mkdir /usr/local/src/TeX-edit
cp tools/template.pdf /usr/local/TeX-edit/
cp -r polyglot /usr/local/src/TeX-edit/
chmod a+x /usr/local/src/TeX-edit/polyglot/truepolyglot
mkdir /tmp/TeX-edit

