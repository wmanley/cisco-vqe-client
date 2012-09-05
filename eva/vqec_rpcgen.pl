#!/usr/bin/perl
# Copyright (c) 2006-2010 by cisco Systems, Inc.
# All rights reserved.


sub Usage
{
    print STDERR "cm_rpcgen.pl  [-in <in-file>] [-common <file>] [-server <file>] [-client <file>]  [-marker <marker>] [-module <name>] \n";
}

my $do_common = 0;
my $do_server = 0;
my $do_client = 0;
my $module_name = "cm";
my $in_file = "";
my $common_file = "";
my $server_file = "";
my $client_file = "";
my @markers;
$markers[0] = "RPC";

while ($#ARGV >= 0) {
    $arg = shift @ARGV;
    if ($arg eq "-common") {
        $do_common = 1;
        $common_file = shift @ARGV;
    } elsif ($arg eq "-in")  {
        $in_file = shift @ARGV;
    } elsif ($arg eq "-server") {
        $do_server = 1;
        $server_file = shift @ARGV;
    } elsif ($arg eq "-client") {
        $do_client = 1;
        $client_file = shift @ARGV;
    } elsif ($arg eq "-marker") {
        $markers[$#markers + 1] = shift @ARGV;
    } elsif ($arg eq "-module") {
        if ($#ARGV >= 0) {
            $module_name = shift @ARGV;
        } else {
            &Usage;
        }
    } else {
        &Usage;
        die "Bad option $arg\n";
    }
}

open IN,'<',$in_file || die;

if ($do_common) {
    open(COMMON,'>',$common_file) || die "cannot open $common_file\n";
}
if ($do_client) {
    open(CLIENT, '>',$client_file) || die;
}
if ($do_server) {
    open(SERVER, '>',$server_file) || die;
}


# strip comments

$/ = undef;
$_ = <IN>; 

s#/\*[^*]*\*+([^/*][^*]*\*+)*/|([^/"']*("[^"\\]*(\\[\d\D][^"\\]*)*"[^/"']*|'[^'\\]*(\\[\d\D][^'\\]*)*'[^/"']*|/+[^*/][^/"']*)*)#$2#g;

# strip preprocessor lines

s/^#.*//mg;

#
# Parse all RPCs into the @rpc array.  

my $remaining = $_;
my $num_rpcs = 0;
my @rpcs;
my $marker_re = '(?:';
my $marker_num;
for ($marker_num=0; 
     $marker_num <= $#markers;
     $marker_num++) {
    if ($marker_num > 0) {
        $marker_re .= '|';
    }
    $marker_re .= $markers[$marker_num];
}

$marker_re .= ')';
print $marker_re."\n";

while ($remaining =~ /((?:$marker_re)[ \t\n][^;]*;)(.*)/s) {
    # split off current function prototype from those remaining in file
    # "current" holds the current prototype being worked on
    # "remaining" holds the remaining prototypes not yet processed
    $remaining = $2;
    $current = $1;

    # search the current prototype for an OUTARRAY() macro and expand it
    # to its arguments, if found
    #
    # NOTE:  this only handles a single OUTARRAY macro per function prototype
    $_ = $1;
    if (/OUTARRAY[\(](([^\)]*))/) {
	$fields = $1;
	@split_fields = split /[ \n\t]*,[ \n\t]*/s,$fields;
	if ($#split_fields ne 1) {
		die "Bad macro input";
 	}
	$array_args = "RPC_OUTARRAY_PTR<". $split_fields[1]. "> ". $split_fields[0]. " *_array,  RPC_OUTARRAY_COUNT uint32_t _count, RPC_OUTARRAY_USED uint32_t *_used";
	s/OUTARRAY[\(](([^\)]*\)))/$array_args/g;
            $current = $_;
    }     

    # search the current prototype for an INARRAY() macro and expand it
    # to its arguments, if found
    #
    # NOTE:  this only handles a single INARRAY macro per function prototype
    if (/INARRAY[\(](([^\)]*))/) {
	$fields = $1;
	@split_fields = split /[ \n\t]*,[ \n\t]*/s,$fields;
	if ($#split_fields ne 1) {
		die "Bad macro input";
 	}
	$array_args = "RPC_INARRAY_PTR<". $split_fields[1]. "> ". $split_fields[0]. " *__array,  RPC_INARRAY_COUNT uint32_t __count";
	s/INARRAY[\(](([^\)]*\)))/$array_args/g;
            $current = $_;
    }     
        
    # decompose the expanded function prototype into its parts and process...
    ($pre_tokens, $arg_list) = split /[\(\)]/s,$current;

    @split_pretokens = split /[ \n\t][ \n\t]*/s,$pre_tokens;
    shift @split_pretokens;
    $function_name = $split_pretokens[$#split_pretokens];
    $return_type = $split_pretokens[0];
    for ($i=1; $i<$#split_pretokens; $i++) {
        $return_type .= " $split_pretokens[$i]";
    }

    @split_args = split /[ \n\t]*,[ \n\t]*/s, $arg_list;
    if ($#split_args == 0 && ($split_args[0] =~ /[ \t\n]*void[ \t\n]*/s)) {
        $#split_args = -1;
    }

    $args = [];
    $arg_dir = 0;    
    for ($i=0; $i <= $#split_args; $i++) {
        $split_args[$i] =~ s/\*/ \* /g;
        @aw = split /[ \n\t][ \n\t]*/s,"x ".$split_args[$i];
        shift @aw;
        $arg_bound = 0;
        $arg_name = $aw[$#aw];
        if ($aw[0] =~ /(RPC_OUTARRAY_PTR<.*>)/s) {
                $arg_dir = $arg_dir | 0x2;
                $arg_class = shift(@aw);

	    $arg_bound = $arg_class;
	    $arg_bound =~ s/.*<//;
	    $arg_bound =~ s/>.*//;

                $arg_class =~ s/<.*>//;
 
        } elsif ($aw[0] =~ /(RPC_INARRAY_PTR<.*>)/s) {
                $arg_dir = $arg_dir | 0x1;
                $arg_class = shift(@aw);

	    $arg_bound = $arg_class;
	    $arg_bound =~ s/.*<//;
	    $arg_bound =~ s/>.*//;

                $arg_class =~ s/<.*>//;
 
        } elsif ($aw[0] =~ /(INV|INR|INR_OPT|RPC_INARRAY_COUNT|RPC_INARRAY_USED)/s) {
                $arg_dir = $arg_dir |  0x1;
                $arg_class = shift(@aw);
        } elsif ($aw[0] =~ /(OUT|OUT_OPT|RPC_OUTARRAY_COUNT|RPC_OUTARRAY_USED)/s) {
                $arg_dir = $arg_dir | 0x2;
                $arg_class = shift(@aw);
        } elsif ($aw[0] =~ /(INOUT)/s) {
                $arg_dir = $arg_dir | 0x3;
                $arg_class = shift(@aw);
        } else {
            $arg_class = "NONE";
        }
        $arg_type = $aw[0];
        while ($#aw > 1) {
            shift @aw;
            $arg_type .= $aw[0];
        }
        $args->[$i] = { CLASS => $arg_class, TYPE => $arg_type, NAME => $arg_name, BOUND => $arg_bound };
    }

    $rpcs[$num_rpcs] = { RET => $return_type, NAME => $function_name, , DIR => $arg_dir, ARGS => $args, LASTARG => $#split_args};
    $num_rpcs++;
}


# Dump out the enum for the different RPCs

sub mk_rpc_id
{
    my $fname = shift;
    return "__" . uc($module_name) . "_RPC_ID_" . uc($fname);
}

sub mk_rpc_enum_name
{
    return "__" . $module_name . "_rpc_id_";
}

sub mk_req_struct_name
{
    my $fname = shift;
    return "__" . $module_name . "_" . $fname . "_req_";
}

sub mk_rsp_struct_name
{
    my $fname = shift;
    return "__" . $module_name  . "_" . $fname . "_rsp_";
}


if ($do_common) {
    print COMMON "/* THIS IS GENERATED CODE.  DO NOT EDIT. */\n\n";
    print COMMON "#define VQEC_DEV_IPC_BUF_LEN 4096\n";
    print COMMON "#define VQEC_DEV_IPC_BUF_REQ_OFFSET 0\n";
    print COMMON "#define VQEC_DEV_IPC_BUF_RSP_OFFSET VQEC_DEV_IPC_BUF_LEN\n";
    print COMMON "#define VQEC_DEV_IPC_IOCTL_TYPE 0xCA\n";
    print COMMON "#define VQEC_DEV_IPC_IOCTL_WRITE 0x1\n";
    print COMMON "#define VQEC_DEV_IPC_IOCTL_READ 0x2\n";

    print COMMON  "typedef enum " . &mk_rpc_enum_name . " {\n";
    my $name = &mk_rpc_id("INVALID");
    print COMMON  "     $name = 0,\n";

    for ($i = 0; $i <= $#rpcs; $i++) {
        $name = &mk_rpc_id($rpcs[$i]->{'NAME'});
        print COMMON  "     $name";
        if ($i < $#rpcs) {
            print COMMON  ",\n";
        } else  {
            print COMMON  ", " . &mk_rpc_id("MAX") . "\n";
        }
    }
    print COMMON  "} " . &mk_rpc_enum_name . "t;\n\n";
    
    print COMMON "#define " . &mk_rpc_enum_name . "strings__ \\\n \"INVALID\", \\\n";
    for ($i = 0; $i <= $#rpcs; $i++) {
        $name = $rpcs[$i]->{'NAME'};
        print COMMON  "     \"$name\"";
        if ($i < $#rpcs) {
            print COMMON  ", \\\n";
        } else  {
            print COMMON  "\n\n";
        }
    }


# print COMMON  out request and response structure defs



    for ($i=0; $i<$num_rpcs; $i++) {
        my $ret_type = $rpcs[$i]->{'RET'};
        my $fcn_name = $rpcs[$i]->{'NAME'};
        my $num_args = $rpcs[$i]->{'LASTARG'} + 1;
        my $args_ref = $rpcs[$i]->{'ARGS'};
        my @args = @{$args_ref};
        my $req_struct_name =  &mk_req_struct_name($fcn_name);
        my $rsp_struct_name = &mk_rsp_struct_name($fcn_name);
        
        # the request structure

        print COMMON  "typedef struct $req_struct_name {\n";
        print COMMON  "     int32_t __rpc_req_len;\n";
        print COMMON  "     " . &mk_rpc_enum_name . "t __rpc_fcn_num;\n";
        print COMMON  "     uint32_t __rpc_ver_num;\n";

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
	my $maxbound = $args[$j]->{'BOUND'};       
            if ($class eq "INV") {
                print COMMON  "     $type $name ;\n";
	} elsif ($class eq "RPC_INARRAY_COUNT") {
                print COMMON  "     $type $name ;\n";
	} elsif ($class eq "RPC_OUTARRAY_COUNT") {
                print COMMON  "     $type $name ;\n";
            } elsif ($class eq "INR" || $class eq "INOUT") {
                print COMMON  "     __typeof__ (*(($type)0)) $name ;\n";
            } elsif ($class eq "INR_OPT") {
                print COMMON  "     boolean $name" . "_valid;\n";
                print COMMON  "     __typeof__ (*(($type)0)) $name ;\n";
            } elsif ($class eq "RPC_INARRAY_PTR") {
                print COMMON  "     __typeof__ (*(($type)0)) $name\[$maxbound\];\n";
            }            
        }
        print COMMON  "} " . $req_struct_name . "t;\n";

        # the resp structure

        print COMMON  "typedef struct $rsp_struct_name {\n";
        print COMMON  "     int32_t __rpc_rsp_len;\n";
        print COMMON  "     " . &mk_rpc_enum_name . "t __rpc_fcn_num;\n";
        print COMMON  "     uint32_t __rpc_ver_num;\n";
        print COMMON  "     $ret_type __ret_val;\n";

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
	my $maxbound = $args[$j]->{'BOUND'};       
            if ($class eq "OUT" || $class eq "OUT_OPT" || $class eq "INOUT") {
                print COMMON  "     __typeof__ (*(($type)0)) $name ;\n";
            } elsif ($class eq "RPC_OUTARRAY_PTR") {
                print COMMON  "     __typeof__ (*(($type)0)) $name\[$maxbound\];\n";
	} elsif ($class eq "RPC_OUTARRAY_USED") {
                print COMMON  "     __typeof__ (*(($type)0)) $name ;\n";
            }
        }

        print COMMON  "} " . $rsp_struct_name . "t;\n";  

    }

    print COMMON  "\ntypedef union __" . $module_name . "_rpc_all_req_ {\n";
    print COMMON  " struct {\n";
    print COMMON  "     int32_t len;\n";
    print COMMON  "     " . &mk_rpc_enum_name . "t fcn_num;\n";
    print COMMON  "     uint32_t vqec_api_ver;\n";
    print COMMON  "} ol;\n";

    for ($i=0; $i<$num_rpcs; $i++) {
        my $fcn_name = $rpcs[$i]->{'NAME'};
        my $req_struct_name =  &mk_req_struct_name($fcn_name);

        print COMMON  $req_struct_name . "t $fcn_name" . "_req;\n";
    }
    print COMMON  "} __" . $module_name . "_rpc_all_req_t;\n\n";

    print COMMON  "\ntypedef union __" . $module_name . "_rpc_all_rsp_ {\n";
    print COMMON  " struct {\n";
    print COMMON  "     int32_t len;\n";
    print COMMON  "     " . &mk_rpc_enum_name . "t fcn_num;\n";
    print COMMON  "     uint32_t vqec_api_ver;\n";
    print COMMON  "} ol;\n";
    for ($i=0; $i<$num_rpcs; $i++) {
        my $fcn_name = $rpcs[$i]->{'NAME'};
        my $rsp_struct_name = &mk_rsp_struct_name($fcn_name);
        
        print COMMON  $rsp_struct_name . "t $fcn_name" . "_rsp;\n";
    }
    print COMMON  "} __" . $module_name . "_rpc_all_rsp_t;\n\n";

    print COMMON  "int32_t $module_name" . "_rpc_server (__" . $module_name . "_rpc_id_t __id, uint32_t req_size, __" . $module_name . 
		"_rpc_all_req_t *req, __" . $module_name . "_rpc_all_rsp_t *rsp, uint32_t *rsp_size);\n";
}

