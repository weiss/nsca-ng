use strict;
use warnings;
use POSIX qw(setlocale LC_ALL);
use Test::More tests => 14;
use Test::Exception;
use Config;
use Net::NSCAng::Client;

BEGIN {
    setlocale(LC_ALL, "C");
    $Config{useithreads}
        and warn "WARNING: Net::NSCAng::Client is not thread safe but your perl has threads enabled!\n";
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

dies_ok(sub { $n->command("BOGUS_COMMAND;1;2;3") }, 'command() dies w/o argument');
lives_ok(sub {crf(sub { $n->command("BOGUS_COMMAND;1;2;3") })}, 'command() works');
# Connection-refused-filter
# Supresses exceptions with a "connection refused" error as this is expected
sub crf {
    my $sub = shift;
    eval { $sub->() };
    if($@) {
        die $@ unless $@ =~ /SSL error:Connection refused/;
    }
}
