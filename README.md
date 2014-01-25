# fxopt

fxopt is a plugin for the GNU compiler collection (gcc) that converts
floating-point or integer calculations into fixed-point arithmetic. It directly
modifies the internal representation used in gcc, so the result of this
conversion is a typical object file or assembly language file. In addition,
fxopt can produce a transcript that documents the conversion process and the
fixed-point formats of the result and intermediate variables. Using the
provided perl script you can also create generic C code that exactly duplicates
the fixed-point calculations.

A gcc plugin that performs floating-point to fixed-point arithemetic conversions.