if ($do_client) {
# Produce the client stubs
    print CLIENT "/* THIS IS GENERATED CODE.  DO NOT EDIT. */\n\n";
    for ($i=0; $i<$num_rpcs; $i++) {
        my $ret_type = $rpcs[$i]->{'RET'};
        my $fcn_name = $rpcs[$i]->{'NAME'};
        my $num_args = $rpcs[$i]->{'LASTARG'} + 1;
        my $args_ref = $rpcs[$i]->{'ARGS'};
        my $dir = $rpcs[$i]->{'DIR'};
        my @args = @{$args_ref};
        my $req_struct_name =  &mk_req_struct_name($fcn_name);
        my $rsp_struct_name = &mk_rsp_struct_name($fcn_name);
        my $fcn_num = &mk_rpc_id($fcn_name);

        print CLIENT  "\n$ret_type $fcn_name(\n";
        if ($num_args == 0) {
            print CLIENT  "void";
        } else {
            for ($j=0; $j< $num_args; $j++) {
                my $name = $args[$j]->{'NAME'};
                my $type = $args[$j]->{'TYPE'};
                print CLIENT  "     $type $name";
                if ($j < ($num_args-1)) {
                    print CLIENT  ",\n";
                } 
            }
        }

        print CLIENT  ")\n{\n";

        print CLIENT  "     int32_t rsp_size, result;\n";
        print CLIENT  "     volatile ".$req_struct_name . "t *req = VQEC_RPC_CLIENT_SHM_REQBUF_PTR;\n";
        print CLIENT  "     volatile ".$rsp_struct_name . "t *rsp = VQEC_RPC_CLIENT_SHM_RSPBUF_PTR;\n";

        print CLIENT  "     if ((req == (void *)-1) || (rsp == (void *)-1)) {\n";
        print CLIENT  "         return VQEC_DP_ERR_INTERNAL;\n";
        print CLIENT  "     }\n";

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "INR" || $class eq "INOUT") {
                print CLIENT  "     if (! $name) {\n";
                print CLIENT  "         return VQEC_DP_ERR_INVALIDARGS;\n";
                print CLIENT  "     }\n";
            } 
        }
        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "OUT" ||
                $class eq "RPC_OUTARRAY_PTR" ||
                $class eq "RPC_OUTARRAY_USED" ||
                $class eq "RPC_INARRAY_PTR") {
                print CLIENT  "     if (! $name) {\n";
                print CLIENT  "         return VQEC_DP_ERR_INVALIDARGS;\n";
                print CLIENT  "     }\n";
            } 
        }

        print CLIENT  "     vqec_lock_lock(vqec_ipc_lock);\n";        
        print CLIENT  "     req->__rpc_fcn_num = $fcn_num;\n";
        print CLIENT  "     req->__rpc_req_len = sizeof(*req);\n";
        print CLIENT  "     VQEC_ASSERT(req->__rpc_req_len <= VQEC_DEV_IPC_BUF_LEN);\n";
        print CLIENT  "     req->__rpc_ver_num = VQEC_DP_API_VERSION;\n";

        $inarray_cnt_name = "";
        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "INV") {
                print CLIENT  "     req->$name = $name;\n";
            } elsif ($class eq "INR" || $class eq "INOUT") {
                print CLIENT  "     req->$name = *($name);\n";
            } elsif ($class eq "INR_OPT") {
                print CLIENT  "     if ($name != NULL) {\n";
                print CLIENT  "         req->$name = *($name);\n";
                print CLIENT  "         req->$name" . "_valid = TRUE;\n";
                print CLIENT  "     } else {\n";
                print CLIENT  "         req->$name" . "_valid = FALSE;\n";
                print CLIENT  "     }\n";
            } elsif ($class eq "RPC_OUTARRAY_COUNT") {
                print CLIENT  "     req->$name = $name;\n";
            } elsif ($class eq "RPC_INARRAY_COUNT") {
                print CLIENT  "     req->$name = $name;\n";
	    $inarray_cnt_name = $name;
            } 
        }

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "RPC_INARRAY_PTR") {
                print CLIENT  "     if ((sizeof (*$name) * $inarray_cnt_name) > VQEC_DEV_IPC_BUF_LEN) {\n";
                print CLIENT  "         vqec_lock_unlock(vqec_ipc_lock);\n";
                print CLIENT  "         return (VQEC_DP_ERR_INVALIDARGS);\n";
                print CLIENT  "     } else {\n";
                print CLIENT  "         memcpy(&req->$name, $name, sizeof (*$name) * $inarray_cnt_name);\n";
                print CLIENT  "     }\n";
            } 
        }

        print CLIENT  "     rsp_size = sizeof(*rsp);\n";
        print CLIENT  "     result = RPC_SND_RCV(req,sizeof(*req),rsp,&rsp_size,$dir);\n"; 
        print CLIENT  "     if (result ||  rsp->__rpc_rsp_len != sizeof(*rsp) || rsp_size != sizeof(*rsp) || rsp->__rpc_fcn_num != req->__rpc_fcn_num || rsp->__rpc_ver_num != VQEC_DP_API_VERSION) {\n";
        print CLIENT  "         RPC_BAD_RSP(result,rsp->__rpc_rsp_len, sizeof(*rsp), rsp->__rpc_fcn_num, req->__rpc_fcn_num, rsp->__rpc_ver_num);\n";
        print CLIENT  "     }\n";

        print CLIENT  "     if (!result) {\n";

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "OUT" || $class eq "INOUT") {
                print CLIENT  "         *$name = rsp->$name;\n";
            } elsif ($class eq "OUT_OPT") {
                print CLIENT "        if ($name) {\n";
                print CLIENT "            *$name = rsp->$name;\n";
                print CLIENT "        }\n";
            } elsif ($class eq "RPC_OUTARRAY_PTR") {
	    $array_name = $name;
	    $array_type = $type;
            } elsif ($class eq "RPC_OUTARRAY_COUNT") {
	    $array_cnt_name = $name;
	} elsif ($class eq "RPC_OUTARRAY_USED") { 
                print CLIENT "         VQEC_ASSERT(rsp->$name <= $array_cnt_name);\n";
                print CLIENT "         *$name = rsp->$name;\n";
                print CLIENT "         memcpy($array_name, &rsp->$array_name, sizeof(*$array_name) * rsp->$name);\n";
	}
        }

        print CLIENT  "         vqec_lock_unlock(vqec_ipc_lock);\n";
        print CLIENT  "         return rsp->__ret_val;\n";
        print CLIENT  "     } else {\n";

        for ($j=0; $j< $num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};       
            if ($class eq "OUT" || $class eq "INOUT") {
                print CLIENT "         memset($name, 0, sizeof(*$name));\n";
            } elsif ($class eq "OUT_OPT") {
                print CLIENT "        if ($name) {\n";
                print CLIENT "            memset($name, 0, sizeof(*$name));\n";
                print CLIENT "        }\n";
	} elsif ($class eq "RPC_OUTARRAY_USED") { 
                print CLIENT "         *$name = 0;\n";
            }
        }
        print CLIENT  "         vqec_lock_unlock(vqec_ipc_lock);\n";
        print CLIENT  "         return VQEC_DP_ERR_INTERNAL;\n";
        print CLIENT  "     }\n";

        print CLIENT  "}\n";
    }
}

