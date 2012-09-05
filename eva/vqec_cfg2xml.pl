#!/usr/bin/perl

#********************************************************************
#
# Copyright (c) 2008 by cisco Systems, Inc.
# All rights reserved.
#
# This perl module takes vqec_cfg_settings.h, and a map file defining
# the association of groups to vqec attribute configuration files as input
# and generates a vqec xml instance file that can then be used as input
# to the VCDS server.
#
#*********************************************************************/


sub Usage
{
    $help =
      "\n vqec_cfg2xml.pl  -cfg <cfg-file> -cfg2group <cfg2group-file> -out <out-file>

       (a) <cfg-file> is the input configuration parameters file: supply 
           eva/vqec_cfg_settings.h here.
       (b) <cfg2group-file> is a map file that defines the association between 
           groups and vqec attribute configuration files. The association is 
           specified as \"group:file\", one per line. For example, the file may
           have 2 lines:
               1:vqec0.cfg
               2:vqec1.cfg
           which means that group 1 will have attributes taken from vqec0.cfg,
           and group 2 will have attributes taken from vqec1.cfg.
       (c) <out-file> is the output xml file to which the generated xml instance
           is written.

       The generation of attributes will fail if any parameter in the 
       configuration files given in the map file is not a recognizable attribute.

         example: 
                    vqec_cfg2xml.pl -in vqec_cfg_settings.h 
                                   -cfg2group groups.map
                                   -out instance.xml\n\n";
  print STDERR $help;
}

#
# schema version - this corresponds only to the syntax of the schema
# and not to the attributes version.
#
my $schema_version = 1;

my $cfg_file = "";
my $cfg2group_file = "";
my $out_file = "";
my $attributes_version = 0;
my $attributes_major_version = 0;
my $in_post_comment;
my %namespace_id_list;

while ($#ARGV >= 0) {
    $arg = shift @ARGV;
    if ($arg eq "-cfg2group")  {
      $cfg2group_file = shift @ARGV;    
    } elsif ($arg eq "-cfg") {
      $cfg_file = shift @ARGV;    
    } elsif ($arg eq "-out") {
      $out_file = shift @ARGV;    
    } else {
        &Usage;
        die "Bad option $arg\n";
    }
}

open (CFG,'<',$cfg_file) || die &Usage;
open (OUT,'>',$out_file) || die &Usage;
open (CFG2GROUP,'<',$cfg2group_file) || die &Usage;

# strip comments from configuration settings file

$/ = undef;
$_ = <CFG>; 

s#/\*[^*]*\*+([^/*][^*]*\*+)*/|([^/"']*("[^"\\]*(\\[\d\D][^"\\]*)*"[^/"']*|'[^'\\]*(\\[\d\D][^'\\]*)*'[^/"']*|/+[^*/][^/"']*)*)#$2#g;

#
# get "attribute-set" version
#
$in_post_comment = $_;
s/(.|\s)*(VQEC_ATTRIBUTES_VERSION\s*)((\w+)(\.\d+))((.|\s)*)/\4/;
$attributes_version = $_;
s/(\w+)\..*/\1/;
$attributes_major_version = $_;
$_ = $in_post_comment;
s/(.|\s)*($attributes_major_version)( |\t)+(\d+)(.|\s)*/\4/;
$attributes_major_version = $_;
$_ = $in_post_comment;
s/(.|\s)*(VQEC_ATTRIBUTES_VERSION\s*)((\w+)(\.)(\d+))((.|\s)*)/\6/;
$attributes_version = $_;
s/(\d+)\..*/\1/;
$attributes_minor_version = $_;
$_ = $in_post_comment;

# strip preprocessor lines from configuration settings file
s/\s*\\\s*//g;
s/^#.*//mg;


