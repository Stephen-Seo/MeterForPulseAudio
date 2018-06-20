# Version 1.7

Fix listing sinks/sources to halt even when querying failed (previously, only
halted on successful query of all sinks/sources).

# Version 1.6

Improve options "--list-sinks" and "--list-sources" (now halts after querying
for sinks/sources has finished instead of waiting for 1 second to pass).

# Version 1.5

Color of previous max amount in meter is inverted after a certain threshold
(0.98).  
Add markings on meter (can be disabled in program arguments).  
Minor cleanup.

# Version 1.4

Fix bug involving channel size.

# Version 1.3

Added feature to print available sinks/sources (invoked via arguments to
program).

# Version 1.2

Improved error output when failed getting info on sink/source.

# Version 1.1

Fixed error output on invalid sink/source and invalid arguments to program.

# Version 1.0

Initial working release. Check the help text for usage.
