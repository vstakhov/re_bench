#!/usr/bin/env perl

use strict;
use warnings;

use Getopt::Std;

my %opts;
getopts("o:", \%opts) or die;

my $outfile = $opts{o} || "a.png";
if ($outfile !~ /\.png$/i) {
    die "Only PNG output is allowed.\n";
}

my $infile = shift
    or die "No input file specified.\n";
open my $in, $infile
    or die "Cannot open $infile for reading: $!\n";

my $found;
my $total_size;
my $title;
my @data;
my $max_y = 0;
while (<$in>) {
    if (m{^\./bench\d*\s+(.*?)\s+(\S+)$}) {
        if ($found) {
            last;
        }
        my ($re, $file) = ($1, $2);
        #warn $re;
        my @info = stat $file;
        $total_size = $info[7];
        if (!defined $total_size) {
            die "failed to stat $file.\n";
        }
        my $msize = sprintf("%.01f MB", $total_size / 1024 / 1024);
        $re =~ s{\\}{\\\\}g;
        $re =~ s{"}{\\"}g;
        $title = "Benchmarking regex /$re/ matching file $file of size $msize";
        #print $out "$title\n";
        $found = 1;
    } elsif ($found && /^(\w+ .*?)\s*(?:match|error|no match)/) {
        my $name = $1;
        $name =~ s/^\s+|\s+$//sg;
        if (/((?:\d+)(?:\.\d+)?) ms elapsed/) {
            my $time = $1;
            my $speed = sprintf "%.01lf", $total_size/$time*1000/1024/1024;
            #warn "$name: $time";
            #print $out "$speed,$name\n";
            if ($speed > $max_y) {
                $max_y = $speed;
            }
            my $label;
            if (/\berror:/) {
                $label = "error";
            } else {
                $label = $speed;
            }
            push @data, "$name,$speed,$label";
        }
    }
}
close $in;

$title =~ s/(.{70}.*?(?![\.\w]))/$1\\n/g;

if (!@data) {
    die "No benchmark data found!";
}

if (!$title) {
    die "No ./bench command found!";
}

$max_y *= 1.1;

my $label_delta = $max_y / 25;
#warn $label_delta;

my $gnufile = "a.gnu";
open my $out, ">$gnufile"
    or die "Cannot open $gnufile for writing: $!\n";

print $out <<_EOC_;
set terminal pngcairo background "#ffffff" fontscale 1.0 size 800, 500 enhanced font 'andale mono,10'

set boxwidth 1
set grid
set datafile separator ","
set output "$outfile"
set yrange [0:$max_y]
set style fill solid 1.0 border -1
set xtics border in scale 0,10  nomirror rotate by -45
set ylabel "Matching Speed (Mega Bytes/Sec)" font "bold"
set title "$title" noenhanced
plot "a.csv" using (\$0):2:(\$0):xticlabels(1) with boxes lc variable notitle, \\
     "" using (\$0):(\$2+$label_delta):3 with labels notitle
_EOC_

close $out;

my $csvfile = "a.csv";
open $out, ">$csvfile"
    or die "Cannot open $csvfile for writing: $!\n";
for my $row (@data) {
    print $out "$row\n";
}
close $out;

my $cmd = "gnuplot $gnufile";
print "$cmd\n";
system($cmd) == 0 or die;
print "$outfile generated.\n";