if ($do_server) {
# Generate the server side 
    print SERVER "/* THIS IS GENERATED CODE.  DO NOT EDIT. */\n\n";

    print SERVER  "static const char * __rpc_fcn_name_strings[] = { " . &mk_rpc_enum_name . "strings__ };\n";

    print SERVER  "int32_t $module_name" . "_rpc_server (__" . $module_name . "_rpc_id_t __id, uint32_t req_size, __" . $module_name . 
		"_rpc_all_req_t *req, __" . $module_name . "_rpc_all_rsp_t *rsp, uint32_t *rsp_size)\n";
    print SERVER  "{\n";
    print SERVER  "     ASSERT(sizeof(*req) <= VQEC_DEV_IPC_BUF_LEN);\n";
    print SERVER  "     ASSERT(sizeof(*rsp) <= VQEC_DEV_IPC_BUF_LEN);\n";
    print SERVER  "     *rsp_size = 0;\n";
    print SERVER  "     if (req_size < sizeof(req->ol) || req_size != req->ol.len || req->ol.fcn_num <= 0 || req->ol.fcn_num >= ".&mk_rpc_id("MAX")." || 
		req->ol.vqec_api_ver != VQEC_DP_API_VERSION || __id != req->ol.fcn_num) {\n";
    print SERVER  "             RPC_REQ_ERROR(__id, req_size, req->ol.len, req->ol.fcn_num, req->ol.vqec_api_ver);\n";
    print SERVER  "             return (-EINVAL);\n";
    print SERVER  "         }\n\n";
    print SERVER  "         RPC_TRACE(__rpc_fcn_name_strings[req->ol.fcn_num]);\n";
    print SERVER  "         switch (req->ol.fcn_num) {\n";

    for ($i=0; $i<$num_rpcs; $i++) { 
        my $ret_type = $rpcs[$i]->{'RET'};
        my $fcn_name = $rpcs[$i]->{'NAME'};
        my $num_args = $rpcs[$i]->{'LASTARG'} + 1;
        my $args_ref = $rpcs[$i]->{'ARGS'};
        my @args = @{$args_ref};
        my $req_struct_name =  &mk_req_struct_name($fcn_name);
        my $rsp_struct_name = &mk_rsp_struct_name($fcn_name);
        my $fcn_num = &mk_rpc_id($fcn_name);

        print SERVER  "          case $fcn_num:\n";
        print SERVER  "          {\n";
        print SERVER  "               $req_struct_name" . "t * req_p = (struct $req_struct_name *) req;\n";
        print SERVER  "               $rsp_struct_name" . "t * rsp_p = (struct $rsp_struct_name *) rsp;\n";
        print SERVER  "               $ret_type ret_val = 0;\n";

        for ($j=0; $j<$num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};  
            if ($class eq "INV"  || $class eq "RPC_INARRAY_COUNT" || $class eq "RPC_OUTARRAY_COUNT") {
                print SERVER  "               $type $name;\n";
            } elsif ($class eq "INR" || $class eq "OUT" || $class eq "INR_OPT" 
                     || $class eq "OUT_OPT" || $class eq "RPC_OUTARRAY_PTR" 
                     || $class eq "RPC_INARRAY_PTR" || $class eq "RPC_OUTARRAY_USED"
                     || $class eq "INOUT") {
                print SERVER  "               __typeof__ ((($type)0)) $name;\n";
            } 
        }

        print SERVER  "               if (req->ol.len != sizeof(*req_p)) {\n";
        print SERVER  "                   RPC_REQ_ERROR(__id, req_size, req->ol.len, req->ol.fcn_num, req->ol.vqec_api_ver);\n";
        print SERVER  "                   return (-EINVAL);\n";
        print SERVER  "               }\n";

        for ($j=0; $j<$num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};  
            if ($class eq "INR" || $class eq "INR_OPT") {
                print SERVER  "               $name = &req_p->$name;\n";
            } elsif ($class eq "RPC_INARRAY_PTR") {
                print SERVER  "               $name = req_p->$name;\n";
            } elsif ($class eq "INV" || $class eq "RPC_INARRAY_COUNT" || $class eq "RPC_OUTARRAY_COUNT") {
                print SERVER  "               $name = req_p->$name;\n";
            } elsif ($class eq "OUT" || $class eq "OUT_OPT" || $class eq "RPC_OUTARRAY_USED") {
                print SERVER  "               $name = &rsp_p->$name;\n";
            } elsif ($class eq "RPC_OUTARRAY_PTR") {
                print SERVER  "               $name = rsp_p->$name;\n";
            } elsif ($class eq "INOUT") {
                print SERVER  "               rsp_p->$name = req_p->$name;\n";
                print SERVER  "               $name = &rsp_p->$name;\n";
            }
        }

        for ($j=0; $j<$num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};  
           	if ($class eq "OUT" || $class eq "OUT_OPT") {
                print SERVER  "               memset($name, 0, sizeof(*$name));\n";
            }
        }

        print SERVER  "               vqec_lock_lock(g_vqec_dp_lock);\n";
        print SERVER  "               ret_val = $fcn_name(";
        for ($j=0; $j<$num_args; $j++) {
            my $class = $args[$j]->{'CLASS'};
            my $name = $args[$j]->{'NAME'};
            my $type = $args[$j]->{'TYPE'};  
            if ($class eq "INV" || $class eq "RPC_INARRAY_COUNT" || $class eq "RPC_OUTARRAY_COUNT") {
                print SERVER  $name;
            } elsif ($class eq "INR" || $class eq "OUT" || $class eq "OUT_OPT" || 
                     $class eq "RPC_OUTARRAY_PTR" || $class eq "RPC_INARRAY_PTR" || 
                     $class eq "RPC_OUTARRAY_USED" || $class eq "INOUT") {
                print SERVER  "$name";
            } elsif ($class eq "INR_OPT") {
                print SERVER  "((req_p->$name"."_valid) ? &req_p->$name : NULL)";
            }
            if ($j < ($num_args - 1)) {
                print SERVER  ",";
            }
        }
        print SERVER  ");\n";
        print SERVER  "               vqec_lock_unlock(g_vqec_dp_lock);\n";

        print SERVER  "               rsp_p->__ret_val = ret_val;\n";
        print SERVER  "               rsp_p->__rpc_rsp_len = sizeof(*rsp_p);\n";
        print SERVER  "               rsp_p->__rpc_fcn_num = req_p->__rpc_fcn_num;\n";
        print SERVER  "               rsp_p->__rpc_ver_num = VQEC_DP_API_VERSION;\n";
        print SERVER  "               *rsp_size = sizeof(*rsp_p);\n";

        print SERVER "          } break;\n\n";
    }

        print SERVER  "          default:\n";
        print SERVER  "          {\n";
        print SERVER  "               RPC_REQ_ERROR(__id, req_size, req->ol.len, req->ol.fcn_num, req->ol.vqec_api_ver);\n";
        print SERVER  "               return (-EINVAL);\n";
        print SERVER  "          } break;\n\n";

    print SERVER  "    }\n";
    print SERVER  "    return (0);\n";
    print SERVER  "}\n";

 

}

#for ($i=0; $i<$num_rpcs; $i++) {
#    print "$rpcs[$i]->{'RET'} xxx $rpcs[$i]->{'NAME'}\n";
#    $max = ${$rpcs[$i]}{'LASTARG'};
#    for ($j=0; 
#         $j <= $max;
#         $j++) {
#        %arg = %{$rpcs[$i]->{"ARGS"}->[$j]};
#        print "\t$arg{'CLASS'} xxx $arg{'TYPE'} xxx $arg{'NAME'}\n";
#    }
#}

