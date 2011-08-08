iitis-generator-traffic(5) -- iitis-generator traffic description file
======================================================================

## SYNOPSIS

traffic.txt

## DESCRIPTION

iitis-generator(1) is a tool for distributed, statistical evaluation of wireless networks. Each
network node that takes part in an evaluation experiment runs the `iitis-generator` program. This
manual page documents format of the file describing the traffic to be generated in test networkss.

## FILE FORMAT

The traffic file is a text file. Each line is a statement and can be treated as an independent
traffic generator. The file is common for the whole network. Exemplary few lines below.

	# on time 3.5s, on interface 0, generate a 100B frame from node 7->4
	3.5  0  7 4   0 0 packet 100
	
	# after 0.25s, send 30 frames of size 200B in the opposite direction
	3.75 0  4 7   0 0 packet 200 30
	
	# on time 10.0s, send final, big 7->4 frame with bitrate 54Mbps and no ACK
	10.0 0  7 4  54 1 packet 1 1500

Comments start with a hash (#). Generator lines follow the same syntax:

  *time_s*.*ms* *interface_num* *srcid* *dstid* *rate* *noack?* *command* [*params*...]

Meaning of line columns:

`1.` `time_s`: start time (integer 0+, s)

Time is given in seconds after the whole experiment start moment (which is synchronized across
all nodes).

`2.` `ms`: start time milisecond fraction (integer 0-999, ms)

`3.` `interace_num`: interface number (integer 0+)

Decides on which interface the traffic should be generated. Numbering starts at 0, so first
interface is 0, second is 1, and so on.

`4.` `srcid`: source node ID (integer 1+)

Defines which node should generate the traffic.

`5.` `dstid`: destination node ID (integer 1+)

Defines which node should receive the traffic.

`6.` `rate`: wireless bitrate (float value, Mbps)

PHY layer speed to send the frames with. Notice that this may disable WiFi retry mechanism (can be
verified using frame dump files).

There is only a limited set of possible values for this option. Run `iw phy` and in the
"Bitrates" section check if the value you want to use is valid. However, usually:

  * for the 2.4 GHz band, supported bitrates are: 1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54
  * for the 5 GHz band, these are: 6, 9, 12, 18, 24, 36, 48, 54

Use 0 for automatic selection by the driver.

`7.` `noack?`: disable WiFi ACK mechanism (bool)

Use either value 0 (enable ACK) or 1 (disable ACK).

`8.` `command`: line command (see [COMMANDS][])

`9.` `params...`: optional command parameters

Traffic file can have 1000 lines at most.

## COMMANDS

Line command chooses characteristics of the traffic that will be generated with parameters chosen in
line specification, before the command part.

Currently, only one command is supported - `packet` - which sends a simple frame, optionally
repeated with a fixed time period. Command syntax:

  *packet* [*len* [*num* [*T*]]]

`1.` `len`: frame length, default 100B (integer 100-1500)

This defines the total frame length in the air, ie. it includes the IEEE 802.11, LLC and
iitis-generator headers. If there is a remaining space, frame payload is filled with line definition,
and repeated until frame is full.

`2.` `num`: number of repetitions, default 1 (integer 1+)

`3.` `T`: repetition period, default 1000ms (integer 1+)

If `num` is greater than 1, this defines the time gap between frame repetitions.

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator(1), iitis-generator-output(5), iitis-generator-conf(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
