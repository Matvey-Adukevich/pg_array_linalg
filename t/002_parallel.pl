#!/usr/bin/perl

use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

plan tests => 2;

my $node = PostgreSQL::Test::Cluster->new('main');
$node->init;
$node->start;

$node->safe_psql('postgres', 'CREATE EXTENSION pg_array_linalg;');

my $session1 = $node->background_psql('postgres');
my $session2 = $node->background_psql('postgres');

my $result1 = $session1->query_safe("SELECT array_dot(ARRAY[1,2,3], ARRAY[4,5,6]);");
my $result2 = $session2->query_safe("SELECT array_dot(ARRAY[2,3,4], ARRAY[5,6,7]);");

$session1->quit;
$session2->quit;

chomp($result1);
chomp($result2);

is($result1, '32', 'First parallel array_dot call');
is($result2, '56', 'Second parallel array_dot call');

$node->stop;
