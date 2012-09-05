: # use perl -*-Perl-*-
 eval 'exec perl -S $0 ${1+"$@"}'
    if 0;

#####################################################################
#
#      File:   syslog_doc.pl
#      Name:   Harsha Bharadwaj
#
#      Description:
#       Generates syslog HTML file from syslog.def.
#
#
# Copyright (c) 1985-2008 by cisco Systems, Inc.
# All rights reserved.
#
#
# $Id: syslog_doc.pl,v 1.13 2004/03/24 04:01:19 harshabh Exp $
# $Source: /auto/vwsvua/kirsten/sanos3_fix/VegasSW__isan__platform__common__syslogd/export/isan/syslog_doc.pl,v $
# $Author: harshabh $
#
#####################################################################

#######Still need to do "use strict 'vars'"######

#Set of subroutines to trim the strings.
sub str_quotes {
   $_[0] =~ s/\\\"/\$/g;
   $_[0] =~ s/\"//g;
   $_[0] =~ s/\$/\"/g;
}

sub str_strip {
   $_[0] =~ s/\\\"/\$/g;
   $_[0] =~ s/\"//g; 
   $_[0] =~ s/^[ ]//g; 
   $_[0] =~ s/\$/\"/g;        
}

sub str_modify {
   $_[0] =~ s/\\\"/\$/g;
   $_[0] =~ s/\"//g;
   $_[0] =~ s/\\n//g;
   $_[0] =~ s/%s/\[chars\]/g;
   $_[0] =~ s/%m/\[Linux error\]/g;
   $_[0] =~ s/%[dcu]/\[dec\]/g;
   $_[0] =~ s/0x%[Xx]/\[hex\]/g;
   $_[0] =~ s/%[Xx]/\[hex\]/g;
   $_[0] =~ s/%[0-9\.\-]+s/\[chars\]/g;
   $_[0] =~ s/%[0-9\.\-]+[dcu]/\[dec\]/g;
   $_[0] =~ s/0x%[0-9\.\-]+[xX]/\[hex\]/g;
   $_[0] =~ s/%[0-9\.\-]+[xX]/\[hex\]/g;
   $_[0] =~ s/%%/%/g;
   $_[0] =~ s/\$/\"/g;
}

$data_file="syslog.def";
$tmp1_file="syslog.tmp1";
$tmp2_file="syslog.tmp2";
$htm_file="syslog_def.html";
open(DAT, $data_file) || die("cannot open syslog.def input file");
open(TMP1, ">$tmp1_file") || die("cannot open tmp1 file");

