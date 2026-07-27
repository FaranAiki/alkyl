" Vim syntax file for ALIR (Alkyl Intermediate Representation)
" Language: ALIR
" Filetype: alir

" Quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

" Comments
syn match alirComment ";.*$"
syn region alirComment start="/\*" end="\*/"

" Keywords
syn keyword alirKeyword func flux promise block if onstack panic

" Instructions
syn keyword alirInstruction alloc alloc4 store load call ret jmp if switch bitcast sizeof alignof typeof defined getptr phi mov fallback yield iter_init iter_valid iter_next iter_get free_stack

" Arithmetic / logical ops
syn keyword alirInstruction add sub mul div mod and or xor not shl shr sar rotl rotr eq lt gt lte gte neq

" Types
syn keyword alirType int long float double char bool void any unknown def ld ll array

" Constants
syn match alirConstant "\<\d\+\>"
syn match alirConstant "\<\d\+\.\d*\>"
syn match alirConstant "\<0x\x\+\>"
syn match alirConstant "\<0b[01]\+\>"
syn match alirConstant "\<\d\+[eE][+-]\?\d\+\>"
syn match alirConstant "\<\d\+\.\d*[eE][+-]\?\d\+\>"

" Values
syn match alirTemp "%%[a-zA-Z0-9_]\+"
syn match alirGlobal "@[a-zA-Z0-9_]\+"
syn match alirLabel "\<entry\>\|\<then\>\|\<else\>\|\<merge\>\|\<while_cond\>\|\<while_body\>\|\<untaint_merge\>"

" Operators
syn match alirOperator "<-"
syn match alirOperator "="
syn match alirOperator ","
syn match alirOperator "("
syn match alirOperator ")"

" String literals
syn region alirString start='"' end='"' skip='\\"'

" Highlights
hi def link alirComment Comment
hi def link alirKeyword Keyword
hi def link alirInstruction Statement
hi def link alirType Type
hi def link alirConstant Constant
hi def link alirTemp Identifier
hi def link alirGlobal Identifier
hi def link alirLabel Label
hi def link alirOperator Operator
hi def link alirString String

let b:current_syntax = "alir"
