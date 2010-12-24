divert(-1)

#debugmode(e)
#traceon

changecom(`# ')
define(`_n',0)

# internals

     define(`forloop',
            `pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')
     define(`_forloop',
            `$4`'ifelse($1, `$3', ,
                   `define(`$1', incr($1))_forloop(`$1', `$2', `$3', `$4')')')

# REPEAT(char,num)

define(`_REPEAT_',dnl
`forloop(`i',1,$2,`$1')'dnl
)



# _URL_[url,txt]

define(`_URL_',dnl
`define(`_n', incr(_n))'dnl
`syscmd(echo URL [_n] $1>>url.list)'dnl
`[_n] <$2>'dnl
)dnl


# _H1_[s]

define(`_H1_',dnl
`_REPEAT_(` ',eval( (70-len($*))/2 ))'$1dnl
)dnl

# _H2_[s]

define(`_H2_',dnl
$1
)dnl


# _TITLE_(s)

define(`_TITLE_',dnl
`_REPEAT_(` ',eval( 70-len($*)))' $1dnl
)dnl


# _IMG_(s)

define(`_IMG_',`$2')dnl


divert
