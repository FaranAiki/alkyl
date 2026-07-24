" Vim syntax file
" Language: Alkyl
" Latest Revision: 24 July 2026

if exists("b:current_syntax")
  finish
endif


" Keywords
syn keyword alkylStatement return break continue defer emit purge link import as is in has meta premeta postmeta define defined accept reject flux leak once reason alir
syn keyword alkylConditional if else elif then switch case default
syn keyword alkylRepeat while for loop
syn keyword alkylKeyword let mut imut immutable const final pristine reactive covalent inert wash pure impure tainted untaint residue clean errnum not mutable
syn keyword alkylModifier public private open closed extern naked abstract exact method container frame pragma
syn keyword alkylTypeKeyword class struct enum union namespace typedef compound type
syn keyword alkylBuiltinType int void char bool single double short long unsigned typeof sizeof alignof hasmethod hasattribute
syn keyword alkylBoolean true false null
syn keyword alkylOperatorKeyword infop prefop suffop infmut premut sufmut

" Numbers
syn match alkylNumber "\<\d\+\>"
syn match alkylFloat "\<\d\+\.\d\+\>"

" Strings and Chars
syn region alkylString start=/"/ skip=/\\"/ end=/"/
syn region alkylCString start=/c"/ skip=/\\"/ end=/"/
syn match alkylChar /'.'|'\\.'/

" Types/Classes (PascalCase)
syn match alkylClass "\<[A-Z][a-zA-Z0-9_]*\>"

" Functions
syn match alkylFunction "\<[a-zA-Z_][a-zA-Z0-9_]*\>\s*("me=e-1

" Delimiters and Punctuation
syn match alkylDelimiter "[(){}\[\];,]"
syn match alkylOperator "[+\-*/%=<>!&|^~]"
syn match alkylDot "\."

" Comments (must be after operators so they take precedence)
syn match alkylComment "//.*$"
syn region alkylBlockComment start="/\*" end="\*/"

" Highlighting Links
hi def link alkylStatement Statement
hi def link alkylConditional Conditional
hi def link alkylRepeat Repeat
hi def link alkylKeyword Keyword
hi def link alkylModifier StorageClass
hi def link alkylTypeKeyword Structure
hi def link alkylBuiltinType Type
hi def link alkylClass Type
hi def link alkylFunction Function
hi def link alkylBoolean Boolean
hi def link alkylOperatorKeyword Keyword

hi def link alkylNumber Number
hi def link alkylFloat Float
hi def link alkylString String
hi def link alkylCString String
hi def link alkylChar Character

hi def link alkylComment Comment
hi def link alkylBlockComment Comment

hi def link alkylDelimiter Delimiter
hi def link alkylOperator Operator
hi def link alkylDot Special

let b:current_syntax = "alkyl"
