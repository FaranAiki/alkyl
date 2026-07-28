vim9script
g:SessionLoad = 1
if &cp | set nocp | endif
var cpo_save: string
cpo_save = &cpo
set cpo&vim
inoremap <silent> <SNR>30_AutoPairsReturn =AutoPairsReturn()
imap <silent> <C-Y>m <Plug>(emmet-merge-lines)
inoremap <Plug>(emmet-merge-lines) =emmet#util#closePopup()=emmet#mergeLines()
imap <silent> <C-Y>A <Plug>(emmet-anchorize-summary)
inoremap <Plug>(emmet-anchorize-summary) =emmet#util#closePopup()=emmet#anchorizeURL(1)
imap <silent> <C-Y>a <Plug>(emmet-anchorize-url)
inoremap <Plug>(emmet-anchorize-url) =emmet#util#closePopup()=emmet#anchorizeURL(0)
imap <silent> <C-Y>k <Plug>(emmet-remove-tag)
inoremap <Plug>(emmet-remove-tag) =emmet#util#closePopup()=emmet#removeTag()
imap <silent> <C-Y>j <Plug>(emmet-split-join-tag)
inoremap <Plug>(emmet-split-join-tag) :call emmet#splitJoinTag()
imap <silent> <C-Y>/ <Plug>(emmet-toggle-comment)
inoremap <Plug>(emmet-toggle-comment) =emmet#util#closePopup()=emmet#toggleComment()
imap <silent> <C-Y>I <Plug>(emmet-image-encode)
inoremap <Plug>(emmet-image-encode) =emmet#util#closePopup()=emmet#imageEncode()
imap <silent> <C-Y>i <Plug>(emmet-image-size)
inoremap <Plug>(emmet-image-size) =emmet#util#closePopup()=emmet#imageSize()
inoremap <Plug>(emmet-move-prev-item) :call emmet#moveNextPrevItem(1)
inoremap <Plug>(emmet-move-next-item) :call emmet#moveNextPrevItem(0)
imap <silent> <C-Y>N <Plug>(emmet-move-prev)
inoremap <Plug>(emmet-move-prev) =emmet#util#closePopup()=emmet#moveNextPrev(1)
imap <silent> <C-Y>n <Plug>(emmet-move-next)
inoremap <Plug>(emmet-move-next) =emmet#util#closePopup()=emmet#moveNextPrev(0)
imap <silent> <C-Y>D <Plug>(emmet-balance-tag-outword)
inoremap <Plug>(emmet-balance-tag-outword) :call emmet#balanceTag(-1)
imap <silent> <C-Y>d <Plug>(emmet-balance-tag-inward)
inoremap <Plug>(emmet-balance-tag-inward) :call emmet#balanceTag(1)
imap <silent> <C-Y>u <Plug>(emmet-update-tag)
inoremap <Plug>(emmet-update-tag) =emmet#util#closePopup()=emmet#updateTag()
imap <silent> <C-Y>; <Plug>(emmet-expand-word)
inoremap <Plug>(emmet-expand-word) =emmet#util#closePopup()=emmet#expandAbbr(1,"")
imap <silent> <C-Y>, <Plug>(emmet-expand-abbr)
inoremap <Plug>(emmet-expand-abbr) =emmet#util#closePopup()=emmet#expandAbbr(0,"")
inoremap <silent> <Plug>(fzf-maps-i) :call fzf#vim#maps('i', 0)
legacy inoremap <expr> <Plug>(fzf-complete-buffer-line) fzf#vim#complete#buffer_line()
legacy inoremap <expr> <Plug>(fzf-complete-line) fzf#vim#complete#line()
legacy inoremap <expr> <Plug>(fzf-complete-file-ag) fzf#vim#complete#path('ag -l -g ""')
legacy inoremap <expr> <Plug>(fzf-complete-file) fzf#vim#complete#path("find . -path '*/\.*' -prune -o -type f -print -o -type l -print | sed 's:^..::'")
legacy inoremap <expr> <Plug>(fzf-complete-path) fzf#vim#complete#path("find . -path '*/\.*' -prune -o -print | sed '1d;s:^..::'")
legacy inoremap <expr> <Plug>(fzf-complete-word) fzf#vim#complete#word()
inoremap <M-C-Q> :qa!
inoremap <C-Q> :mkses!:wqa
inoremap <C-Right> :tabni
inoremap <C-Left> :tabpi
inoremap <C-S> :wi
inoremap <C-Up> gki<Right>
inoremap <C-Down> gji<Right>
inoremap <C-A> ggVG$
inoremap <M-N> :tabe
legacy inoremap <expr> <C-L> &relativenumber == 1 ? ':set number:set norelativenumberi' : ':set relativenumber:set numberi'
nnoremap  ggVG$
vmap  "+y
nnoremap  :NERDTreeFind
nnoremap <NL> gj
nnoremap  gk
legacy nnoremap <expr>  &relativenumber == 1 ? ':set number:set norelativenumber' : ':set relativenumber:set number'
nnoremap  :NERDTree
nnoremap  :mkses!:wqa
nnoremap  :w
nnoremap  :NERDTreeToggle
vmap  yy"+p
vmap <silent> c <Plug>(emmet-code-pretty)
nmap <silent> m <Plug>(emmet-merge-lines)
nmap <silent> A <Plug>(emmet-anchorize-summary)
nmap <silent> a <Plug>(emmet-anchorize-url)
nmap <silent> k <Plug>(emmet-remove-tag)
nmap <silent> j <Plug>(emmet-split-join-tag)
nmap <silent> / <Plug>(emmet-toggle-comment)
nmap <silent> I <Plug>(emmet-image-encode)
nmap <silent> i <Plug>(emmet-image-size)
nmap <silent> N <Plug>(emmet-move-prev)
nmap <silent> n <Plug>(emmet-move-next)
vmap <silent> D <Plug>(emmet-balance-tag-outword)
nmap <silent> D <Plug>(emmet-balance-tag-outword)
vmap <silent> d <Plug>(emmet-balance-tag-inward)
nmap <silent> d <Plug>(emmet-balance-tag-inward)
nmap <silent> u <Plug>(emmet-update-tag)
nmap <silent> ; <Plug>(emmet-expand-word)
vmap <silent> , <Plug>(emmet-expand-abbr)
nmap <silent> , <Plug>(emmet-expand-abbr)
nnoremap  :call AdjustFontSize(-1)
inoremap  :qa!
inoremap Î :tabe
nnoremap \n :NERDTreeFocus
xmap gx <Plug>(open-word-under-cursor)
nmap gx <Plug>(open-word-under-cursor)
xnoremap <Plug>(open-word-under-cursor) <ScriptCmd>vim9.Open(getregion(getpos('v'), getpos('.'), { type: mode() })->join())
nnoremap <Plug>(open-word-under-cursor) <ScriptCmd>vim9.Open(GetWordUnderCursor())
vmap <silent> <C-Y>c <Plug>(emmet-code-pretty)
vnoremap <Plug>(emmet-code-pretty) :call emmet#codePretty()
nmap <silent> <C-Y>m <Plug>(emmet-merge-lines)
nnoremap <Plug>(emmet-merge-lines) :call emmet#mergeLines()
nmap <silent> <C-Y>A <Plug>(emmet-anchorize-summary)
nnoremap <Plug>(emmet-anchorize-summary) :call emmet#anchorizeURL(1)
nmap <silent> <C-Y>a <Plug>(emmet-anchorize-url)
nnoremap <Plug>(emmet-anchorize-url) :call emmet#anchorizeURL(0)
nmap <silent> <C-Y>k <Plug>(emmet-remove-tag)
nnoremap <Plug>(emmet-remove-tag) :call emmet#removeTag()
nmap <silent> <C-Y>j <Plug>(emmet-split-join-tag)
nnoremap <Plug>(emmet-split-join-tag) :call emmet#splitJoinTag()
nmap <silent> <C-Y>/ <Plug>(emmet-toggle-comment)
nnoremap <Plug>(emmet-toggle-comment) :call emmet#toggleComment()
nmap <silent> <C-Y>I <Plug>(emmet-image-encode)
nnoremap <Plug>(emmet-image-encode) :call emmet#imageEncode()
nmap <silent> <C-Y>i <Plug>(emmet-image-size)
nnoremap <Plug>(emmet-image-size) :call emmet#imageSize()
nnoremap <Plug>(emmet-move-prev-item) :call emmet#moveNextPrevItem(1)
nnoremap <Plug>(emmet-move-next-item) :call emmet#moveNextPrevItem(0)
nmap <silent> <C-Y>N <Plug>(emmet-move-prev)
nnoremap <Plug>(emmet-move-prev) :call emmet#moveNextPrev(1)
nmap <silent> <C-Y>n <Plug>(emmet-move-next)
nnoremap <Plug>(emmet-move-next) :call emmet#moveNextPrev(0)
vmap <silent> <C-Y>D <Plug>(emmet-balance-tag-outword)
vnoremap <Plug>(emmet-balance-tag-outword) :call emmet#balanceTag(-1)
nmap <silent> <C-Y>D <Plug>(emmet-balance-tag-outword)
nnoremap <Plug>(emmet-balance-tag-outword) :call emmet#balanceTag(-1)
vmap <silent> <C-Y>d <Plug>(emmet-balance-tag-inward)
vnoremap <Plug>(emmet-balance-tag-inward) :call emmet#balanceTag(1)
nmap <silent> <C-Y>d <Plug>(emmet-balance-tag-inward)
nnoremap <Plug>(emmet-balance-tag-inward) :call emmet#balanceTag(1)
nmap <silent> <C-Y>u <Plug>(emmet-update-tag)
nnoremap <Plug>(emmet-update-tag) :call emmet#updateTag()
nmap <silent> <C-Y>; <Plug>(emmet-expand-word)
nnoremap <Plug>(emmet-expand-word) :call emmet#expandAbbr(1,"")
vmap <silent> <C-Y>, <Plug>(emmet-expand-abbr)
vnoremap <Plug>(emmet-expand-abbr) :call emmet#expandAbbr(2,"")
nmap <silent> <C-Y>, <Plug>(emmet-expand-abbr)
nnoremap <Plug>(emmet-expand-abbr) :call emmet#expandAbbr(3,"")
tnoremap <silent> <Plug>(fzf-normal) 
tnoremap <silent> <Plug>(fzf-insert) i
nnoremap <silent> <Plug>(fzf-normal) <Nop>
nnoremap <silent> <Plug>(fzf-insert) i
onoremap <silent> <Plug>(fzf-maps-o) :call fzf#vim#maps('o', 0)
xnoremap <silent> <Plug>(fzf-maps-x) :call fzf#vim#maps('x', 0)
nnoremap <silent> <Plug>(fzf-maps-n) :call fzf#vim#maps('n', 0)
vmap <C-C> "+y
vmap <C-V> yy"+p
nnoremap <M-C-Q> :qa!
nnoremap <C-Q> :mkses!:wqa
nnoremap <C-Right> :tabn
nnoremap <C-Left> :tabp
nnoremap <C-S> :w
nnoremap <C-Up> gk
nnoremap <C-Down> gj
nnoremap <C-K> gk
nnoremap <C-J> gj
nnoremap <C-A> ggVG$
legacy nnoremap <expr> <C-L> &relativenumber == 1 ? ':set number:set norelativenumber' : ':set relativenumber:set number'
nnoremap <C-_> :call AdjustFontSize(-1)
nnoremap <C-+> :call AdjustFontSize(1)
nnoremap <C-F> :NERDTreeFind
nnoremap <C-T> :NERDTreeToggle
nnoremap <C-N> :NERDTree
inoremap  ggVG$
legacy inoremap <expr>  &relativenumber == 1 ? ':set number:set norelativenumberi' : ':set relativenumber:set numberi'
inoremap  :mkses!:wqa
inoremap  :wi
imap <silent> m <Plug>(emmet-merge-lines)
imap <silent> A <Plug>(emmet-anchorize-summary)
imap <silent> a <Plug>(emmet-anchorize-url)
imap <silent> k <Plug>(emmet-remove-tag)
imap <silent> j <Plug>(emmet-split-join-tag)
imap <silent> / <Plug>(emmet-toggle-comment)
imap <silent> I <Plug>(emmet-image-encode)
imap <silent> i <Plug>(emmet-image-size)
imap <silent> N <Plug>(emmet-move-prev)
imap <silent> n <Plug>(emmet-move-next)
imap <silent> D <Plug>(emmet-balance-tag-outword)
imap <silent> d <Plug>(emmet-balance-tag-inward)
imap <silent> u <Plug>(emmet-update-tag)
imap <silent> ; <Plug>(emmet-expand-word)
imap <silent> , <Plug>(emmet-expand-abbr)
inoremap ( ()O
inoremap << <<
inoremap <  < 
inoremap < <><Left>
nnoremap  :qa!
inoremap [ []O
inoremap { {}O
&cpo = cpo_save
set autoindent
set autoread
set background=dark
set backupdir=~/.cache/vim/backup//
set directory=~/.cache/vim/swap//
set fileencodings=ucs-bom,utf-8,default,latin1
set helplang=en
set mouse=a
set runtimepath=~/.vim,~/.vim/bundle/Vundle.vim,~/.vim/bundle/jslint.vim,~/.vim/bundle/fzf.vim,~/.vim/bundle/fzf,~/.vim/bundle/emmet-vim,~/.vim/bundle/xmledit,~/.vim/bundle/vimtex,~/.vim/bundle/vim-javascript,~/.vim/bundle/auto-pairs,/usr/share/vim/vimfiles,/usr/share/vim/vim92,/usr/share/vim/vim92/pack/dist/opt/netrw,/usr/share/vim/vimfiles/after,~/.vim/after,~/.vim/bundle/Vundle.vim,~/.vim/bundle/Vundle.vim/after,~/.vim/bundle/jslint.vim/after,~/.vim/bundle/fzf.vim/after,~/.vim/bundle/fzf/after,~/.vim/bundle/emmet-vim/after,~/.vim/bundle/xmledit/after,~/.vim/bundle/vimtex/after,~/.vim/bundle/vim-javascript/after,~/.vim/bundle/auto-pairs/after
set shiftwidth=4
set suffixes=.bak,~,.o,.info,.swp,.aux,.bbl,.blg,.brf,.cb,.dvi,.idx,.ilg,.ind,.inx,.jpg,.log,.out,.png,.toc
set tabstop=4
set termsync
set undodir=~/.cache/vim/undo//
import autoload '/usr/share/vim/vim92/autoload/dist/vim9.vim'
const so_save: number = &g:so | const siso_save: number = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
v:this_session = expand("<sfile>:p")
doautoall SessionLoadPre
var save_splitbelow: bool
var save_splitright: bool
var save_winminheight: number
var save_winminwidth: number
var wipebuf: number = -1
var shortmess_save: string
silent only
silent tabonly
cd ~/Git/alkyl
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  wipebuf = bufnr('%')
endif
set shortmess+=aoO
argglobal
:%argdel
argglobal
enew
cpo_save = &cpo
set cpo&vim
inoremap <buffer> <silent> <M-n> :call AutoPairsJump()a
legacy inoremap <buffer> <silent> <expr> <M-p> AutoPairsToggle()
inoremap <buffer> <silent> <M-b> =AutoPairsBackInsert()
inoremap <buffer> <silent> <M-e> =AutoPairsFastWrap()
inoremap <buffer> <silent> <C-H> =AutoPairsDelete()
inoremap <buffer> <silent> <BS> =AutoPairsDelete()
inoremap <buffer> <silent> <M-'> =AutoPairsMoveCharacter('''')
inoremap <buffer> <silent> <M-"> =AutoPairsMoveCharacter('"')
inoremap <buffer> <silent> <M-}> =AutoPairsMoveCharacter('}')
inoremap <buffer> <silent> <M-{> =AutoPairsMoveCharacter('{')
inoremap <buffer> <silent> <M-]> =AutoPairsMoveCharacter(']')
inoremap <buffer> <silent> <M-[> =AutoPairsMoveCharacter('[')
inoremap <buffer> <silent> <M-)> =AutoPairsMoveCharacter(')')
inoremap <buffer> <silent> <M-(> =AutoPairsMoveCharacter('(')
inoremap <buffer> <silent> § =AutoPairsMoveCharacter('''')
inoremap <buffer> <silent> ¢ =AutoPairsMoveCharacter('"')
inoremap <buffer> <silent> © =AutoPairsMoveCharacter(')')
inoremap <buffer> <silent> ¨ =AutoPairsMoveCharacter('(')
inoremap <buffer> <silent> î :call AutoPairsJump()a
legacy inoremap <buffer> <silent> <expr> ð AutoPairsToggle()
inoremap <buffer> <silent> â =AutoPairsBackInsert()
inoremap <buffer> <silent> å =AutoPairsFastWrap()
inoremap <buffer> <silent> ý =AutoPairsMoveCharacter('}')
inoremap <buffer> <silent> û =AutoPairsMoveCharacter('{')
inoremap <buffer> <silent> Ý =AutoPairsMoveCharacter(']')
inoremap <buffer> <silent> Û =AutoPairsMoveCharacter('[')
noremap <buffer> <silent> <M-n> :call AutoPairsJump()
noremap <buffer> <silent> <M-p> :call AutoPairsToggle()
inoremap <buffer> <silent>  =AutoPairsDelete()
inoremap <buffer> <silent>   =AutoPairsSpace()
inoremap <buffer> <silent> " =AutoPairsInsert('"')
inoremap <buffer> <silent> ' =AutoPairsInsert('''')
inoremap <buffer> <silent> ( =AutoPairsInsert('(')
inoremap <buffer> <silent> ) =AutoPairsInsert(')')
noremap <buffer> <silent> î :call AutoPairsJump()
noremap <buffer> <silent> ð :call AutoPairsToggle()
inoremap <buffer> <silent> [ =AutoPairsInsert('[')
inoremap <buffer> <silent> ] =AutoPairsInsert(']')
inoremap <buffer> <silent> ` =AutoPairsInsert('`')
inoremap <buffer> <silent> { =AutoPairsInsert('{')
inoremap <buffer> <silent> } =AutoPairsInsert('}')
&cpo = cpo_save
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal backupcopy=
setlocal balloonexpr=
setlocal nobinary
setlocal nobreakindent
setlocal breakindentopt=
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal nocindent
setlocal cinkeys=0{,0},0),0],:,0#,!^F,o,O,e
setlocal cinoptions=
setlocal cinscopedecls=public,protected,private
setlocal cinwords=if,else,while,do,for,switch
setlocal colorcolumn=
setlocal comments=s1:/*,mb:*,ex:*/,://,b:#,:%,:XCOMM,n:>,fb:-
setlocal commentstring=/*\ %s\ */
setlocal complete=.,w,b,u,t,i
setlocal completefunc=
setlocal completeopt=
setlocal concealcursor=
setlocal conceallevel=0
setlocal nocopyindent
setlocal cryptmethod=
setlocal nocursorbind
setlocal nocursorcolumn
setlocal nocursorline
setlocal cursorlineopt=both
setlocal define=
setlocal dictionary=
setlocal nodiff
setlocal diffanchors=
setlocal equalprg=
setlocal errorformat=
setlocal eventignorewin=
setlocal noexpandtab
if &filetype != ''
setlocal filetype=
endif
setlocal fillchars=
setlocal findfunc=
setlocal fixendofline
setlocal foldcolumn=0
setlocal foldenable
setlocal foldexpr=0
setlocal foldignore=#
setlocal foldlevel=0
setlocal foldmarker={{{,}}}
setlocal foldmethod=manual
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal formatoptions=tcq
setlocal formatprg=
setlocal grepformat=
setlocal grepprg=
setlocal iminsert=0
setlocal imsearch=-1
setlocal include=
setlocal includeexpr=
setlocal indentexpr=
setlocal indentkeys=0{,0},0),0],:,0#,!^F,o,O,e
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
setlocal keywordprg=
setlocal lhistory=10
setlocal nolinebreak
setlocal nolisp
setlocal lispoptions=
setlocal lispwords=
setlocal nolist
setlocal listchars=
setlocal makeencoding=
setlocal makeprg=
setlocal matchpairs=(:),{:},[:]
setlocal modeline
setlocal modifiable
setlocal nrformats=bin,octal,hex
set number
setlocal number
setlocal numberwidth=4
setlocal omnifunc=
setlocal path=
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
setlocal norelativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal noscrollbind
setlocal scrolloff=-1
setlocal scrolloffpad=-1
setlocal shiftwidth=4
setlocal noshortname
setlocal showbreak=
setlocal sidescrolloff=-1
setlocal signcolumn=auto
setlocal nosmartindent
setlocal nosmoothscroll
setlocal softtabstop=0
setlocal nospell
setlocal spellcapcheck=[.?!]\\_[\\])'\"\	\ ]\\+
setlocal spellfile=
setlocal spelllang=en
setlocal spelloptions=
setlocal statusline=
setlocal statuslineopt=
setlocal suffixesadd=
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != ''
setlocal syntax=
endif
setlocal tabstop=4
setlocal tagcase=
setlocal tagfunc=
setlocal tags=
setlocal termwinkey=
setlocal termwinscroll=10000
setlocal termwinsize=
setlocal textwidth=0
setlocal thesaurus=
setlocal thesaurusfunc=
setlocal noundofile
setlocal undolevels=-123456
setlocal varsofttabstop=
setlocal vartabstop=
setlocal virtualedit=
setlocal wincolor=
setlocal nowinfixbuf
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal winhighlight=
setlocal wrap
setlocal wrapmargin=0
tabnext 1
if wipebuf != -1 && len(win_findbuf(wipebuf)) == 0
  silent exe 'bwipe ' .. wipebuf
endif
set winheight=1 winwidth=20
set shortmess=filnxtToOS
var sx: string = expand("<sfile>:p:r") .. "x.vim"
if filereadable(sx)
  exe "source " .. fnameescape(sx)
endif
&g:so = so_save | &g:siso = siso_save
nohlsearch
doautoall SessionLoadPost
unlet g:SessionLoad
# vim: set ft=vim :
