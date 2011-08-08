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
is a supportive tool for an alternative approach, complementary to network simulation.

In this approach, real traffic is generated in a physical network and statistical data is collected.
Results of such experiment can be used for further search for the answer to the question on network
performance.

## DESCRIPTION

`iitis-generator` is basically a traffic generator. It is run on many network hosts at the same
time. Each host is called a *node*. Each node may be connected to many *test networks* - on which
traffic is to be generated - and also must be connected to one IP *service network*.

Basically `iitis-generator` does the following:

1. parse traffic characteristics in <TRAFFIC FILE>
1. negotiate a common experiment start moment using the service network
1. generate and receive traffic on test networks
1. periodically write statistics to disk

As the output, a directory with a tree-like structure is created. It contains the resultant network
statistics, amongst the others (see [OUTPUT][]).

`iitis-generator` is written in C and designed to be run on embedded systems, especially MikroTik
RouterBoards running [OpenWrt](http://www.openwrt.org/).

## NETWORK ENVIRONMENT

The test traffic is generated using the low level packet injection facility of the Linux mac80211
subsystem. Hence, a precise control over wireless frames is possible. Traffic capture is realized
in similar way.

In order to make a wireless interface available to be used by `iitis-generator`, it must be put into
the *monitor mode* and named like "mon\*", where \* is a number 0-7, e.g. "mon0". This interface number
is also used in [the traffic file][iitis-generator-traffic(5)].

Frames generated in test networks have 3 headers:

1. IEEE 802.11 header (24B + 4B FCS)
1. LLC header, encapsulated Ethernet (8B)
1. `iitis-generator` header (20B)

The service network is realized by:

1. sending UDP broadcast (255.255.255.255) frames on port 31337
1. replying with UDP unicast frames

By default, the "eth0" interface is used. This can be changed using the "svc-ifname" option (see
iitis-generator-conf(5)). Currently, the service network is used only for node synchronization.

## NODE SYNCHRONIZATION

Definitions in the traffic file use relative time moments. In order to synchronize actions being
taken by nodes, a certain moment of time needs to be chosen as the *time origin* ("origin" for
short), common for all nodes. This moment is chosen on the wall clock. Such approach has an
advantage of opening a possibility for an external program working on wall clock synchronization
parallely to `iitis-generator`.

Node synchronization is realized during startup, in a master-slave manner. The node with the lowest
ID is chosen as the *master*, and all other nodes become *slaves*. Master will periodically propose
some origin a few seconds ahead the wall clock, until all slaves reply with a positive
acknowledgement.

A reliable wall clock source is required on all nodes. Running NTP on each node is enough to have a
1 ms accuracy in a typical LAN environment.

## OPTIONS

`iitis-generator` accepts following options.

  * `--id`=<num>:
  ID number of this node; by default it will be extracted from the hostname, e.g. "router5" gives number 5

  * `--root`=<directory>:
  the *root* directory for the program output; by default "./out"

  * `--sess`=<name>:
  optional session name

  * `--world` :
  make all generated files and directories readable and writable by anyone

  * `--verbose`,`-V`:
  be verbose; alias for `--debug=5`

  * `--debug`=<num>:
  set debugging level

  * `--help`,`-h`:
  display short help screen and exit

  * `--version`,`-v`:
  display version and copying information

Most options can be overriden in the <CONFIG FILE>, which is applied after command line options.

## CONFIG FILE

The <CONFIG FILE> is recommended to be common for the whole network. Exemplary config file below.

	# write statistics each 3 seconds
	stats = 3

	# commit disk writes each 10 seconds (if NFS, sends data to the server)
	sync = 10

	# on nodes 1-3 and 8
	"1-3,8" = {
		# dump raw frames in a pcap format
		dump = "yes"
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

## OUTPUT

Exemplary output structure:

	2011.07/
	*-- session-name/
	    *--- 2011.07.26-15:40:25/
	        *-- 1/
	        |   *-- internal-stats.txt
	        |   *-- linestats.txt
	        |   *-- mon0/
	        |   |   *-- dump.pcap
	        |   |   *-- interface.txt
	        |   |   *-- link-2->1.txt
	        *-- 2/
	            *-- internal-stats.txt
	            *-- linestats.txt
	            *-- mon0/
	                *-- interface.txt

See iitis-generator-output(5) for further documentation.

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
  aborted, see below
  * `139`:
  segmentation fault

`iitis-generator` may abnormally abort execution with return value 134 due to unexpected
circumstances. However, the user will be notified about detailed source code file name and line
number that caused such behaviour. Such information will be in the last line printed to the standard
error output of the program. This may be used for further tracing of the root error cause.

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator-output(5), iitis-generator-conf(5), iitis-generator-traffic(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
