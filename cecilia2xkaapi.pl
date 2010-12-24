#!/usr/bin/perl

use strict;
use warnings;

my $header_file = "main.h";

open(HEADER, ">$header_file") or die $!;

print HEADER <<EOP;
/*
 * Generated header : $header_file.
 *
 */

#include <kaapi.h>
#include <stddef.h>
EOP

foreach my $file (@ARGV)
{
  open(FILEHANDLER, $file) or die $!;
  while (my $line = <FILEHANDLER>)
  {
    if ($line =~ / METHOD\s*\(.*,\s*(.*)\)\s*\(\s*void\s*\*\s*_this\s*,*(.*)\)/)
    {
      my $function = $1;
      my @args = split (/,/, $2);

      &WriteHeader ($function, @args);
    }
  }
}

open(HEADER, $header_file) or die $!;

foreach my $file (@ARGV)
{
  my @splitted_path = split (/\//, $file);
  my $file_out = $splitted_path[$#splitted_path];
  open(FILEHANDLER, $file) or die $!;
  open(FILEOUT, ">$file_out") or die $!;

  while (my $line = <FILEHANDLER>)
  {
    # Suppress DECLARE_DATA :
    my $nb_cb = 0;
    if($line =~ /DECLARE_DATA\s*{/)
    {
      $nb_cb++;
      while(defined($line = <FILEHANDLER>) && $nb_cb != 0)
      {
        if(my @arr = $line =~ /{/g)
        {
          $nb_cb += @arr;
        }

        if(my @arr = $line =~ /}/g)
        {
          $nb_cb -= @arr;
        }
      }
    }

    # Change included header :
    elsif($line =~ /#include <cecilia\.h>/)
    {
      print FILEOUT "\#include \"$header_file\"";
    }

    # Method re-definition and header update :
    elsif ($line =~ / METHOD\s*\(.*,\s*(.*)\)\s*\(\s*void\s*\*\s*_this\s*,*(.*)\)/)
    {
      my $function = $1;
      my @args = split (/,/, $2);


      $line = <FILEHANDLER>;
      if ($line !~ /{/)
      {
        print FILEOUT $line;
      }

      print FILEOUT "void ".$function."_body (void* taskarg, kaapi_thread_t* thread)\n"; 
      print FILEOUT "{\n";
      print FILEOUT "  ".$function."_arg_t* arg0 = (".$function."_arg_t*) taskarg;\n\n";


      if ($#args != -1)
      {
        foreach my $arg (@args)
        {
          $arg =~ /.*\s\**([A-Za-z_][A-Za-z0-9_]*)/ or die "No match found";
          print FILEOUT " $arg = arg0->$1;\n";
        }
        print FILEOUT "\n";
      }
    }

    # CALL becomes task :
#    elsif ($line =~ /^\s*CALL\s*\([^,]*,\s*([^,]*),(.*)\)\s*;/)
    elsif ($line =~ /^\s*CALL\s*\([^,]*,\s*(.*)\)\s*;/)
    {
      my @args = split (/,/, $1);
      my $function = $args[0];

      print FILEOUT "  kaapi_task_t* task;\n";
      print FILEOUT "  ".$function."_arg_t* ".$function."_args;\n";
      print FILEOUT "  task = kaapi_thread_toptask(thread);\n";
      print FILEOUT "  kaapi_task_initdfg( task, ".$function."_body, kaapi_thread_pushdata(thread, sizeof(".$function."_arg_t)) );\n";
      print FILEOUT "  ".$function."_args = kaapi_task_getargst( task, ".$function."_arg_t );\n";

      for (my $i = 1; $i <= $#args; $i++)
      {
        #$args[$i] =~ /.*\s\**([A-Za-z_][A-Za-z0-9_]*)/ or die "No match found";
        my $var_name = &Var_Name_Func ($function, $i);
        print FILEOUT "  ".$function."_args->".$var_name." = $args[$i];\n";
      }

      print FILEOUT "  kaapi_thread_pushtask(thread);\n\n";
    }

    # CALLMINE becomes task :
    elsif ($line =~ /^\s*CALLMINE\s*\([^,]*,\s*(.*)\)\s*;/)
    {
      my @args = split (/,/, $1);
      my $function = $args[0];

      print FILEOUT "  kaapi_task_t* task;\n";
      print FILEOUT "  ".$function."_arg_t* ".$function."_args;\n";
      print FILEOUT "  task = kaapi_thread_toptask(thread);\n";
      print FILEOUT "  kaapi_task_initdfg( task, ".$function."_body, kaapi_thread_pushdata(thread, sizeof(".$function."_arg_t)) );\n";
      print FILEOUT "  ".$function."_args = kaapi_task_getargst( task, ".$function."_arg_t );\n";

      for (my $i = 1; $i <= $#args; $i++)
      {
        #$args[$i] =~ /.*\s\**([A-Za-z_][A-Za-z0-9_]*)/ or die "No match found";
        my $var_name = &Var_Name_Func ($function, $i);
        print FILEOUT "  ".$function."_args->".$var_name." = $args[$i];\n";
      }

      print FILEOUT "  kaapi_thread_pushtask(thread);\n\n";
    }

    # Normal C line :
    else
    {
      print FILEOUT $line;
    }
  }
  print FILEOUT "\n";
}

sub Var_Name_Func {
  (my $function, my $index) = @_;
  open (HEADER, $header_file);

  while (my $line = <HEADER>)
  {
    if ($line =~ /typedef struct ($function)_arg_t/)
    {
      for (my $j = 0; $j < $index; $j++)
      {
        $line = <HEADER>;
      }
      $line =~ /.*\s\**([A-Za-z_][A-Za-z0-9_]*)/;
      return $1;
    }
  }
}

sub WriteHeader {
  (my $function, my @args) = @_;
  #@args = split (/,/, $arguments);

  print HEADER "\n/**********************************************************************/\n\n";
  print HEADER "/* $function function prototype : */\n\n";

  print HEADER "void ".$function."_body (void* taskarg, kaapi_thread_t* thread);\n\n";  

  print HEADER "/* $function argument structure : */\n\n";

  print HEADER "typedef struct ".$function."_arg_t {\n";
  foreach my $my_arg (@args)
  {
    print HEADER "\t$my_arg;\n";
  }
  print HEADER "} ".$function."_arg_t;\n\n";

  print HEADER "/* $function task decl. : */\n\n";

  print HEADER "KAAPI_REGISTER_TASKFORMAT ( ".$function."_body_format,\n";
  print HEADER "\t\"".$function."\",\n";
  print HEADER "\t".$function."_body,\n";
  print HEADER "\tsizeof (".$function."_arg_t),\n";
  print HEADER "\t".($#args+1).",\n";
  print HEADER "\t(kaapi_access_mode_t[]) {";

  if ($#args == -1)
  {
    print HEADER "},\n";
  }
  else
  {
    for (my $i = 0; $i < $#args; $i++)
    {
      print HEADER "KAAPI_ACCESS_MODE_V, ";
    }

    print HEADER "KAAPI_ACCESS_MODE_V},\n";
  }
  
  # data field :
  print HEADER "\t(kaapi_offset_t[]) {";

  if ($#args == -1)
  {
    print HEADER "},\n";
  }
  else
  {
    my $argc = $#args;
    foreach my $arg (@args)
    {
      $arg =~ /.*\s\**([A-Za-z_][A-Za-z0-9_]*)/ or die "No match found";
      my $var_name = $1;
      print HEADER "offsetof (".$function."_arg_t, $var_name)";
      if ($argc != 0)
      {
        $argc--;
        print HEADER ", ";
      }
    }

    print HEADER "},\n";
  }

  # version field :
  print HEADER "\t(kaapi_offset_t[]) {";

  if ($#args == -1)
  {
    print HEADER "},\n";
  }
  else
  {
    my $argc = $#args;
    foreach my $arg (@args)
    {
      print HEADER "0";
      if ($argc != 0)
      {
        $argc--;
        print HEADER ", ";
      }
    }

    print HEADER "},\n";
  }

  print HEADER "\t(const struct kaapi_format_t*[]) {";

  if ($#args == -1)
  {
    print HEADER "}, 0)\n\n";
  }
  else
  {
    my $argc = $#args;
    foreach my $arg (@args)
    {
      if ($arg =~ /int [A-Za-z_][A-Za-z0-9_]*/)
      {
        print HEADER "kaapi_int_format";
      }
      else
      {
        print HEADER "kaapi_ulong_format";
      }

      if ($argc != 0)
      {
        $argc--;
        print HEADER ", ";
      }
    }

    print HEADER "}, 0)\n\n";
  }
}
