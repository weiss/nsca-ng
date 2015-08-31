use strict;
use warnings;
use POSIX qw(setlocale LC_ALL);
use Test::More;
use Test::Exception;

BEGIN {
    setlocale(LC_ALL, "C");
    use_ok('Net::NSCAng::Client', ':all');
};

my @cparams = qw/ localhost myid s3cr3t /;
my @nn = (node_name => 'here');
my @sd = (svc_description => 'bogus');
my $n;

lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams) }, 'Simple constructor');
dies_ok(sub { $n->host_result(0, "OK") }, 'host_result() dies w/o node_name');
dies_ok(sub { $n->svc_result(0, "OK") }, 'svc_result() dies w/o node_name');
lives_ok(sub { crf(sub { $n->host_result(0, "OK", @nn) })}, 'host_result() with local node_name');
dies_ok(sub { $n->svc_result(0, "OK", @nn) }, 'svc_result() still dies with local node_name');

lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams, @nn) }, 'Constructor with node_name');
lives_ok(sub {crf(sub { $n->host_result(0, "OK") })}, 'host_result() with node_name from constructor');
dies_ok(sub { $n->svc_result(0, "OK") }, 'svc_result() dies w/o svc_description');
lives_ok(sub {crf(sub { $n->svc_result(0, "OK", @sd) })}, 'svc_result() with local svc_description');

lives_ok(sub { $n = Net::NSCAng::Client->new(@cparams, @nn, @sd) }, 'Constructor with node_name');
lives_ok(sub { crf(sub { $n->host_result(0, "OK") })}, 'host_result() OK w/o local params');
lives_ok(sub { crf(sub { $n->svc_result(0, "OK") })}, 'svc_result() OK w/o local params');

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
