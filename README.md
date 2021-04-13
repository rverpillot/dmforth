# DMFORTH
DMFORTH is a FORTH port for Swissmicros DM42 calculator (probably works on DM41X). 

This is a work in progress. Bugs could crash your calculator !

## Build
- Install GNU ARM toolchain
- Launch `make`

## Install
- Connect USB cable from DM42 to your computer.
- Backup your data !
- Activate USB disk.
- Copy build/DMFORTH.pgm to root of calculator disk.
- Eject the calculator disk.
- In DMCP menu, load it with "Load Program" item.

## Run
### Normal mode (aka calculator mode)
In this mode, you can view 5 items of the stack and you have one line of input. Words associated with keys are executed immediately. 

Example, if you enter the number `12` and press `SIN` key, DMFORTH evaluate the line "`12 sin`", so the number `12` is pushed to the stack, and the word `sin` is executed and the result is pushed back to the stack.

You can use all forth words with the function menu (keys `F1`-`F6` and arrows keys).

![](docs/calc.bmp)


### Program mode
To enter or exit: `SHIFT` `PRGM`

In this mode, the keyboard is switched to alpha mode (use `SHIFT` `ALPHA` to switch between uppercase and lowercase). To enter numbers or some operators characters (`+`,`-`, ...), use `SHIFT` and the right key. You can enter up to 6 lines of text, and `ENTER` key evaluate or compile input text.

TODO: show keyboard layout

![](docs/prgm.bmp)

The function menu works in completion mode, so it's live updated when you press keys. 

![](docs/completion.bmp)

you can go back in the history input texts with `SHIFT` and `UP`  or `DOWN` keys. In editor mode, cursor can be moved on the line (TODO: editor mode).



### Output
When words output some text, DMFORTH erase display and switch to output mode, and if rows are greater than display lines, DMFORTH use a basic "page mode". 

![](docs/output.bmp)


### Load FORTH programs
- Copy your FORTH program (extension: `.zf`) to calculator disk in `FORTH` folder.
- Use `SHIFT` and `0` keys to enter in the setup menu.
- Choose `Load program` item to load it.


## FORTH
- DMFORTH use a modified version of zforth (https://github.com/zevv/zForth). This is not an ANS Forth.
- DMFORTH use a heap memory block of 64 Ko to store words (32 Ko), PAD zone (16 Ko), data stack and return stack (16 Ko).
- PAD zone stores temporary strings. When this memory is full, PAD pointer is reset.
- `s" Hello"` can be written `"Hello"` .

TODO: list words





