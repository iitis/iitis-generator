iitis-generator(1) -- tool for distributed, statistical evaluation of wireless networks
=======================================================================================

## SYNOPSIS

`iitis-generator` [<OPTIONS>...] <TRAFFIC FILE> [<CONFIG FILE>]

## INTRODUCTION

This tool provides partial, limited solution to the problem formulated by the question: given a
computer network, how will it perform under traffic work load? The network may consist of many nodes
and we want detailed statistical data for each node.

Motivations behind this fundamental question are - for instance - the need to assess service quality
and the need to predict the performance of the network. There exist an approach to solve this
problem by means of digital modelling and simulation. The `iitis-generator` program, however,
is a supportive tool for alternative approach, complementary to network simulation.

In this approach, real traffic is generated in a physical network, and results of such experiment
may be used to answer the question on network performance.

## DESCRIPTION

`iitis-generator` is run parallely on many network nodes at the same time. The nodes must be
connected with an IP network used as a *service network*, and any numbers of other networks to be
put under test.

`iitis-generator` basically does the following work on a given network node:

1. read traffic characteristics from <TRAFFIC FILE>
1. wait for all nodes to prepare, so a common start moment is reached
1. generate and receive traffic
1. periodically write statistics to disk
1. optionally dump frames to disk, in a pcap format

The program will handle node synchronization on its own, but it requires a reliable system clock.
Typically, running an NTP client on each node is enough to have a 1ms accuracy in a LAN environment.

Traffic is generated at low level, through the packet injection facility of the Linux mac80211
subsystem. Hence, a precise control over wireless frames is possible. Traffic is captured using
wireless interface in *monitor mode*.

As output, a directory with tree-like structure is created. It contains:

* statistics stored in text files,
* copies of <TRAFFIC FILE> and <CONFIG FILE>,
* frame dump files in pcap format, if enabled.

See iitis-generator-output(5) for documentation on the structure and contents of the output
directory.

## OPTIONS

`iitis-generator` accepts following options.

  * `--id`=<num>:
  ID number of this node; by default extract it from the hostname, e.g. "router5" gives number 5

  * `--root`=<directory>:
  the **root** directory for program outputs; by default "./out"

  * `--sess`=<name>:
  optional session name; it will be used as a prefix for program output directory

  * `--world` :
  make all generated files and directories readable and writable by everyone

  * `--verbose`,`-V`:
  be verbose; alias for `--debug=5`

  * `--debug`=<num>:
  set debugging level

  * `--help`,`-h`:
  display short help screen and exit

  * `--version`,`-v`:
  display version and copying information

Most options can be overriden in the <CONFIG FILE>, which is applied after command line options (see
[CONFIG FILE][]).

## CONFIG FILE

The <CONFIG FILE> is recommended to be common for the whole network. Exemplary config file below.

	# write statistics each 3 seconds
	stats = 1

	# commit disk writes each 10 seconds (if NFS, sends data to the server)
	sync = 10

	# on nodes 1-3 and 8
	"1-3,8" = {
		# dump raw frames in pcap format
		dump = 1
	}

See iitis-generator-conf(5) for further documentation.

## TRAFFIC FILE

The <TRAFFIC FILE> is common for the whole network. Exemplary few lines below.

	# on time 3.5s, on interface 0, generate a 100B frame from node 7->4
	3.5  0  7 4   0 0 packet 100
	
	# after 0.25s, send 30 frames of size 200B in the opposite direction
	3.75 0  4 7   0 0 packet 200 30
	
	# on time 10.0s, send final, big 7->4 frame with bitrate 54Mbps and no ACK
	10.0 0  7 4  54 1 packet 1 1500

See iitis-generator-traffic(5) for further documentation.

## DIAGNOSTICS AND RETURN VALUES

`iitis-generator` work can be monitored through the debugging messages facility (the `--debug`
option). Messages will be printed to the standard error output, as in example below:

	mgi_init(): bound to mon0
	mgi_init(): bound to mon1
	mgi_init(): bound to mon2
	_master_read(): received time sync ACK from node 2: 1311683491
	_master_read(): received all ACKs
	mgstats_start(): storing statistics in ./stats/2011.07/tests/2011.07.26-14:31:31/1
	main(): Starting
	heartbeat(): Finished, exiting...

Notice the message prefix with information on the program internals. Level of detail of this
information increases with the debug level. In example above it's name of the C function which
generated the message.

Possible program return values and their meanings:

  * `0`:
  everything completed successfully
  * `1`:
  invalid command line arguments
  * `2`:
  no available interfaces found
  * `3`:
  invalid <TRAFFIC FILE> provided
  * `4`:
  invalid <CONFIG FILE> provided
  * `134`:
  see below
  * `139`:
  segmentation fault

`iitis-generator` may also abnormally abort execution with return value 134 due to unexpected
circumstances. However, the user will be notified about detailed source code file name and line
number that caused such behaviour. Such information will be in the last line printed to the standard
error output of the program. This may be used for further tracing of the root error cause.

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator-output(5), iitis-generator-conf(5), iitis-generator-traffic(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
