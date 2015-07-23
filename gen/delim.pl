#!/usr/bin/env perl

use strict;
use warnings;

my $outfile = "delim.txt";
open my $out, ">$outfile" or
    die "Cannot open $outfile for writing: $!\n";
my @alphabet = qw( a b c );
my @chars;
for (my $i = 0; $i < 1024 * 1024; $i++) {
    push @chars, $alphabet[int(rand $i) % 3];
}
print $out "d" . join("", @chars) x 10 . "aaabbccb";
close $out;