#Start reading the "syslog.def" file line by line.
#The scanned pattern is written into syslog.tmp1 in the following format:
#"fac-name|level|msg|txt|expln|action|DDTS-component\n"
$modname = "";
while ($line = <DAT>)
{
    #Look for the facility_def pattern.
    if ($line =~ /<FAC_DEF>/) {
        $is_fac = 0;
        $txt_start = 0;  

        #Flag an error for modules having nothing other than facility def.
        if (($fac_cnt == 0) && $modname) {
            #printf("No syslog_msgdef() for module: %s\n", $modname);
        }
        ($junk1, $modname, $junk2) = split('"', $line);
        $fac_cnt = 0;
    } elsif ($line =~ /<MSG_DEF>/) {
        $txt_start = 0;  
        ($junk1, $level, $junk2) = split("-", $line);
        ($msg, $junk1, $junk3) = split(" ", $junk2);
         printf(TMP1 "\n%s|NONE|%d|%s|", uc $modname, $level, $msg);

         #only if the log-msg has a facility, level & msg it is considered
         #for generation of the document. All others are ignored.
         $is_fac = 1;
         $fac_cnt++;

         #a MSG_TXT tag doesnt necessarily start in a new line. So we need
         #to handle this as a special case. The presence of this tag is
         #indicated by the $txt_start variable.
         if ($junk3 =~ /<MSG_TEXT>/) {
            if ($line =~ /<\/MSG_TEXT>/) {
                ($mjunk, $mtext1) = split(/<MSG_TEXT>/, $line);
                ($text, $mjunk) = split(/<\/MSG_TEXT>/, $mtext1);
                 &str_modify($text);
                 printf(TMP1 "%s|", $text);
            } else {
                 $txt_start = 1;
            }
         }

         if ($mjunk =~ /<MSG_LIMIT>/) {
             if ($line =~ /<\/MSG_LIMIT>/) {
                 ($ljunk, $ltext1) = split(/<MSG_LIMIT>/, $line);
                 ($limit, $ljunk) = split(/<\/MSG_LIMIT>/, $ltext1);
                 &str_modify($limit);
                 printf(TMP1 "%s|", $limit);
             }
         } else {
             printf(TMP1 " NO_LIMIT |");
         }
         
    } elsif ($line =~ /<MSG_LC_DEF>/) {
        $txt_start = 0;  
        ($junk1, $level, $junk2) = split("-", $line);
        ($msg, $junk1, $junk3) = split(" ", $junk2);
         printf(TMP1 "\n%s|SLOT#|%d|%s|", uc $modname, $level, $msg);

         #only if the log-msg has a facility, level & msg it is considered
         #for generation of the document. All others are ignored.
         $is_fac = 1;
         $fac_cnt++;

         #a MSG_TXT tag doesnt necessarily start in a new line. So we need
         #to handle this as a special case. The presence of this tag is
         #indicated by the $txt_start variable.
         if ($junk3 =~ /<MSG_TEXT>/) {
            if ($line =~ /<\/MSG_TEXT>/) {
                ($mjunk, $mtext1) = split(/<MSG_TEXT>/, $line);
                ($text, $mjunk) = split(/<\/MSG_TEXT>/, $mtext1);
                 &str_modify($text);
                 printf(TMP1 "%s|", $text);
            } else {
                 $txt_start = 1;
            }
         }
    } elsif (($is_fac == 1) && ($line =~ /<MSG_EXPL>/)) {

         #Since the msg-expln can spread into multiple lines, we gather all
         #of them, again looping until the /MSG_EXPL is encountered. Since
         #some modules dont properly close the string within "" when it
         #spills into multiple lines, we need to take care of it.
         $txt_start = 0;  
         ($junk1, $expl1) = split(/<MSG_EXPL>/, $line);
         if ($line =~ /<\/MSG_EXPL>/) {
            ($expl, $junk2) = split(/<\/MSG_EXPL>/, $expl1);
             $expl =~ s/\"//g;
             &str_modify($expl);
             printf(TMP1 "%s|", $expl);
             next;
         }
         $expl1 =~ s/\"//g; 
         $expl1 =~ s/^[ ]//g;           
         chop($expl1);
         &str_modify($expl1);
         push(@explarr, $expl1);
         while ($line = <DAT>) {
             if ($line =~ /<\/MSG_EXPL>/) {
                 ($expl, $junk2) = split(/<\/MSG_EXPL>/, $line);
                 $expl =~ s/\"//g;
                 chop($expl);
                 &str_modify($expl);
                 push(@explarr, $expl);
                 $texpl = join(" ", @explarr);
                 printf(TMP1 "%s|", $texpl);
                 @explarr = ();
                 last;
             } else {
                 $line =~ s/\"//g;
                 $line =~ s/^[ ]//g;
                 chop($line);
                 &str_modify($line);
                 push(@explarr, $line);
             }
         }    
    } elsif (($is_fac == 1) && ($line =~ /<MSG_ACT>/)) {

         #Handling is very similar to MSG_EXPL.
         $txt_start = 0;  
         ($junk1, $act1) = split(/<MSG_ACT>/, $line);
         if ($line =~ /<\/MSG_ACT>/) {
            ($act, $junk2) = split(/<\/MSG_ACT>/, $act1);
             &str_quotes($act);
             printf(TMP1 "%s|", $act);
             next;
         }
         &str_strip($act1);
         chop($act1);
         push(@actarr, $act1);
         while ($line = <DAT>) {
             if ($line =~ /<\/MSG_ACT>/) {
                 ($act, $junk2) = split(/<\/MSG_ACT>/, $line);
                 &str_quotes($act);
                 push(@actarr, $act);
                 $tact = join(" ", @actarr);
                 printf(TMP1 "%s|", $tact);
                 @actarr = ();
                 last;
             } else {
                 &str_strip($line);
                 chop($line);
                 push(@actarr, $line);
             }
         }    
    } elsif (($is_fac == 1) && ($line =~ /<MSG_DDTS>/)) {
        $txt_start = 0;  
        #Get the DDTS component.
        ($junk1, $ddts1) = split(/<MSG_DDTS>/, $line);
        ($ddts, $junk2) = split(/<\/MSG_DDTS>/, $ddts1);
        $ddts =~ s/\"//g;
        $ddts =~ s/^[ ]//g;
        chop($ddts);
        printf(TMP1 "%s\n", $ddts);
    } elsif ($txt_start == 1) {
         $txt_start = 0;
         if ($line =~ /<\/MSG_TEXT>/) {
            ($txt, $junk2) = split(/<\/MSG_TEXT>/, $line);
             &str_modify($txt);
             printf(TMP1 "%s|", $txt);
             next;
         }
         $txt1 = $line;
         &str_strip($txt1);
         chop($txt1);
         push(@txtarr, $txt1);
         while ($line = <DAT>) {
             if ($line =~ /<\/MSG_TEXT>/) {
                 ($txt, $junk2) = split(/<\/MSG_TEXT>/, $line);
                 $txt =~ s/\\\"/\$/g;
                 $txt =~ s/\"//g;
                 chop($txt);
                 push(@txtarr, $txt);
                 $ttxt = join(" ", @txtarr);
                 &str_modify($ttxt);
                 printf(TMP1 "%s|", $ttxt);
                 @txtarr = ();
                 last;
             } else {
                 &str_strip($line);
                 chop($line);
                 push(@txtarr, $line);
             }
         }    
    }
}
close(DAT);
close(TMP1);

