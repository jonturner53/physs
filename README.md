This repository contains code for Mote Marine Research Laboratory's
Programmable Hyperspectral Seawater Scanner (physs).

There are several major parts to the overall collection of software.

    dataCollector - contains a C++ program that runs on a fizz and
			performs automated data collection; specifically,
            it obtains seawater samples according to a user-supplied
            script, acquires optical spectra for these sample
            and stores them in an output file; it can be controlled
            from a remote console program; this directory also contains
			a basic hardware verification program called basicTest.
    opsConsole - is an web-based command console that provides
            a simple graphic interface for controlling the
            collector; it consists of two components: a server and
			a user interface that runs in a user's browser. Both
			are written in javascript. The server handles some of
			the commands it receives from the user interface by
			itself, and relays others to the data collector.
    analysisConsole - is a web-based console for viewing and analyzing
            data collected by a physs; it consists of two components,
			a server and user interface that runs in a web browser.
            The server can be run either on a single beaglebone,
			or on a cloud server, in which case users can view data
			from all beaglebones that report their data do that server
    xfer    is a data transfer program consisting of a client/server
            pair; the sender component runs on a fizz and
            transfers data produced by the data collector to
            the receiver component, running on a cloud server.
	summarize is a module that is invoked periodically to compute
			or updata summary data files as new raw data is
			transferred to a cloud server; an associated summaryServer
			makes these files available to remote data repositories
			(such as gcoos and secoora).

Supporting code is found in the following sub-directories.

    include     contains all C++ header files
    misc        contains miscellaneous utilities
    hardware    contains low-level components related directly to the
                physs hardware
    collector   contains higher level components of the collector
    cloudServer contains components that are associated with a remote
                cloud server
    startup     contains systemd service files and scripts used to
                perform various configuration tasks when the system
                boots up, and start the collector, opsServer,
                analysisServer and xferSender

Since physs units are usually used in environments where they have only
a dynamically assigned address they are typically configured to communicate
using ssh tunnels to a remote cloud server. Specifically, four tunnels are
created over an ssh connection to the remote server; three are used by
opsConsole, analysisConsole and xfer. The fourth enables a user to login
via ssh from the remote cloud server.

Iptable filters are configured to restrict access to the physs.
Default setup limits incoming traffic to ssh; everything else must
pass through tunnels.
