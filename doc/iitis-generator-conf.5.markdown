iitis-generator-conf(5) -- config file of iitis-generator
=========================================================

## SYNOPSIS

generator.conf

## DESCRIPTION

iitis-generator(1) is a tool for distributed, statistical evaluation of wireless networks. Each
network node that takes part in an evaluation experiment runs the `iitis-generator` program. This
manual page documents its configuration file, `generator.conf`.

## FILE FORMAT

The file uses the [JSON](http://www.json.org/) format and can accept a simplified syntax, easier to
read and write by a human operator. It might be observed on the example below.

	# write statistics each 3 seconds
	stats = 3

	# commit disk writes each 10 seconds (if NFS, sends data to the server)
	sync = 10

	# on nodes 1-3 and 8
	"1-3,8" = {
		# dump raw frames in a pcap format
		dump = "yes"
	}

Notice the following differences vs. JSON:

* a variable name needs an apostrophe only if it starts with a digit,
* an equal sign (=) may be used instead of a colon (:) before variable value,
* commas after the value may be skipped,
* comments start with a hash (#).

It is recommended that all network nodes use the same configuration file. In such situation, the
variables put in the root level (ie. those not surrounded by curly brackets) will be interpreted and
used by all nodes.

However, variables put in a hash (curly braces) will be used only if the key name includes the ID of
the node interpreting the configuration file. Syntax of such a key name lets for specifying just a
single node (e.g. "1"), a range of nodes (e.g. "1-3"), a list of nodes (e.g. "1,2,3") and a
combination (e.g. "1-3,8" means nodes 1, 2, 3 and 8). For instance, in the example above, if the
node ID is 2, the `iitis-generator` will dump raw frames to disk. Hashes can be nested.

## CONFIGURATION OPTIONS

Following options can be configured:

  * `id`=*int*: node ID

	ID number of this node. By default it will be extracted from the system hostname (see
	iitis-generator(1)).

  * `stats`=*int*: statistics write period

	Defines how often to generate statistics and write them to disk (new lines in statistics files).
	Notice that after such event all statistics of a *counter* type will be set back to zero.

	Set `stats` to 0 if you wish to disable statistics. This will also disable any file-system
	output of the program.

  * `sync`=*int*: [sync(2)](http://linux.die.net/man/2/sync) period

	This option is mainly relevant if you store `iitis-generator` output on a network drive. For
	instance, this can force the node operating system to send statistics to an NFS server each few
	seconds.

  * `root`=*string*: the *root* directory for program outputs

	As the output of `iitis-generator` a tree-like directory structure is generated (see
	iitis-generator-output(5) for more info). This option defines the path to root directory under
	which the directory tree will be started.

  * `session`=*string*: session name

	If this option is specified, it will be used as a prefix for the output directory name.

  * `dump`=*bool*: dump raw frames to disk

	Enable this option to dump all incoming and outgoing frames, except WiFi beacons. A PCAP file
	format is used and resultant files can be found in the directory with interface statistics (see
	iitis-generator-output(5)).

  * `dump-size`=*int*: limit size of dumped frames

	By default, whole frames will be dumped. Use this option to limit the size in bytes of the
	dumped frames. A meaningful minimum of this option is about 100.

  * `dump-beacons`=*bool*: include beacons in dumped frames

	Dont skip WiFi beacons in frame dumps. Notice that beacons can be generated about 10 times per
	second by each node. However, they may be useful in order to detect other wireless networks
	operating on the same channel, thus possibly disturbing the experiment.

  * `svc-ifname`=*string*: service network interface name

	Choose the interface connected to the service network. Default: "eth0".

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator(1), iitis-generator-output(5), iitis-generator-traffic(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
