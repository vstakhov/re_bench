#!/usr/bin/env perl

use strict;
use warnings;

my $found;
my $total_size;
while (<>) {
    if (m{^\./bench '(.*?)' (\S+)}) {
        if ($found) {
            last;
        }
        my ($re, $file) = ($1, $2);
        #warn $re;
        my @info = stat $file;
        $total_size = $info[7];
        my $msize = sprintf("%.01f MB", $total_size / 1024 / 1024);
        $re =~ s{\\}{\\\\}g;
        print "Benchmarking regex /$re/ matching file $file of size $msize\n";
        $found = 1;
    } elsif ($found && /^(\w+ .*?)\s*(?:match|error|no match|:)/) {
        my $name = $1;
        $name =~ s/^\s+|\s+$//sg;
        if (/((?:\d+)(?:\.\d+)?) ms elapsed/) {
            my $time = $1;
            my $speed = $total_size/$time*1000/1024/1024;
            #warn "$name: $time";
            print "$speed,$name\n";
        }
    }
}
