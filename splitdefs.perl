#!/usr/bin/env perl

eval 'exec perl -S $0 ${1+"$@"}'
     if $running_under_some_shell;

use strict;
use warnings;

my $CMP="/usr/bin/cmp -s";
my $MKDIR="/bin/mkdir";
my $CPP="/usr/bin/cpp";
my $SED="/usr/bin/sed";
my $PREFIX="_DEFS_";
my $usage="Usage: splitdefs [-v] [-Ddefn]... [-Iincludes]... [-p prefix] [-d dir] <.h file> ...\n";

if ($#ARGV == -1) {
  print $usage;
  exit 1;
}

my $opts = "";
my $dir;
my $arg;
my $verbose;

while (1) {
    $arg = shift;
    if (!$arg) {
        print $usage;
        exit 1;
    }
    if (!($arg =~ /^-/)) { last }
    if ($arg eq '-v') {
        $verbose = 1;
    } elsif ($arg =~ /^-p/) {
      if (length $arg > 2) {
        $PREFIX = substr($arg, 2);
      } else {
        $PREFIX = shift;
      }
    } elsif ($arg =~ /^-D/ || $arg =~ /^-I/) {
      if (length $arg > 2) {
        $opts="$opts $arg"
      } else {
        $opts="$opts $arg ".shift;
      }
    } elsif ($arg =~ /^-d/) {
      if (length $arg > 2) {
        $dir = substr($arg, 2);
      } else {
        $dir = shift;
      }
      if (!($dir =~ m|/$|)) { $dir .= '/' }
    } else {
      print $usage;
      exit 1;
    }
}

my $input_file;

if ($arg && !$dir) {
    $dir = $arg . ".d/";
}

if (! -d $dir) {
    system("$MKDIR $dir") || exit 1;
}

my $cpp_input;

while ($arg) {
    $input_file = $arg;
    $arg = shift;
    if (! -r $input_file) {
        print STDERR "Can't read file: \"$input_file\"\n";
        exit 1;
    }
    open(INPUT, $input_file);
    my $input;
    {
        local $/;
        $input = <INPUT>;
    }

    $input =~ s/(\A|\n)#\s*define/$1\nsdXXX#define/g;
    $input =~ s/(\A|\n)#\s*undef/$1sdXXX#undef/g;
    $input =~ s%(\A|\n)/\* # *\(undef .*\) \*/[ \t\r\n]*%$1sdXXX#$2%g;

    $cpp_input .= $input;
}

my $tmpfile = "$dir/splitdefs.tmp.$$";

open (CPP_INPUT, ">$tmpfile");
print CPP_INPUT $cpp_input;
close(CPP_INPUT);

open(DEFNS, "$CPP < $tmpfile |");

my $fname;
my %recorded;
my %defn;

while (<DEFNS>) {
  s/^sdXXX#/#/;
  #print $_;

  if ($_ =~ "^#define" || $_ =~ "^#undef") {
    (my $cmd, my $var, my $val) = split(/[ \t\r\n]+/, $_, 3);
    $defn{$var} = [$cmd, $val] if $var;
  }
}
close(DEFNS);

unlink($tmpfile);

sub generate_file {
    my ($cmd, $var, $val) = @_;
    my $base = $var;
    $base =~ s/\(.*\)$//;
    $fname = $dir . $base . ".h";
    if (!open(INCFILE,">" . $fname . "-new")) {
        print STDERR ("Could not open " . $fname . "-new\n");
        exit(-1);
    }
    print INCFILE ("#ifndef " . $PREFIX . $base . "_H\n");
    print INCFILE ("#define " . $PREFIX . $base . "_H\n");
    print INCFILE ("\n");
    if ($cmd ne "#undef") {
        print INCFILE ("#define " . $var . " " . $val . "\n");
    } else {
        print INCFILE ("#undef " . $var . " " . $val . "\n")
    }
    print INCFILE ("\n#endif /* " . $PREFIX . $base . "_H */\n");
    close(INCFILE);
    $recorded{$fname} = 1;
}

foreach my $var (keys %defn) {
    (my $cmd, my $val) = @{$defn{$var}};
    generate_file($cmd, $var, $val);
}

my $changes = 0;

foreach $fname (keys %recorded) {
  if ((-r $fname) &&
    (0 == system("$CMP " . $fname . " " . $fname . "-new")/256)) {
    unlink($fname . "-new");
  } else {
    if (!rename($fname . "-new", $fname)) {
      print STDERR "Rename failed\n";
      exit 1;
    }
    if ($verbose) {
        print "  ", $fname, "\n";
    }
    $changes++;
  }
}

print "$changes files changed.\n";
