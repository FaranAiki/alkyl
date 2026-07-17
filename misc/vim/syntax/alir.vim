" Vim syntax file
" Language: Alkyl Intermediate Representation (.alir)

if exists("b:current_syntax")
  finish
endif

" Keywords
syn keyword alirKeyword block func store load call return onstack sizeof bitcast getptr promise type struct
syn match alirLabel "^\s*[a-zA-Z0-9_]\+:"

" Types
syn keyword alirType int double float char void string array vec hashmap any unknown

" Registers and Globals
syn match alirRegister "%[a-zA-Z0-9_.*]\+"
syn match alirGlobal "@[a-zA-Z0-9_.*]\+"

" Literals
syn region alirString start='"' end='"'
syn match alirNumber "\<\d\+\>"
syn match alirFloat "\<\d\+\.\d\+\>"

" Comments
syn match alirComment ";.*$"

" Highlighting Links
hi def link alirKeyword Statement
hi def link alirLabel Label
hi def link alirType Type
hi def link alirRegister Identifier
hi def link alirGlobal Constant
hi def link alirString String
hi def link alirNumber Number
hi def link alirFloat Float
hi def link alirComment Comment

let b:current_syntax = "alir"
