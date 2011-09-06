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
	3 500  0  7 4   0 0 packet 100
	
	# after 0.25s, send quickly 30 frames of size 200B in the opposite direction
	3 750 0  4 7   0 0 packet size=200 rep=30 T=10
	
	# on time 10s + random 0-1000 ms, send final, big frame; bitrate 54Mbps and no ACK
	10 uniform(0,1000) 0  7 4  54 1 packet size=1500

Comments start with a hash (#). At most 1000 lines is allowed. Functions can be used as values, e.g.
in order to obtain a random start moment (see [FUNCTIONS][]).

Generator lines follow the following syntax:

  *s* *ms* *interface_num* *srcid* *dstid* *rate* *noack?* *command* [*params*...]

Meaning of line columns:

`1.` `s`: start time (integer 0+, seconds)

Time is given in seconds after the whole experiment start moment (which is synchronized across
all nodes).

`2.` `ms`: start time milisecond fraction (integer 0-999, miliseconds)

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

`8.` `command`: line command (see below)

`9.` `params...`: optional command parameters

Parameters can be either given with their names (e.g. `size=1500`), or without. However, in latter
case, strict order must be retained.

## THE packet COMMAND

The command sends a simple frame, optionally repeated with a time period. Command syntax:

  *packet* *size=* *rep=* *T=* *burst=*

  * `size` :
  Frame length, default 100B (integer 100-1500).
  This defines the total frame length in the air, ie. it includes the IEEE 802.11, LLC and
  `iitis-generator` headers. If there is a remaining space, frame payload is filled with line
  definition, and repeated until frame is full.

  * `rep` :
  Number of repetitions, default 1 (integer 1+).

  * `T` :
  Repetition period in miliseconds, default 1000 (integer 1+).

  * `burst` :
  Number of frames to send in each repetition, default 1 (integer 1+).

## THE ttftp COMMAND

The command mimics the TFTP protocol in a simplified manner. The difference is that file transfer is
initiated by the server, and the data rate is fixed, regardless any TFTP ACK frames. Syntax:

  *ttftp* *size=* *rep=* *T=* *rate=* *MB=*

  * `size`,`rep`,`T` :
  See [THE packet COMMAND][].

  * `rate` :
  Data rate, in megabits per second, default 1 (integer 1+).

  * `MB` :
  Virtual file size, in megabytes, i.e. amount of data to send in single request. Value of 0 means
  an infinite buffer. Default 1 (integer 1+).

## FUNCTIONS

Following functions are supported:

  * `const(val)`:
  Generate a constant value of `val`.

  * `uniform(val1, val2)`:
  Generate a random number in range [`val1`, `val2`] according to uniform distribution.

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator(1), iitis-generator-output(5), iitis-generator-conf(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
