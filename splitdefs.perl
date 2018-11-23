eval 'exec perl -S $0 ${1+"$@"}'
     if $running_under_some_shell;

$DIFF="/usr/bin/diff";
$MKDIR="/bin/mkdir";
$CPP="/lib/cpp";
$SED="/usr/bin/sed";
$INPUT=$ARGV[0];
$PREFIX="_DEFS_";
$usage="Usage: splitdefs <.h file> [-Ddefn]... [-Iincludes]... [-pprefix] \n";

if ($#ARGV == -1) {
  print $usage;
  exit 1;
}

if (! -r $INPUT) {
  print STDERR "Can't read file: \"$INPUT\"\n";
  exit 1;
}

$opts = "";

for ($i = 1; $i <= $#ARGV; $i++) {
    if ($ARGV[$i] =~ /^-p/) {
      $PREFIX=substr($ARGV[$i],2);
      next;
    }
    if ($ARGV[$i] =~ /^-D/ || $ARGV[$i] =~ /^-I/) {
      $opts=$opts." ".$ARGV[$i];
    } else {
      print $usage;
      exit 1;
    }
}

$dir = $INPUT . ".d/";

if (! -d $dir) {
    system("$MKDIR $dir");
}

$sedpattern = '-e \'s/^# *define/sdXXX#define/\' \
	       -e \'s/^# *undef/sdXXX#undef/\' \
	       -e \'s%^/\* # *\(undef .*\) \*/[ \t\r\n]*%sdXXX#\1%\'';

$sedpattern2 = '-e \'s/^sdXXX#/#/\'';

$cmd = "$SED $sedpattern $INPUT | $CPP -P $opts | $SED $sedpattern2";
# print "$cmd\n";

open(DEFNS, "$cmd |");

while (<DEFNS>) {
  # print $_;
  if ($_ =~ "^#define" || $_ =~ "^#undef") {
    ($cmd, $var, $val) = split(/[ \t\r\n]+/, $_, 3);
    if ($var ne "") {
	$fname = $dir . $var . ".h";
	if (!open(INCFILE,">" . $fname . "-new")) {
	    print STDERR ("Could not open " . $fname . "-new\n");
	    exit(-1);
	}
	print INCFILE ("#ifndef " . $PREFIX . $var . "_H\n");
	print INCFILE ("#define " . $PREFIX . $var . "_H\n");
	print INCFILE ("\n");
	if ($cmd ne "#undef") {
	    print INCFILE ("#define " . $var . " " . $val . "\n");
	} else {
	    print INCFILE ("#undef " . $var . " " . $val . "\n")
	}
	print INCFILE ("\n#endif /* " . $PREFIX . $var . "_H */\n");
	close(INCFILE);
	if ($recorded{$fname} != 1) {
	    push(@defined, $fname);
	    $recorded{$fname} = 1;
	}
    }
  }
}
close(DEFNS);

foreach $fname (@defined) {
  if ((-r $fname) &&
    (0 == system("$DIFF " . $fname . " " . $fname . "-new")/256)) {
    unlink($fname . "-new");
  } else {
    if (!rename($fname . "-new", $fname)) {
      print STDERR "Rename failed\n";
      exit 1;
    }
  }
}
