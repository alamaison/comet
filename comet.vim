" Comet wizard functions
" Author: Michael Geddes <michaelrgeddes@optushome.com.au>
" Version: 1.1
"

if !exists('g:TLB2H')
  let g:TLB2H='tlb2h'

  if !executable(g:TLB2H)
    let g:TLB2H=expand('<sfile>:h').'\bin\tlb2h.exe'
    if !executable(g:TLB2H)
      let g:TLB2H='c:\lambdasoft\comet\bin\tlb2h' 
      if !executable(g:TLB2H)
        let g:TLB2H='t:\comet\bin\tlb2h' 
      endif
    endif
  endif
endif


fun! s:CometHeaderWizard()
  let curdir=expand('%:p:h')
  while 1
    let dir=(( curdir =~ '[/\\]$')?(curdir):(curdir.'/'))
    let files=glob(dir.'*.tlb')
    if files != '' | break |echo "Yes"| endif
    let nextdir=fnamemodify(curdir,':h')
    if(curdir == nextdir) | break | endif
    let curdir=nextdir
  endwhile
  if files == '' 
    echo "no Files" 
    return 0
  endif

  if files =~ "\n"
    let idx=confirm("Type Library",substitute(files,"[ 	\r\n]*$",'',''))
    if idx==0 | return | endif
    let files= matchstr( files."\n", "^\\(\\zs[^\n]*\\ze\n\\)\\{".(idx)."}")
  endif
  exe '.r! '.g:TLB2H.' -l '.files
endfun

fun! s:CometSourceWizard(coclass, iface)
  let curdir=expand('%:p:h')

  if a:iface
    let srch_opt=' -Wi '
    let title='Interface'
    let wizard_opt_some='-wi- '
    let wizard_opt_all=''
    let has_all=0
    let all_title=''
  else
    let srch_opt=' -Wc '
    let title='Coclass'
    let wizard_opt_some='-w- '
    let wizard_opt_all='-W- '
    let has_all=1
    let all_title="&All\n"
  endif

  while 1
    let dir=(( curdir =~ '[/\\]$')?(curdir):(curdir.'/'))
    let files=glob(dir.'*.tlb')
    if files != '' | break |echo "Yes"| endif
    let nextdir=fnamemodify(curdir,':h')
    if(curdir == nextdir) | break | endif
    let curdir=nextdir
  endwhile
  if files == '' | echohl Error | echo "No type libraries" | echohl None | return | endif
  if a:coclass == '*'
    let choice=system(g:TLB2H.srch_opt.dir.'*.tlb')
    let idx=confirm(title,all_title.substitute(choice,"[ 	\r\n]*$",'',''))
    if idx==0 | return | endif
    if has_all && idx==1
      let which=''
    else
      let which= matchstr( choice."\n", "^\\(\\zs[^\n]*\\ze\n\\)\\{".(idx-has_all)."}")
    endif
  else
    let which= a:coclass
  endif
  
  let opt=((which=='')?(wizard_opt_all):(wizard_opt_some.which))
  let here=line('.')
  exe '.r!'.g:TLB2H.' '.opt.' '.dir.'*.tlb'
  exe here
endfun
map <buffer> <localleader>cc :call <SID>CometSourceWizard('*',0)<CR>
map <buffer> <localleader>ci :call <SID>CometSourceWizard('*',1)<CR>
map <buffer> <localleader>ch :call <SID>CometHeaderWizard()<CR>

if exists(':Bmenu')
  exe FindBufferSID()
  if BufferOneShot('cometcommands')
    Bamenu 10.10 &Code.Comet\ &Wizard :call <SID>CometSourceWizard('*',0)<CR>
    Bamenu 10.11 &Code.Comet\ &Interface\ Wizard :call <SID>CometSourceWizard('*',1)<CR>
    Bamenu 10.12 &Code.Comet\ Wi&zard\ Coclasses :call <SID>CometSourceWizard(inputdialog('Class Name:'),0)<CR>
    Bamenu 10.13 &Code.Comet\ Wiza&rd\ Interface :call <SID>CometSourceWizard(inputdialog('Interface Name:'),1)<CR>
    Bamenu 10.14 &Code.Comet\ Header :call 
  endif
endif

