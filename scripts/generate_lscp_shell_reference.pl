#!/usr/bin/perl -w

# Updates the built-in LSCP documentation reference of the LSCP shell.
#
# Copyright (c) 2014-2016 Christian Schoenebeck
#
# Extracts all sections from Documentation/lscp.xml marked with our magic
# XML attribute "lscp_cmd=true" (and uses the section's "anchor" XML attribute
# for *knowing* the respective exact LSCP command of the section).
# Then src/network/lscp_shell_reference.cpp is generated by this script with
# the documentation for each individual LSCP command extracted.
#
# Usage: generate_lscp_shell_reference.pl [--output=OUTFILE] [--debug-xml-extract]

use XML::Parser;
use Data::Dumper; # just for debugging
use Storable qw(dclone);

my $YACC_FILE = "../Documentation/lscp.xml";
my $REFERENCE_CPP_FILE = "../src/network/lscp_shell_reference.cpp";

###########################################################################
# class MyDOM
#
# Wraps the data model returned by XML::Parser and provides convenient methods
# to access the model in DOM style. Because the tree model provided by
# XML::Parser uses a very inconvenient layout, which would require a lot of
# hard readable and error prone code if accessed directly.

package MyDOM;

# $dom = MyDOM->new($doc);
sub new {
    my ($class, $self) = @_;    
    my $data = {
        'type'    => 0,
        'attr'    => 0,
        'content' => $self
    };
#     print ::Dumper($self) . "\n";
    return bless $data, $class;
}

# $element = $dom->element($name, [$index = 0]);
sub element {
    my $self = shift @_;
    my $name = shift @_;
    my $nr    = (@_) ? shift @_ : 0;
    my $content = $self->{content};
    my $i = 0;
    my $k = 0;
    CYCLE: while ($i + 1 < @$content) {
        my $type = $content->[$i++];
        my $subContent = $content->[$i++];

        next CYCLE if ($type ne $name);

        if ($k == $nr) {
            my $attr = ($type && @$subContent) ? $subContent->[0] : 0;
            my $subContentClone = ($type) ? ::dclone($subContent) : $subContent; # clone it, since we will modify it next
            if ($attr && $type) { shift @$subContentClone; } # drop first element, which contains attributes
            my $data = {
                'type'    => $type,
                'attr'    => $attr,
                'content' => $subContentClone
            };
            return bless $data, 'MyDOM';
        }
        $k++;
    }
    return 0;
}

# $element = $dom->elementNr(4);
sub elementNr {
    my $self = shift @_;
    my $nr   = shift @_;
    my $content = $self->{content};
    my $i = 0;
    my $k = 0;
    while ($i + 1 < @$content) {
        my $type = $content->[$i++];
        my $subContent = $content->[$i++];
        if ($k == $nr) {
            my $attr = ($type && @$subContent) ? $subContent->[0] : 0;
# print "type $type\n";
# print ::Dumper($subContent) . "\n";
            my $subContentClone = ($type) ? ::dclone($subContent) : $subContent; # clone it, since we will modify it next
            if ($attr && $type) { shift @$subContentClone; } # drop first element, which contains attributes
            my $data = {
                'type'    => $type,
                'attr'    => $attr,
                'content' => $subContentClone
            };
            return bless $data, 'MyDOM';
        }
        $k++;
    }
    return 0;
}

# $s = $element->name();
sub name {
    my $self = shift @_;
    return $self->{type};
}

# $s = $element->attr("anchor");
sub attr {
    my $self = shift @_;
    my $name = shift @_;
    if (!$self->{attr} || !exists $self->{attr}->{$name}) {
        return 0;
    }
    return $self->{attr}->{$name};
}

# $s = $element->body();
sub body {
    my $self = shift @_;
    $s = "";
    if (!$self->{type}) {
        return $self->{content};
    }
    for (my $i = 0; $self->elementNr($i); $i++) {
        $e = $self->elementNr($i);
        if (!$e->name()) {
            $s .= $e->{content};
        }
    }
    return $s;
}

# $element->dumpMe();
sub dumpMe {
    my $self = shift @_;
    print "[dumpME()]: " . ::Dumper($self->{content}) . "\n";
}

###########################################################################
# main app

package main;

# parse command line argument(s)
my $g_debug_xml_extract = 0;
foreach $arg (@ARGV) {
    if ($arg eq "--debug-xml-extract") {
        $g_debug_xml_extract = 1;
    } elsif ($arg =~ /^--output/) { # argument --output=OUTFILE
        my ($name, $value) = split(/=|\s+/, $arg); # key value separated by space or "=" character
        $REFERENCE_CPP_FILE = $value;
    }
}

# will be populated by collectCommands()
my $g_cmds = { };

# collectCommands($dom);
sub collectCommands {
    my $dom = shift @_;
    for (my $i = 0; $dom->element("section", $i); $i++) {
        my $section = $dom->element("section", $i);
        if ($section->attr("lscp_cmd")) {
            if (!$section->attr("anchor")) {
                die "ERROR: Section deteced with 'lscp_cmd' attribute, but without 'anchor' attribute.";
            }
            my $name = $section->attr("anchor");
            if (exists $g_cmds->{$name}) {
                die "ERROR: Multiple occurence of LSCP command detected: $name";
            }
            $g_cmds->{$name} = $section;
        } else {
            collectCommands($section);
        }
    }
}

