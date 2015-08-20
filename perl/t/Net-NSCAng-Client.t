use strict;
use warnings;

use Test::More;
use Test::Exception;
BEGIN { use_ok('Net::NSCAng::Client', ':all') };

my @cparams = qw/ localhost myid s3cr3t /;
my @nn = (node_name => 'here');
my @sd = (svc_description => 'bogus');
my ($n, $setup);


lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams) }, 'Simple constructor');
$setup = sub { Net::NSCAng::Client->new(@cparams) };

throws_ok(sub { tws('host_result', 0, "OK") }, qr/node_name missing/, 'host_result() dies w/o node_name');
throws_ok(sub { tws('svc_result', 0, "OK") }, qr/svc_description missing/, 'svc_result() dies w/o node_name');
lives_ok(sub { crf(sub { tws('host_result', 0, "OK", @nn) })}, 'host_result() with local node_name');
throws_ok(sub { tws('svc_result', 0, "OK", @nn) }, qr/svc_description missing/, 'svc_result() still dies with local node_name');


lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams, @nn) }, 'Constructor with node_name');
$setup = sub { Net::NSCAng::Client->new(@cparams, @nn) };

lives_ok(sub {crf(sub { tws('host_result', 0, "OK") })}, 'host_result() with node_name from constructor');
throws_ok(sub { tws('svc_result', 0, "OK") }, qr/svc_description missing/, 'svc_result() dies w/o svc_description');
lives_ok(sub {crf(sub { tws('svc_result', 0, "OK", @sd) })}, 'svc_result() with local svc_description');


lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams, @nn, @sd) }, 'Constructor with node_name');
$setup = sub { Net::NSCAng::Client->new(@cparams, @nn, @sd) };
lives_ok(sub { crf(sub { tws('host_result', 0, "OK") })}, 'host_result() OK w/o local params');
lives_ok(sub { crf(sub { tws('svc_result', 0, "OK") })}, 'svc_result() OK w/o local params');
done_testing;


# Connection-refused-filter
# Supresses exceptions with a "connection refused" error as this is expected
sub crf {
    my $sub = shift;
    eval { $sub->() };
    if($@) {
        die $@ unless $@ =~ /: SSL error:Connection refused/;
    }
}

# Try with setup
# Calls $setup->() to obtain a new object before calling the argument method
sub tws {
    my $method = shift;
    my $n;
    lives_ok { $n = $setup->() };
    $n->$method(@_);
}
