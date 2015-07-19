#!/usr/bin/env perl

use strict;
use warnings;

my $outfile = "abc.txt";
open my $out, ">$outfile" or
    die "Cannot open $outfile for writing: $!\n";
print $out "abccc" x (5 * 1024 * 1024) . "aaabbccb";
close $out;