#Now sort the contents of syslog.tmp1 into syslog.tmp2.
open(TMP1, $tmp1_file) || die("cannot open tmp1 file");
open(TMP2, ">$tmp2_file") || die("cannot open tmp2 file");
@sortarr = <TMP1>;
@sortarr = sort(@sortarr);
print TMP2 @sortarr;
close(TMP1);
unlink("syslog.tmp1");

#Create the HTML file syslog.html
open(TMP2, $tmp2_file) || die("cannot open tmp2 file");
open(HTM, ">$htm_file") || die("cannot open html file");
print HTM "<html> <head>\n";
print HTM "<title> VQE System Messages and Recovery Procedures </title>\n";
print HTM "</head>\n";
print HTM "<body BGCOLOR=#FFFFFF>\n";
print HTM "<h2>VQE System Messages and Recovery Procedures</h2>\n";
$tocid = 0;
print HTM "<a HREF=\"#INTRO\"><b>Introduction to VQE System Messages </b><br></a><br>";

while ($hline = <TMP2>)
{
    chop($hline);
    ($name, $slot, $level, $msg, $txt, $limit, $expln, $action, $DDTS) = split(/\|/, $hline);
    if ($name && ($level >= 0) && ($level <= 7) && $msg && $txt && $limit && $expln && $action) {
        if ($name ne $modname) {
            $modlevel = "";
            $tocid++;
            if ($tocid == 1) {
                print HTM "<a HREF=\"#$tocid\"><b>$name Messages</b><br></a><ul>\n";
            } else {
                print HTM "</ul><a HREF=\"#$tocid\"><b>$name Messages</b><br></a><ul>\n";
            }
            $modname = $name;
        }
        if ($level ne $modlevel) {
            $tocid++;
			if ($slot !~ /NONE/) {
                print HTM "<a HREF=\"#$tocid\"><span>$name-$slot-$level</span><br></a>\n";
            } else {
                print HTM "<a HREF=\"#$tocid\"><span>$name-$level</span><br></a>\n";
			}
            $modlevel = $level;
        }
    }
}
print HTM "</ul> </ul>";
close(TMP2);


# Define Introduction Text HTML File and insert Intro text in html file.
my $intro_file = "IntroTextAlone.html";
open(INTROFILE, $intro_file) || die("cannot open Intro file $intro_file: $!\n");
print HTM "<hr>";
while ($introline = <INTROFILE>)
{
    print HTM "$introline";
}
print HTM "<hr>";
close(INTROFILE);


$tocid = 0;        
open(TMP2, $tmp2_file) || die("cannot open tmp file");
while ($hline = <TMP2>)
{
    chop($hline);
    ($name, $slot, $level, $msg, $txt, $limit, $expln, $action, $DDTS) = split(/\|/, $hline);
    if ($name && ($level >= 0) && ($level <= 7) && $msg && $txt && $limit && $expln && $action) {
        if ($name ne $modname) {
            $tocid++;
            $modlevel = "";
            print HTM "<h2><a NAME=\"$tocid\"> $name Messages</a></h2>\n";
            print HTM "<p> This section contains the $name messages.</p>\n";
            $modname = $name;
        }
        if ($level ne $modlevel) {
            $tocid++;
            if ($slot !~ /NONE/) {
                print HTM "<h3><a NAME=\"$tocid\"> $name-$slot-$level</a></h3>\n";
            } else {
                print HTM "<h3><a NAME=\"$tocid\"> $name-$level</a></h3>\n";
            }
            $modlevel = $level;
        }
        if ($slot !~ /NONE/) {
            print HTM "<p><b>Error Message&nbsp;</b><tt> $name-$slot-$level-$msg: $txt</tt></p>\n";
		} else {
            print HTM "<p><b>Error Message&nbsp;</b><tt> $name-$level-$msg: $txt</tt></p>\n";
        }

        #Remove leading blanks and capitalize the first charecter in the expln & action.

        #No need to print Rate limit attribute for the message

        $expln =~ s/^\s+//;
        print HTM "<ul><p><b>Explanation&nbsp;</b> \u$expln</p>\n";
        $action =~ s/^\s+//;
        print HTM "<p><b>Recommended Action&nbsp;</b> \u$action</p>\n";

        #No need to print DDTS component for now
        #print HTM "<p><b>DDTS Component&nbsp;</b> $DDTS</p></ul><p>\n";
        print HTM "</ul>\n";
    } else {
        if ($name) {
            printf("ERROR -- Ignoring incomplete syslog message: %s-%s-%s\n", 
                   $name, $level, $msg);
        }
    }
}
print HTM "</body>";
print HTM "</html>";
close(TMP2);
unlink("syslog.tmp2");
close(HTM);
