#!/bin/env perl
use strict;
use Data::Dumper;
my $fname = $ARGV[0];
my @questions;
my @answers;
if (!-e $fname) {
	die "File does not exist\n";
}
open(FILE, "<$fname");
while(<FILE>) {
	chomp;
	if (length($_) == 0) {
		next;
	}
	if ($_ =~ /^Q(?: \d+){0,1}\. (.*)$/) {
		my $question = $1;
		my $current = \$question;
		my $answer;
		my $break = 0;
		while (<FILE>) {
			chomp;
			if (length($_) == 0) {
				if ($break == 1) { last; } else { next; };
			}
			my $line = $_;
			if ($_ =~ /^A\. (.*)$/) {
				$current = \$answer;
				$line = $1;
				$break = 1;
			}
			$$current .= $line."\n";
		}
		$question =~ s/&/&amp;/g;
		$question =~ s/"/&quot;/g; #"
		$question =~ s/</&lt;/g;
		$question =~ s/>/&gt;/g;
		$answer =~ s/&/&amp;/g;
		$answer =~ s/"/&quot;/g; #"
		$answer =~ s/</&lt;/g;
		$answer =~ s/>/&gt;/g;
		$answer =~ s/(http:\/\/[^ ]*)/<a href="$1">$1<\/a>/g;
		push(@questions, $question);
		push(@answers, $answer);
	}
}
close(FILE);
print qq|<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN"
   "http://www.w3.org/TR/html4/strict.dtd">
<html>
	<head>
		<title>DansGuardian 2.9 FAQ</title>
	</head>
	<body>
		<h1>Contents</h1>
		<ol id="top">
|;
my $count = 1;
foreach my $question (@questions) {
	print qq|			<li><a href="#q$count">$question</a></li>\n|;
	$count++;
}
print qq|		</ol>
		<h1>FAQ</h1>
|;
$count = 0;
foreach my $question (@questions) {
	my $answer = $answers[$count++];
	chomp $answer;
	$answer =~ s/\n/<\/p><p>/g;
	print qq|		<p id="q$count" style="margin-top: 2em;"><a href="#top">^</a> <strong>$count.</strong> <em>$question</em></p>
		<p>$answer</p>
|;
}
print qq|	</body>
</html>
|;
