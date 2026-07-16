" Vim syntax file
" Language: Alkyl
" Latest Revision: 14 July 2026

if exists("b:current_syntax")
  finish
endif

" Keywords
syn keyword alkylStatement return break continue defer emit purge link import as is in has meta premeta postmeta
syn keyword alkylConditional if else elif then switch case default
syn keyword alkylRepeat while for loop
syn keyword alkylKeyword let mut const final pristine reactive covalent inert wash pure impure tainted untaint residue clean
syn keyword alkylModifier public private open closed extern naked abstract exact method container frame pragma
syn keyword alkylTypeKeyword class struct enum union trait impl namespace typedef compound type
syn keyword alkylBuiltinType int void char bool single double short long unsigned typeof sizeof alignof
syn keyword alkylBoolean true false
syn keyword alkylOperatorKeyword infop prefop suffop infmut premut sufmut

" Numbers
syn match alkylNumber "\<\d\+\>"
syn match alkylFloat "\<\d\+\.\d\+\>"

" Strings and Chars
syn region alkylString start=/"/ skip=/\\"/ end=/"/
syn region alkylCString start=/c"/ skip=/\\"/ end=/"/
syn match alkylChar /'.'|'\\.'/

" Comments
syn match alkylComment "//.*$"
syn region alkylBlockComment start="/\*" end="\*/"

" Types/Classes (PascalCase)
syn match alkylClass "\<[A-Z][a-zA-Z0-9_]*\>"

" Highlighting Links
hi def link alkylStatement Statement
hi def link alkylConditional Conditional
hi def link alkylRepeat Repeat
hi def link alkylKeyword Keyword
hi def link alkylModifier StorageClass
hi def link alkylTypeKeyword Structure
hi def link alkylBuiltinType Type
hi def link alkylClass Type
hi def link alkylBoolean Boolean
hi def link alkylOperatorKeyword Keyword

hi def link alkylNumber Number
hi def link alkylFloat Float
hi def link alkylString String
hi def link alkylCString String
hi def link alkylChar Character

hi def link alkylComment Comment
hi def link alkylBlockComment Comment

let b:current_syntax = "alkyl"