# removes redundant white spaces
sub trimAll {
    my $s = shift;
    # replace tabs by space
    $s =~ s/\t/ /g;
    # replace occurences of more than one space character by only one space
    # character (including new line character)
    $s =~ s/\s+/ /g;
    # remove leading white spaces
    $s =~ s/^\s+//g;
    # remove trailing white spaces
    $s =~ s/\s+$//g;
    return $s;
}

# creates an optional space intended to be appended to the given string
sub wordSepFor {
    my $s = shift;
    if ($s eq '') { return ""; }
    if ($s =~ /\n$/) { return ""; }
    return " ";
}

# $s = encodeXref($xref);
sub encodeXref {
    my $xref = shift;
    return trimAll($xref->body());
}

# $s = encodeT($t);
sub encodeT {
    my $t = shift;
    my $s = "";
    for (my $i = 0; $t->elementNr($i); $i++) {
        $e = $t->elementNr($i);
        $type = $e->name();
        if (!$type) {
            $s .= wordSepFor($s);
            $s .= trimAll($e->body());
        } elsif ($type eq "t") {
            $s .= wordSepFor($s);
            $s .= encodeT($e);
        } elsif ($type eq "list") {
            $s .= wordSepFor($s);
            $s .= encodeSection($e);
        } elsif ($type eq "xref") {
            $s .= wordSepFor($s);
            $s .= encodeXref($e);
        }
    }
    if (!($s =~ /\n\n$/)) { $s .= "\n\n"; }
    return $s;
}

# $s = encodeSection($section);
sub encodeSection {
    my $section = shift;
    my $s = "";
    for (my $i = 0; $section->elementNr($i); $i++) {
        $e = $section->elementNr($i);
        $type = $e->name();
        if (!$type) {
            # nothing here for now
        } elsif ($type eq "t") {
            $s .= wordSepFor($s);
            $s .= encodeT($e);
        } elsif ($type eq "list") {
            $s .= wordSepFor($s);
            $s .= encodeSection($e);
        } elsif ($type eq "xref") {
            $s .= wordSepFor($s);
            $s .= encodeXref($e);
        }
    }
    return $s;
}

# open and parse lscp.xml
my $parser = XML::Parser->new(Style => 'Tree');
my $doc = $parser->parsefile($YACC_FILE);
my $dom = MyDOM->new($doc);
my $middle = $dom->element("rfc")->element("middle");

# extract all sections from the document with the individual LSCP commands
collectCommands($middle);

# if --debug-xml-extract is supplied, just show the result of XML parsing and exit
if ($g_debug_xml_extract) {
    while (my ($name, $section) = each(%$g_cmds)) {
        print "-> " . $name . "\n";
        print encodeSection($section);
    }
    exit(0);
}

# start generating lscp_shell_reference.cpp ...
open(OUT, ">", $REFERENCE_CPP_FILE) || die "Can't open LSCP shell doc reference C++ file for output";
print OUT <<EOF_BLOCK;
/*****************************************************************************
 *                                                                           *
 *  LSCP documentation reference built into LSCP shell.                      *
 *                                                                           *
 *  Copyright (c) 2014 - 2016 Christian Schoenebeck                          *
 *                                                                           *
 *  This program is part of LinuxSampler and released under the same terms.  *
 *                                                                           *
 *  This source file is auto generated by 'generate_lscp_shell_reference.pl' *
 *  from 'lscp.xml'. Thus do not modify this C++ file directly!              *
 *                                                                           *
 *****************************************************************************/

/*
   This C++ file should automatically be re-generated if lscp.xml was
   modified, if not, you may call "make parser" explicitly.
 */

#include "lscp_shell_reference.h"
#include <string.h>

static lscp_ref_entry_t lscp_reference[] = {
EOF_BLOCK
while (my ($name, $section) = each(%$g_cmds)) {
    # convert reference string block into C-style string format
    my $s = encodeSection($section);
    $s =~ s/\n/\\n/g;
    $s =~ s/\"/\\\"/g;
    # split reference string into equal length chunks, so we can distribute
    # them over lines, in order to not let them float behind 80 chars per line
    my @lines = unpack("(A70)*", $s);
    my $backSlashWrap = 0;
    print OUT "    { \"$name\",\n";
    foreach my $line (@lines) {
        if ($backSlashWrap) { $line = "\\" . $line; }
        $backSlashWrap = ($line =~ /\\$/);
        if ($backSlashWrap) { chop $line; }
        print OUT "      \"$line\"\n";
        
    }
    print OUT "    },\n";
}
print OUT <<EOF_BLOCK;
};

lscp_ref_entry_t* lscp_reference_for_command(const char* cmd) {
    const int n1 = (int)strlen(cmd);
    if (!n1) return NULL;
    int foundLength = 0;
    lscp_ref_entry_t* foundEntry = NULL;
    for (int i = 0; i < sizeof(lscp_reference) / sizeof(lscp_ref_entry_t); ++i) {
        const int n2 = (int)strlen(lscp_reference[i].name);
        const int n = n1 < n2 ? n1 : n2;
        if (!strncmp(cmd, lscp_reference[i].name, n)) {
            if (foundEntry) {
                if (n1 < foundLength && n1 < n2) return NULL;
                if (n2 == foundLength) return NULL;
                if (n2 < foundLength) continue;
            }
            foundEntry  = &lscp_reference[i];
            foundLength = n2;
        }
    }
    return foundEntry;
}
EOF_BLOCK
close(OUT);
exit(0); # all done, success
