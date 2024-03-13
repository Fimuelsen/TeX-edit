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

echo "Checking for dependencies."
if command -v touch >/dev/null 2>&1 ; then
    echo "touch found"
else
    echo "touch not found."
    echo "touch can be installed with 'apt get install touch'"
fi

if command -v zip >/dev/null 2>&1 ; then
    echo "zip found"
else
    echo "zip not found."
    echo "zip can be installed with 'apt get install zip'"
fi

if command -v python3 >/dev/null 2>&1 ; then
    echo "python3 found"
else
    echo "python3 not found."
    echo "python3 can be installed with 'apt get install python3'"
fi

if command -v xsel >/dev/null 2>&1 ; then
    echo "xsel found"
else
    echo "xsel not found."
    echo "xsel can be installed with 'apt get install xsel'"
fi

if command -v pdflatex >/dev/null 2>&1 ; then
    echo "pdflatex found"
else
    echo "pdflatex not found. TeX-edit will not be able to compile documents without pdflatex"
    echo "pdflatex can be installed with 'apt get install pdflatex'."
fi

echo "If you are having issues with latex packages, such as tickcd, we recommend installing the packages:" 
echo "'texlive-latex-recommended' and 'texlive-latex-extra'."


