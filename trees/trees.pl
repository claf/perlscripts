#!/usr/bin/env perl
# CALL :
# for first and second tree :
# ./trees.pl 1 depth x work x x 1 file 
# ./trees.pl 1 10    0 5    0 0 1 graphs
# 
# for general case :
# ./trees.pl 0 depth min_work max_work min_degree max_degree nb_trees file
# ./trees.pl 0 10    1        20       0          5          20       graphs

use strict;
use warnings;
use Data::Dumper;
my $special_case = $ARGV[0];
my $depth = $ARGV[1];
my $work_min = $ARGV[2];
my $work_max = $ARGV[3];
my $degree_min = $ARGV[4];
my $degree_max = $ARGV[5];
my $nb_trees = $ARGV[6];
my $file_prefix = $ARGV[7];

die "use: tree.pl special_case depth work_min work_max degree_min degree_max nb_trees file_prefix" unless defined $degree_max;

if (!defined $nb_trees) {$nb_trees = 1;}
if (!defined $file_prefix) {$file_prefix = "test";}

my $current_node;
my $last_node;
my %father;
my %depth;
my %weight;
my %total_weight;
my %children;


my $tree = 0;
my $current_depth = -1;

if ($special_case == 1) {

# First tree:

  for $tree (0..$nb_trees/2) {
    while($current_depth < $depth) {
      %father = ();
      %depth = ();
      %weight = ();
      %total_weight = ();
      %children = ();
      $last_node = 0;
      $depth{0} = 0;
      $weight{0} = $work_max; #int rand(1+$work_max - $work_min) + $work_min;
      $total_weight{0} = $work_max;

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
        my $children = 2;#int rand(1+$degree_max - $degree_min) + $degree_min;
        my @children;
        for(1..$children) {
          $last_node++;
          $father{$last_node} = $only_father;
          $weight{$last_node} = $work_max; #int rand(1+$work_max - $work_min) + $work_min;
          $total_weight{$last_node} = $work_max * ($current_depth + 1);
          $depth{$last_node} = $current_depth + 1;
          push @left_nodes, $last_node;
          push @children, $last_node;
        }
        foreach my $child (@children) {
          push @{$children{$only_father}}, $child;
        }
      }
    }
    save_graph($file_prefix . "_$weight{0}_" . "$tree.dot", $last_node + 1, \%children, \%weight, \%total_weight, $special_case);
  }

#Regular generation :

  $tree = $nb_trees + 1;
  for $tree ((($nb_trees/2)+1)..$nb_trees) {
    $current_depth = -1;
    while($current_depth < $depth) {
      %father = ();
      %depth = ();
      %children = ();
      $last_node = 0;
      $depth{0} = 0;

      my @left_nodes = (0);
      while(@left_nodes) {
        $current_node = shift @left_nodes;
        $current_depth = $depth{$current_node};

        last if $current_depth == $depth;
        my $children = 2;#($degree_max + $degree_min + 1)/2;
        my @children;
        for(1..$children) {
          $last_node++;
          $father{$last_node} = $current_node;
#      $weight{$last_node} = int rand(1+$work_max - $work_min) + $work_min;
          $depth{$last_node} = $current_depth + 1;
          push @left_nodes, $last_node;
          push @children, $last_node;
        }
        $children{$current_node} = [ @children ];
      }
    }
    save_graph($file_prefix . "_$weight{0}_$tree.dot", $last_node + 1, \%children, \%weight, \%total_weight, $special_case);
}
} else { # ($special_case != 1)

# General case :
  my $tree = 1;
  while ($tree <= $nb_trees) {
    print "TREE : $tree\n";
    $current_depth = -1;
    while($current_depth < $depth) {
      %father = ();
      %depth = ();
      %weight = ();
      %total_weight = ();
      %children = ();
      $last_node = 0;
      $depth{0} = 0;
      $weight{0} = int rand(1+$work_max - $work_min) + $work_min;
      $total_weight{0} = $weight{0};

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
          $total_weight{$last_node} = $weight{$last_node} + $total_weight{$current_node};
          $depth{$last_node} = $current_depth + 1;
          push @left_nodes, $last_node;
          push @children, $last_node;
        }
        $children{$current_node} = [ @children ];
      }
    }

    my $result = save_graph($file_prefix . "_$tree.dot", $last_node + 1, \%children, \%weight, \%total_weight, $special_case);
    if ($result == 1) {
      $tree++;
    }
  }
}

sub save_graph {
  my ($filename, $nodes, $children_ref, $weights_ref, $total_weights_ref, $specif) = @_;

  my @nodes = (keys %{$children_ref});
  open(FILE, "> $filename") or die $!;
  print FILE "$nodes\n";
  my $D = 0;
  my $W = 0;
  for my $node (0..$nodes-1) { #(@nodes) {
    if (defined @{$children_ref->{$node}}) {
      my @children = @{$children_ref->{$node}};
      print FILE "$node : $weights_ref->{$node} / " . scalar @children . ' ' . join(',', @children) . "\n";
    } else {
      print FILE "$node : $weights_ref->{$node} / 0\n";
      if ($total_weights_ref->{$node} > $D) {
        $D = $total_weights_ref->{$node};
      }
    }
    $W += $weights_ref->{$node};
  }



  my $x = 15000;
  my $y = 20000;

  # Only keep trees having x < W < y :
  # Only keep trees having W/16 > 10xD :
  if (((10*$D) >= int($W/16)) ||
      (($specif == 0) && ((int($W/16) <= $x)||(int($W/16) >= $y)))) {
    if (unlink($filename) != 0) {
      print "$filename\t10 x D : " . (10*$D) . "\tW / 16 : " . int($W/16) . " :\tFile deleted successfully.\n";
      close(FILE);
      return 0;
    } else {
      print "ERROR : File was not deleted.\n";
      close(FILE);
      return 0;
    }
  } else {
    print FILE "##$filename\t10 x D : " . (10*$D) . "\tW / 16 : " . int($W/16) . "\n";
    close(FILE);
    return 1;
  }
}

sub save_graph_dot {
  my ($filename, $nodes, $children_ref, $weights_ref) = @_;

  open(DOTFILE, "> $filename") or die $!;

  print DOTFILE "graph {\n";

  for(0..$nodes-1) {
    foreach my $child (@{$children_ref->{$_}}) {
      print DOTFILE " $_ -- $child; /* $weights_ref->{$child} */\n";
    }
  }

  print DOTFILE "}\n";
  close (DOTFILE);
}

