# TeX-edit - A TeX Editor in Your Terminal
TeX-edit is a quick and dirty terminal based TeX editor for Linux. It is tested to work on Ubuntu.

## How to use
To open a TeX-edit compatible pdf file with the TeX-editor, open a terminal and do 
```
TeX-edit filename.pdf
```
If the file doesn't exists one will be made from a standard template. 
If no filename is provided, the TeX-editor will prompt the user for one when attempting to save.
To extract a pure PDF file from a TeX-edit-pdf file do 
```
TeX-edit-extractPDF filename.pdf
```
To recompile a TeX-edit-pdf file without opening the TeX-editor do 
```
TeX-edit-compile filename.pdf
```
To make a TeX-edit compatible pdf file out of a 'main.tex' and a 'main.pdf' file in the working directory do 
```
TeX-edit-make filename.pdf
```

### Commands
Ctrl-S = Save & Compile
Ctrl-Q = Quit
Ctrl-P = Force Quit
Ctrl-R = Move cursor to left
Ctrl-H = Delete the highlighted text or if none is highlighted deletes the previous character 
Ctrl-O = Open PDF file with default (PDF) viewer 
Ctrl-K = Create new SVG file and open it in Inkscape.

Ctrl-C = Copy highlighted test to clipboard
Ctrl-V = Paste text from clipboard
Ctrl-X = Cut highlighted test to clipboard
Shift-Rightarrow = Highlight text one character to the right
Shift-Leftarrow = Highlight text one character to the left
Shift-Uparrow = Highlight text until the character above the current one
Shift-Downarrow = Highlight text until the character below the current one
Ctrl-Shift-Rightarrow = Highlight text until next space on right or end of line
Ctrl-Shift-Leftarrow = Highlight text until next space on left or start of line

Ctrl-Rightarrow = Move right until next space or end of line
Ctrl-Leftarrow = Move left until next space or start of line
Ctrl-Uparrow = Interchange current line with the line above
Ctrl-Downarrow = Interchange current line with the line below

Page UP = Move cursor 1 page worth of lines up
Page DOWN = Move cursor 1 page worth of lines down
HOME = Move to start of line
END  = Move to end of line

TAB = Autocomplete if possible, else puts a tab. Note that tabs currently looks indistinguishable from spaces.

## Features
TeX-edit comes with autocomplete and syntax highlighting to speed up your typesetting and 
reduce the time spent debugging.
### Autocomplete
Some LaTeX commands can be rather long. Autocomplete allows TAB to be used to autocomplete a LaTeX command. 
For example, when I type \xr and press TAB, the command will be expanded to \xrightarrow{} placing the cursor 
between the two curly brackets. 
![alt text](example1.svg)

Typing \be and pressing TAB will create both \begin{} and \end{} while positioning the cursor between the curly bracket. 
Moreover when filling out a "\begin{}" the TeX-editor will automatically fill out the "\end{}"-portion aswell. 
![alt text](example2.svg)

### Highlighting
Highlighting will automatically detect and highlight any typos or incorrectly typed LaTeX commands. 
Text in mathmode is always colored green, so if you forget a "$"-sign somwhere, it will be very obvious!

### Debug
TeX-editor can help you debug by automatically highlighting the relevant part of the compile error message.

## Installing
Run setup.sh to compile the source code and setup the necessary system files. 
```
sudo bash setup.sh
```

### Dependencies
truepolyglot (version 1.6.2)
pdflatex (pdfTeX 3.14159265-2.6-1.40.20 (TeX Live 2019/Debian) kpathsea version 6.3.1)
zip (version 3.0)
python3 (version 3.8.5)
xsel (version 1.2.0 by Conrad Parker)
touch

## License
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

