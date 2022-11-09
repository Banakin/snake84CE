# ----------------------------
# Set NAME to the program name
# Set ICON to the png icon file name
# Set DESCRIPTION to display within a compatible shell
# Set COMPRESSED to "YES" to create a compressed program
# ----------------------------

NAME        ?= SNAKE
COMPRESSED  ?= NO
ICON        ?= iconc.png
DESCRIPTION ?= "Snake for the TI-84 Plus CE"

# ----------------------------

# include $(CEDEV)/include/.makefile
include $(shell cedev-config --makefile)
