#!/usr/bin/perl

use Statistics::R;

foreach my $file (@ARGV)
{
print "Processing file $file\n";

my $R = Statistics::R->new();
$R->startR ;

#$R->send(qq`pdf("$file.pdf")`);
$R->send(qq`png("$file.png")`);

my $read = 0;
my $wait = 0;
my $work = 0;
my $pop = 0;
my $push = 0;
my $split = 0;
my $xkaapi = 0;

open (FILE, $file);

while (<FILE>) {
  chomp;
  if (/sched idle time\s+:\s([+-]?\ *(?:\d+(?:\.\d*)?|\.\d+)(?:[eE]([+-]?\d+))?)/)
  {
    $xkaapi = $1;# * (10*$2);
  }

  if (/read :(\d+)/)
  {
    $read = $1 / 1000000;
  }
  if (/wait :(\d+)/)
  {
    $wait = $1 / 1000000;
  }
  if (/work : (\d+)/)
  {
    $work = $1 / 1000000;
  }
  if (/Total pop : (\d+)/)
  {
    $pop = $1 / 1000000;
  }
  if (/Total push : (\d+)/)
  {
    $push = $1 / 1000000;
  }
  if (/Total splitter : (\d+)/)
  {
    $split = $1 / 1000000;
  }

}

$R->send(qq`slices <- c($work,$push+$pop,$split,$xkaapi)`);
$R->send(q`lbls <- c("Dec+Res","Push + Pop", "Splitter", "XKaapi")`);
$R->send(q`pct <- round(slices/sum(slices)*100)`);
$R->send(q`lbls <- paste(lbls, pct)`);
$R->send(q`lbls <- paste(lbls,"%",sep="")`);
$R->send(qq`pie(slices,labels = lbls, col=rainbow(length(lbls)), main="$file")`);
$R->error();

my $ret = $R->read;
$R->stopR();

close (FILE);
}
exit;
