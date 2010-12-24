package Component;

use strict;
use warnings;

use File::Find;
use XML::Simple;

our %components;
my @fractal_files;
my $filename;
my $init = 0;

sub new {
	my $class = shift;
	my $self = {};
	my $xml = shift;
	
	# store the main component name :
	$self->{name} = $xml->{'name'};
	
	# Mark the main Component for later display :
	if ($init != 1)
	{
		$self->{main} = 1;
		$init = 1;
	}
	if (defined $xml->{'component'})
	{
		$self->{components} = {};
		# not necessary anymore?
		#$self->{components}->{'this'} = $self->{name};
	}
	
	# Definition : an inlined component definition is a component which is entirely defined into
	# another component definition file, otherwise, the 'definition' attribut points to the file
	# containing the component definition.
	
	# components stuff :
	foreach(keys %{$xml->{'component'}})
	{
		# if it's an inlined component definition :
		unless (defined $xml->{'component'}->{$_}->{'definition'})
		{
			$xml->{'component'}->{$_}->{'name'} = $_;
			$self->{components}->{$_} = $_;
			new Component($xml->{'component'}->{$_});
		}
		else # it's not an inlined component definition
		{
			# just keep the actual filename (without the '.fractal' extension) :
			$filename = $xml->{'component'}->{$_}->{'definition'};
			$self->{components}->{$_} = $filename;
			$filename =~s/\./\//g;
			
			undef (@fractal_files);
			find(\&filter_file_names, '.');

            # Deal with multiple files :
            if ($#fractal_files != 0)
            {
            	print "ERROR : Multiple files (or none) find for " . $filename . ".fractal\n";
	            print $_ . "\n" foreach @fractal_files;
            	die;
            }
            
			# need to parse the definition file which is almost :
			# $filename.fractal
			# TODO : uses $PWD
			new Component(XMLin($fractal_files[0], ForceArray => 1, KeyAttr => ['client','name']));
		}
	}
	

	
	
	# store interfaces :
	$self->{interfaces} = [];
	foreach(keys %{$xml->{'interface'}})
	{
		push @{$self->{interfaces}}, $_;
	}
	
	# store bindings :
	$self->{bindings} = [];
	foreach(keys %{$xml->{'binding'}})
	{
		my $client = $_;
		my $server = $xml->{'binding'}->{$_}->{'server'};
		# si c est this
		my ($client_component, $client_interface) = split /\./, $client;
		my ($server_component, $server_interface) = split /\./, $server;
		
		if ($client_component eq 'this')
		{
			$client_component = $self->{name};
		}
		else
		{
			$client_component = $self->{components}->{$client_component};
		}
		if ($server_component eq 'this')
		{
			$server_component = $self->{name} 
		}
		else
		{
			$server_component = $self->{components}->{$server_component};
		}
		
		push @{$self->{bindings}}, [ ($client_component, $client_interface, $server_component, $server_interface) ];
	}
	
	
	
	# finishing stuff :	
	bless ($self, $class);
	$components{$self->{name}} = $self;
	return $self;
}

sub display {
	my $self = shift;
	my $indent = shift;
	my $tab;
	my $i;

	for($i = 0; $i < $indent; $i++) {
		$tab = $tab . "\t";	
	}
	print $main::filehandle $tab . "subgraph \"cluster_$self->{name}\" {\n";
	print $main::filehandle $tab . "\tlabel = \"$self->{name}\";\n";
	print $main::filehandle $tab . "\tcolor = blue;\n";
	#display all components
	foreach(keys %{$self->{components}}) {
		next if $_ eq 'this';
		$components{$self->{components}->{$_}}->display($indent+1);
	}
	#display all interfaces
	foreach(@{$self->{interfaces}}) {
		print $main::filehandle $tab . "\t\"$self->{name}::$_\" [label=\"$_\"];\n";
	}
	#now, display all edges
	foreach(@{$self->{bindings}}) {
		my ($cc, $ci, $sc, $si) = @{$_};
		print $main::filehandle $tab . "\t\"$cc"."::$ci\" -> \"$sc"."::$si\"\n";
	}
	print $main::filehandle $tab . "}\n";
}

sub filter_file_names {
        push @fractal_files, $File::Find::name if $File::Find::name =~ /$filename\.fractal$/;
}
return 1;
