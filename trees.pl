#!/usr/bin/env perl
use strict;
use warnings;
use Data::Dumper;

my $depth = $ARGV[0];
my $work_min = $ARGV[1];
my $work_max = $ARGV[2];
my $degree_min = $ARGV[3];
my $degree_max = $ARGV[4];
my $nb_trees = $ARGV[5];
my $file_prefix = $ARGV[6];

die "use: tree.pl depth work_min work_max degree_min degree_max nb_trees file_prefix" unless defined $degree_max;

if (!defined $nb_trees) {$nb_trees = 1;}
if (!defined $file_prefix) {$file_prefix = "test";}

my $current_node;
my $last_node;
my $current_depth = -1;

my %father;
my %depth;
my %weight;
my %children;

my $tree = 0;

while($current_depth < $depth) {
  %father = ();
  %depth = ();
  %weight = ();
  %children = ();
  $last_node = 0;
  $depth{0} = 0;
  $weight{0} = int rand(1+$work_max - $work_min) + $work_min;

  my $only_father = 0;
  my @left_nodes = (0);
  while(@left_nodes) {
    $current_node = shift @left_nodes;
    if ($current_depth != $depth{$current_node}) {
      $current_depth = $depth{$current_node};
      $only_father = $current_node;
      $children{$only_father} = [];
    } else {
      
    }

    last if $current_depth == $depth;
    my $children = int rand(1+$degree_max - $degree_min) + $degree_min;
    my @children;
    for(1..$children) {
      $last_node++;
      $father{$last_node} = $only_father;
      $weight{$last_node} = int rand(1+$work_max - $work_min) + $work_min;
      $depth{$last_node} = $current_depth + 1;
      push @left_nodes, $last_node;
      push @children, $last_node;
    }
    foreach my $child (@children) {
      push @{$children{$only_father}}, $child;
    }
  }
}

save_graph($file_prefix . "_$tree.dot", $last_node + 1, \%children, \%weight);

#open(DOTFILE, ">$file_prefix" . "_$tree.dot") or die $!;
#
#print DOTFILE "graph {\n";
#
#for(0..$last_node) {
#  #print join(' ', @{$children{$_}}) if defined $children{$_};
#  foreach my $child (@{$children{$_}}) {
#    print DOTFILE " $_ -- $child; /* $weight{$child} */\n";
#  }
#}
#
#print DOTFILE "}\n";

# General case :
# TODO : use previous weight.

for my $tree (1..$nb_trees) {
  $current_depth = -1;
  while($current_depth < $depth) {
    %father = ();
    %depth = ();
    %weight = ();
    %children = ();
    $last_node = 0;
    $depth{0} = 0;
    $weight{0} = int rand(1+$work_max - $work_min) + $work_min;

    my @left_nodes = (0);
    while(@left_nodes) {
      $current_node = shift @left_nodes;
      $current_depth = $depth{$current_node};
      last if $current_depth == $depth;
      my $children = int rand(1+$degree_max - $degree_min) + $degree_min;
      my @children;
      for(1..$children) {
        $last_node++;
        $father{$last_node} = $current_node;
        $weight{$last_node} = int rand(1+$work_max - $work_min) + $work_min;
        $depth{$last_node} = $current_depth + 1;
        push @left_nodes, $last_node;
        push @children, $last_node;
      }
      $children{$current_node} = [ @children ];
    }
  }

  save_graph($file_prefix . "_$tree.dot", $last_node + 1, \%children, \%weight);

#  open(DOTFILE, ">$file_prefix" . "_$tree.dot") or die $!;
#  
#  print DOTFILE "graph {\n";
#
#  for(0..$last_node) {
#    #print join(' ', @{$children{$_}}) if defined $children{$_};
#    foreach my $child (@{$children{$_}}) {
#      print DOTFILE " $_ -- $child; /* $weight{$child} */\n";
#    }
#  }
#
#  print DOTFILE "}\n";
}

#Regular generation :

$current_depth = -1;
while($current_depth < $depth) {
  %father = ();
  %depth = ();
  %weight = ();
  %children = ();
  $last_node = 0;
  $depth{0} = 0;
  $weight{0} = int rand(1+$work_max - $work_min) + $work_min;

   my @left_nodes = (0);
  while(@left_nodes) {
    $current_node = shift @left_nodes;
      $current_depth = $depth{$current_node};

    last if $current_depth == $depth;
    my $children = ($degree_max + $degree_min + 1)/2;
    my @children;
    for(1..$children) {
      $last_node++;
      $father{$last_node} = $current_node;
      $weight{$last_node} = int rand(1+$work_max - $work_min) + $work_min;
      $depth{$last_node} = $current_depth + 1;
      push @left_nodes, $last_node;
      push @children, $last_node;
    }
    $children{$current_node} = [ @children ];
  }
}

save_graph($file_prefix . "_$tree.dot", $last_node + 1, \%children, \%weight);

#open(DOTFILE, ">$file_prefix" . "_" . ($nb_trees + 1) . ".dot") or die $!;
#
#print DOTFILE "graph {\n";
#
#for(0..$last_node) {
#  #print join(' ', @{$children{$_}}) if defined $children{$_};
#  foreach my $child (@{$children{$_}}) {
#    print DOTFILE " $_ -- $child; /* $weight{$child} */\n";
#  }
#}
#
#print DOTFILE "}\n";


sub save_graph {
  my ($filename, $nodes, $children_ref, $weights_ref) = @_;

  my @nodes = (keys %{$children_ref});
  open(FILE, "> $filename");
  print FILE "$nodes\n";
  for my $node (0..$nodes-1) { #(@nodes) {
    if (defined @{$children_ref->{$node}}) {
      my @children = @{$children_ref->{$node}};
      print FILE "$node : $weights_ref->{$node} / " . scalar @children . ' ' . join(',', @children) . "\n";
    } else {
      print FILE "$node : $weights_ref->{$node} / 0\n";
    }
  }
  close(FILE);
}

