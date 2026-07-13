" Vim syntax file
" Language: Alkyl (.aky)

if exists("b:current_syntax")
  finish
endif

syn keyword alkylKeyword accept alignof alir as break case class clean closed const continue default defer define defined elif else emit enum extern final flux for has hasattribute hasmethod if immutable impl import impure imut in inert is leak let link loop meta mut mutable naked namespace not once open postmeta premeta pristine private public pure purge reactive reason reject residue return sizeof struct switch tainted then trait typedef typeof union untaint wash while print

syn keyword alkylType bool char double int long short single string unsigned vector void

syn keyword alkylBoolean true false

syn region alkylString start='"' end='"'
syn region alkylChar start="'" end="'"
syn match alkylNumber "\<\d\+\>"
syn match alkylFloat "\<\d\+\.\d\+\>"

syn match alkylComment "//.*$"
syn region alkylBlockComment start="/\*" end="\*/"

syn match alkylOperator "?"

hi def link alkylKeyword Statement
hi def link alkylType Type
hi def link alkylBoolean Boolean
hi def link alkylString String
hi def link alkylChar Character
hi def link alkylNumber Number
hi def link alkylFloat Float
hi def link alkylComment Comment
hi def link alkylBlockComment Comment
hi def link alkylOperator Operator

let b:current_syntax = "alkyl"
