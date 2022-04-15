fld2=$1
n=$2
p=$3
for ((i=1 ; i<=$n; i++))
do
	diff -Bw --strip-trailing-cr ./output/op-c$i-p$p.txt ./$fld2/ans_$i\_$p.txt > ./diffs/a_$i\_$p.txt
	echo a_$i\_$p
	cat ./diffs/a_$i\_$p.txt
done