package Net::NSCAng::Client;

use 5.010001;
use strict;
use warnings;
use Carp;

require Exporter;
our @ISA = qw(Exporter);

our %EXPORT_TAGS = ( 'all' => [ qw/ OK WARNING CRITICAL UNKNOWN DEPENDENT / ] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our $VERSION = '0.01';

use constant OK         => 0;
use constant WARNING    => 1;
use constant CRITICAL   => 2;
use constant UNKNOWN    => 3;
use constant DEPENDENT  => 4;

require XSLoader;
XSLoader::load('Net::NSCAng::Client', $VERSION);

=head1 NAME

Net::NSCAng::Client - Submit NSCA results using the NSCA-ng protocol

=head1 SYNOPSIS

  use Net::NSCAng::Client ':all';
  my $c = Net::NSCAng::Client->new('icinga.mydomain.org', 'myid', 's3cr3t',
    node_name => 'server_name',
  );
  $c->svc_result(WARNING, 'BogusService is bogus!', { svc_description => 'bogus' });
    

=head1 DESCRIPTION

Net::NSCAng::Client provides a means of sending host or service check results
to an L<NCSA-ng|http://www.nsca-ng.org/> server. It is derived from the Python
code written by Alexander Golovko <alexandro@onsec.ru> but with a more perlish
interface.

=head2 EXPORT

Nothing by default. The standard Nagios return codes C<OK>, C<WARNING>,
C<CRITICAL>, C<UNKNOWN> and C<DEPENDENT> are available.

=head1 METHODS

=head2 new($host, $identity, $psk, %tags)

The constructor takes three mandatory positional arguments and a number of
optional ones that may be passed as hash-style arguments inline or as a
hashref. The mandatory ones:

=over 4

=item C<$host>: The host name of the NSCA-ng server

=item C<$identity>: An arbitrary identity string that is used by the server to
find the corrent PSK. You can think of this as the "user name" part of a
regular login.

=item C<$psk>: A pre-shared key configured on the server for C<$identity>.

=back

The following tags are optional:

=over 4

=item C<port>: the TCP port the NSCA-ng srver runs on. Defaults to 5668.

=item C<ciphers>: A list of ciphers for OpenSSL to use. See ciphers(1) for
details and don't mess with this unless you know what you're doing. The default
is fine.

=item C<node_name>: The name of the local host as configured in your monitoring
system. This is almost always the host name. You may pass this as an argument
to any of the result methods, but considering that a single host or service
check hardly ever submits results for more than one host, it makes sense to
specify this at construction time and not worry about it later.

=item C<svc_description>: The service description as configured in your
monitoring system. Same rationale as C<node_name> above.

=back

The constructor dies with an error message if anything should go wrong, so use
C<eval>, L<Try::Tiny> or the like.

=cut

sub new {
    my ($class, $host, $identity, $psk) = splice(@_, 0, 4);
    my %args = @_%2 ? %{$_[0]} : @_;
    $class->_new($host, $args{port} // 5668, $identity, $psk,
        @args{qw/ ciphers node_name svc_description /},
        $args{timeout} // 10,
    );
}

=head2 svc_result(%tags)

Submit a service check result. C<$return_code> is a Nagios-style plugin return
code between 0 and 4, and C<$plugin_output> is a human-readable description.
The optional arguments may be passed hash-style inline or as a hashref just
like for the constructor. The supported keys are C<node_name> and
C<svc_description>, see L<new()> for their use. If specified here, the values
override anything that may have been set in the constructor. If you haven't set
them in the constructor, you MUST set them here though!

Errors are signaled by dying with an error message.

=cut

sub svc_result {
    my ($self, $return_code, $plugin_output) = splice(@_, 0, 3);
    my %args = @_%2 ? %{$_[0]} : @_;
    my $err = $self->_result(0,
        @args{qw/ node_name svc_description /}, $return_code, $plugin_output
    );
    $err and croak("svc_result: $err");
}

=head2 host_result(%tags)

Submit a host check result. This is the same as L<svc_result()>, te only
exception being that the C<svc_description> argument obviously makes no sense
for a host check and is thus not supported. If an C<svc_description> should
have been set in the constructor, it is ignored for this call.

=cut

sub host_result {
    my ($self, $return_code, $plugin_output) = splice(@_, 0, 3);
    my %args = @_%2 ? %{$_[0]} : @_;
    my $err = $self->_result(1, $args{node_name}, '', $return_code, $plugin_output);
    $err and croak("host_result: $err");
}

1;
__END__

=head1 BUGS

This module is not thread safe! Due do a workaround for the godawful OpenSSL
API, static data has to be used, and this I<will> cause problems with threads.
The original Python library uses pthreads to get around that but due to Perl's
different threading model this would probably not work. Using the C<MY_CXT>
macros is a possibility but frankly I'm too lazy to try just for supporting
Perl threads that don't work well in the first place.

=head1 SEE ALSO

L<send_nsca(8)>

=head1 AUTHOR

Matthias Bethke, E<lt>matthias@towiski.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2015 by Matthias Bethke
Portions Copyright (C) by Alexander Golovko <alexandro@onsec.ru> 

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.20.2 or,
at your option, any later version of Perl 5 you may have available.

=cut
