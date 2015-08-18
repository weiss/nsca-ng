package Net::NSCAng::Client;

use 5.010001;
use strict;
use warnings;
use Carp;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Net::NSCAng::Client ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Net::NSCAng::Client', $VERSION);

sub new {
    my $class = shift;
    my %args = @_%2 ? %{$_[0]} : @_;
    $class->_new(
        $args{host},
        $args{port} // 5668,
        @args{qw/ identity psk ciphers node_name svc_name /},
        $args{timeout} // 10,
    );
}

sub svc_result {
    my $self = shift;
    my %args = @_%2 ? %{$_[0]} : @_;
    my $err = $self->_result(0,
        @args{qw/ node_name svc_description return_code plugin_output /}
    );
    $err and croak("svc_result: $err");
}

sub host_result {
    my $self = shift;
    my %args = @_%2 ? %{$_[0]} : @_;
    my $err = $self->_result(1,
        $args{node_name}, '', @args{qw/ return_code plugin_output /}
    );
    $err and croak("host_result: $err");
}

1;
__END__

=head1 NAME

Net::NSCAng::Client - Submit NSCA results using the NSCA-ng protocol

=head1 SYNOPSIS

  use Net::NSCAng::Client;

=head1 DESCRIPTION


=head2 EXPORT

None by default.


=head1 SEE ALSO


=head1 AUTHOR

Matthias Bethke, E<lt>matthias@towiski.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2015 by Matthias Bethke

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.20.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