my @parameters;
my @initializer;
my %all_attribute_names;
@array_elems = split /ARR\_BEGIN/;
$_ = $array_elems[$#array_elems];
@array_elems = split /ARR\_ELEM\(/;
for ($i = 1; $i < $#array_elems; $i++) {

  #
  # remove end-of-str ")"
  #

  $_ = $array_elems[$i];
  s/(\s*)(.*)(\)[\s]*$)/\2/;
  $attribs = $_;

  #
  # get constructor arguments
  #
  $_ = $attribs;
  s/((\s|.)*)(VQEC_[\w]*_CONSTRUCTOR\()((.|\s)*)(\)[^\)]*)((\s|.)*)/\4/;
  $constructor_args = $_;

  #
  # get constructor type
  #
  $_ = $attribs;
  s/((\s|.)*)(VQEC_[\w]*_CONSTRUCTOR\()((.|\s)*)(\)[^\)]*)((\s|.)*)/\3/;
  $constructor_type = $_;

  #
  # remove constructor str
  # 
  $_ = $attribs;
  s/(VQEC_[\w]*_CONSTRUCTOR\((.|\s)*\))([^\)]*)/\3/;
  $attribs = $_;

  #
  # parse attributes
  #
  $_ = $attribs;
  @fields = split /,/;

  $_ = $fields[0];
  s/(\s*")((\w|\.)+)("\s*)/\2/;
  $name = $_;

  $_ = $fields[1];
  s/(\s*)(\w+)(\s*)/\2/;
  $enum = $_;

  $_ = $fields[2];
  s/(\s*)(\w+)(\s*)/\2/;
  $type = $_;

  $_ = $fields[3];
  s/(^\s*)((\w|\s)*)(\s*)/\2/;
  s/"//g;
  s/(\S*)(\s*)/\1 /g;
  s/\s*$//;
  $docum = $_;


  $_ = $fields[4];
  s/(\s*)(\w+)(\s*)/\2/;
  $is_attrib = $_;

  $_ = $fields[6];
  s/(\s*)(\w+)(\s*)/\2/;
  $update_type = $_;

  $_ = $fields[7];
  s/(\s*)(\w+)(\s*)/\2/;
  $namespace_id = $_;

  #
  # find value for namespace id
  #
  $namespace_id_val = 0;
  if (!exists($namespace_id_list{$namespace_id})) {
    $_ = $in_post_comment;
    s/^(.|\s)*($namespace_id)( |\t)+(\d+)(.|\s)*/\4/;
    $namespace_id_val = $_;
    $namespace_id_list{$namespace_id} = $namespace_id_val;
  } else {
    $namespace_id_val = $namespace_id_list{$namespace_id};
  }

  #
  # parse constructor type and constructor args
  #
  if ($constructor_type =~ /^(VQEC_UINT32_CONSTRUCTOR\()/ ) {
    $ctype = "UI32";
  } elsif ($constructor_type =~ /^(VQEC_UINT32_RANGE_CONSTRUCTOR\()/ ) {
    $ctype = "UI32_RANGE";
  } elsif ($constructor_type =~ /^(VQEC_BOOL_CONSTRUCTOR\()/ ) {
    $ctype = "BOOL";
  } elsif ($constructor_type =~ /^(VQEC_STRING_CONSTRUCTOR\()/ ) {
    $ctype = "STR";
  } elsif ($constructor_type =~ /^(VQEC_STRING_LIST_CONSTRUCTOR\()/ ) {
    $ctype = "LIST";
  }

  $initializer = {TYPE => $ctype, ARGS => $constructor_args};

  #
  # save all data in a hash table
  #
  $parameters[$i] = {NAME => $name, 
		     ENUM => $enum, 
		     TYPE => $type, 
		     DOCSTR => $docum, 
		     ISATTR => $is_attrib, 
		     UPDTYPE => $update_type,
		     NAMESPACEID => $namespace_id_val,
		     INITIALIZER => $initializer};
  if ($is_attrib eq "TRUE") {
    $all_attribute_names{$name} = $name;
  }
}

my @groups;
my %attributes_present;

$/ = undef;
$_ = <CFG2GROUP>; 
@groups = split /\n/;

$xmlhead = 
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
     <GroupFile xmlns=\"http://www.cisco.com/vqe/vqec-syscfg1.0\"
      xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"
      xsi:schemaLocation='http://www.cisco.com/vqe/vqec-syscfg1.0 grp_cfg_schema.xsd'
      version=\"$schema_version.$attributes_major_version.$attributes_minor_version\">\n\n";
  $warn = 
    "\n\t<!-- THIS XML IS AUTO-GENERATED FROM vqec_cfg2xml. -->\n\n";

print OUT $xmlhead;
print OUT $warn;

for ($i = 0; $i <= $#groups; $i++) {
  $_ = $groups[$i];
  s/\s*(\S*)\s*/\1/;
  if (length $_ != 0) {
    @mapped_file = split /:/;
    $group_id = $mapped_file[0];
    $_ = $group_id;
    s/\s*$//;
    $group_id = $_;
    $attrib_file = $mapped_file[1];
    $_ = $attrib_file;
    s/\s*$//;
    $attrib_file = $_;

    # open attributes file
    open(ATTRIBFILE, '<', $attrib_file) || die "cannot open file".$attrib_file."\n";

    # strip comments
    $/ = undef;
    $_ = <ATTRIBFILE>; 
    s#/\*[^*]*\*+([^/*][^*]*\*+)*/|([^/"']*("[^"\\]*(\\[\d\D][^"\\]*)*"[^/"']*|'[^'\\]*(\\[\d\D][^'\\]*)*'[^/"']*|/+[^*/][^/"']*)*)#$2#g;

    # strip preprocessor lines
    s/\s*\\\s*//g;
    s/^#.*//mg;

    $stripped_attribute_file = $_;

    # search for {..} enclosed sub-strings (single 2-level element) and remove them
    s/{[^}]*}\s*;//g;

    # remove empty lines
    s/^(\s)*\n//mg;

    # parse name tokens 
    @attributes = split /;\s*/;

    # if a name token has enclosed { ... } parameter block then the name token 
    # has to be "." appended.
    $num_attribs = $#attributes;
    for ($j = 0; $j <= $num_attribs; $j++) {
      $_ = $attributes[$j];
      @cline = split /=\s*/;
      $_ = $cline[0];
      s/\s*$//;
      $cline[0] = $_;
      
      # get the arguments for the command if they are {...} enclosed.
      if ($stripped_attribute_file =~ /(.|\s)*($cline[0]\s*=\s*{)/) {
	$_ = $stripped_attribute_file;
	s/(.|\s)*(($cline[0])\s*=\s*{[^}]*})(.|\s)*/\2/;
	s/.*=\s*{([^}]*)}.*/\1/;
	@dot_args = split /;\s*/;

	for ($k = 0; $k <= $#dot_args; $k++) {	  
	  $_ = $dot_args[$k];
	  @cmd_args = split /=\s*/;
	  $_ = $cmd_args[0];
	  s/\s*(\S*)\s*/\1/;
	  $cmd = $cline[0].".".$_;
	  $_ = $cmd_args[1];
	  s/\s*(\S*)\s*/\1/;
	  $cmd_data = $_;
	  $attributes[$j] = "";
	  $attributes[$#attributes+1] = $cmd." = ".$cmd_data."\n";
	}
      }
    }

    #
    # search for name tokens in hash-table
    # if not found die
    #
    for ($j = 0; $j <= $#attributes; $j++) {
      $_ = $attributes[$j];
      @cline = split /=\s*/;
      $_ = $cline[0];
      s/\s*$//;
      $cline[0] = $_;
      $_ = $cline[1];
      s/\s*$//;
      $cline[1] = $_;
      if (length $cline[0] != 0) {
	if (!(exists $all_attribute_names{$cline[0]})) {
	  die "Unknown attribute or syntax error \"".$cline[0]."\"\n";
	} else {
	  $attributes_present{($cline[0])} = $cline[1];
	}
      }
    }
  }

  # walk all attributes in order and add the ones present
  print OUT "\t<GroupAttribs group=\"".$group_id."\">\n";

  $ver = 1;
 __loop:
  for ($k = 1; $k <= $#parameters; $k++) {
    $name = $parameters[$k]->{'NAME'};
    $is_attr = $parameters[$k]->{'ISATTR'};
    $type = $parameters[$k]->{'TYPE'};
    $namespace_val = $parameters[$k]->{'NAMESPACEID'};
    
    if ($is_attr eq "TRUE") {
      if (exists $attributes_present{$name}) {
	if ($namespace_val == $ver) {
	  if ($type eq "VQEC_TYPE_BOOLEAN") {
	    $_ = $attributes_present{$name};
	    s/"(.*)"/\1/;
	    $attributes_present{$name} = $_;
	    print OUT "\t\t<".$name.">".$attributes_present{$name}."</".$name.">\n";
	  } else {
	    print OUT "\t\t<".$name.">".(lc $attributes_present{$name})."</".$name.">\n";
	  }
	}
      }
    }
  }

  if ($ver > 1) {
    print OUT "\t\t<extension/>\n";
  }
  if ($ver < $attributes_major_version) {
    $ver++;
    print OUT "\t\t<extension>\n";
    goto __loop;
  }
  print OUT "\t</GroupAttribs>\n\n";
}

print OUT "</GroupFile>\n";



