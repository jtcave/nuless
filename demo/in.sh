#!/bin/sh

#set -x


refresh()
{

cat >/tmp/mem <<END
OS=$OS
VER=$VER
KER=$KER
END
cat > /tmp/cnt <<END 
:T: Input field Demo 
:B: [i] to select [q]uit 
END

#cat /tmp/mem| awk ' BEGIN {FS="="} { printf("%s\t%s\n",$1,$2)}' >>/tmp/cnt

cat /tmp/mem | sed 's/=\(.*\)$/	\1/g' >> /tmp/cnt



kill -10 $pid 
exit
}

pid=$2
set -- $1


if [ -z "$1" ] ; then

OS="------"
VER="------"
KER="------"

refresh
fi


. /tmp/mem

case $1 in
i)

	case $2 in
	1) 
	read -p "inserisci OS: " OS 
	;;
	2)
	read -p "inserisci VER: " VER 
	;;
	3)
	read -p "inserisci KERNEL: " KER
	;;
	esac
	refresh
	;;	
	
esac


