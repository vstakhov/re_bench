set terminal pngcairo background "#ffffff" fontscale 1.0 size 640, 500

set boxwidth 1
set grid
set datafile separator ","
set output "a.png"
set style fill solid 1.0 border -1
set xtics border in scale 0,10  nomirror rotate by -30
set ylabel "Matching Speed (Mega Bytes/Sec)" font "bold"
set title "`head -n1 a.csv`"
plot "a.csv" every ::1 using ($0):1:($0):xticlabels(2) with boxes lc variable notitle

