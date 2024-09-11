#!/bin/bash
if command -v make >/dev/null 2>&1 ; then
    echo "make found"
else
    echo "make not found. It can be installed with: \n sudo apt install make"
    exit
fi
if command -v gcc >/dev/null 2>&1 ; then
    echo "gcc found"
    exit
else
    echo "gcc not found. It can be installed with: \n sudo apt install make"
    exit
fi
if command -v g++ >/dev/null 2>&1 ; then
    echo "g++ found"
else
    echo "g++ not found. It can be installed with: \n sudo apt install make"
    exit
fi

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
cp tools/template.pdf /usr/local/src/TeX-edit/
cp -r polyglot /usr/local/src/TeX-edit/
chmod a+x /usr/local/src/TeX-edit/polyglot/truepolyglot

echo "Checking for dependencies."
if command -v touch >/dev/null 2>&1 ; then
    echo "touch found"
else
    echo "touch not found."
fi

if command -v zip >/dev/null 2>&1 ; then
    echo "zip found"
else
    echo "zip not found. Do you wish to install zip?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes ) apt install zip; break;;
            No ) break;;
        esac
    done
fi

if command -v python3 >/dev/null 2>&1 ; then
    echo "python3 found"
else
    echo "python3 not found. Do you wish to install python3?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes ) apt install python3; break;;
            No ) break;;
        esac
    done
fi

if command -v xsel >/dev/null 2>&1 ; then
    echo "xsel found"
else
    echo "xsel not found. Do you wish to install xsel?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes ) apt install xsel; break;;
            No ) break;;
        esac
    done
fi

if command -v pdflatex >/dev/null 2>&1 ; then
    echo "pdflatex found"
else
    echo "pdflatex not found. TeX-edit will not be able to compile documents without pdflatex."
    echo "Do you wish to install pdflatex as texlive-latex-recommended?"
    select yn in "Yes" "No"; do
        case $yn in
            Yes ) apt install texlive-latex-recommended; break;;
            No ) break;;
        esac
    done
fi

echo "For extended latex features, such as tikzcd, we recommend installing the packages:" 
echo "'texlive-latex-extra'. Do you want to install 'texlive-latex-extra'?"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) apt install texlive-latex-extra; break;;
        No ) exit;;
    esac
done
