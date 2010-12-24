#!/usr/bin/env perl

use strict;
use warnings;

use lib $ENV{HOME} . "/bin/";

use File::Find;
use XML::Simple;
use Component;

# Usage :
if (!defined ($ARGV[0])) {
  print "Usage :\tfractal.pl file.fractal [file.dot | file.png]\n\tAny existing dot or png file specified will be overwritten.\n";
  exit;
}

# Use the first arg as main Fractal file for the application :
my $fractal = $ARGV[0];
my ($comp, $foo) = split /\./, $fractal;

# Init some usefull variables :
my $pngfile = "";
my $extension = "";
my $dottmpfile = "";

# Parse this file :
# faire le push dans le new en fait 
new Component(XMLin($fractal, ForceArray => 1, KeyAttr => ['client','name']));

#  Decide wether to print output to console or file if $ARGV[1] exists :
if (defined ($ARGV[1])) {
  my ($file, $extension) = split /\./, $ARGV[1];
  # Decide wether it's a dot file or a png file :
  if ($extension eq "dot") {# dot file
    open (DOTFILE, "> $ARGV[1]");
    *OUTPUT = *DOTFILE;
  } else {
    # do something
    $pngfile = $file.".png";
    $dottmpfile = $file.".dot";
    open (DOTTMPFILE, "> $dottmpfile");
    *OUTPUT = *DOTTMPFILE;
  }
} else {
  *OUTPUT = *STDOUT;
}

our $filehandle = *OUTPUT;

# Dump graph to output :
$comp =~s/\//\./;
print OUTPUT "digraph g {\n";
foreach(keys %Component::components) {
	if ($Component::components{$_}->{main} eq 1)
	{
		$Component::components{$_}->display(1);
		last;
	}
}
print OUTPUT "}\n";

# Close file if $ARGV[1] was provided :
if (defined ($ARGV[1]))
{
  if ($extension eq "dot") {
    close (DOTFILE);
  } else {
    close (DOTTMPFILE);
  }
}

# system usage :
#   1.  @args = ("command", "arg1", "arg2");
#   2. system(@args) == 0
#   3. or die "system @args failed: $?"


if ( defined $pngfile and $pngfile ne '') {
    my @args = (dot => '-Tpng', $dottmpfile, "-o$pngfile");
    if ( system @args ) {
        warn "'system @args' failed\n";
        my $reason = $?;
        if ( $reason == -1 ) {
            die "Failed to execute: $!";
        }
        elsif ( $reason & 0x7f ) {
            die sprintf(
                'child died with signal %d, %s coredump',
                ($reason & 0x7f),  
                ($reason & 0x80) ? 'with' : 'without'
            );
        }
        else {
            die sprintf('child exited with value %d', $reason >> 8);
        }
    }
#    warn "'system @args' executed successfully\n";
    unlink $dottmpfile;
}
