#!/bin/sh

#set -x


refresh()
{
echo "long wait ..."
cat > /tmp/cnt <<END 
:T: Man pages with hyperlinks -- Demo 
:B:                         -- press enter or [q]uit -- 
END
cd /usr/man/man1

list="`/bin/ls a*.gz b*gz c*gz 2>/dev/null`"
pre="gzip -dc"

if [ -z "$list" ] ; then
	list="`/bin/ls a* b* c*`"
	pre=cat
fi


for m in $list 
do
desc=`$pre /usr/man/man1/$m| awk '\
BEGIN {found=0} ; \
/\.SH NAME/ { found=1}; \
$0 !~ /.*NAME.*/ { if (found==1) {print $0; found=0}; } \
'`

echo -e "$m $desc" 

done | awk '{printf("%-30s",$1); print $4,$5,$6,$7,$8,$9 }' >> /tmp/cnt

kill -10 $pid 
exit
}

pid=$2
set -- $1

if [ -z "$1" ] ; then

refresh
exit
fi


case $1 in
)
	f=$3
	set -- `echo $f| tr '.' ' '`
	page=$1 
	man $page 
	;;	
	
esac


