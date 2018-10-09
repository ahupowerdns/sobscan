# Sobol and other statistical scans of the internet
Many projects make scans of the entire internet to count the number of
servers for certain protocols, or how these servers behave.

In contrast, this program attempts to not scan the entire internet, but
by making use of statistics, arrive at useful numbers with a minimal amount
of traffic.

This makes it possible to make frequent statistics without alarming amounts
of traffic

![Sample plot](sample/splot.png)

## Compiling
Requires a somewhat recent C++ compiler. You may have to issue:

```
git submodule update --init
```

To get two dependencies, and then run `make`.

## Prefixes file
In order to be up to date, the tool needs a list of announced prefixes.

A sample file `prefixes` is provided in the `sample` directory.  It can also
be generated like this from `bird` if you have a BGP feed:

```
# birdc show route | tail -n +1 | cut -f1 -d" " > prefixes
```

# Tools
`dnsscan` scans a tiny part of the internet for nameservers & open
resolvers. This tool is not functional for now.

`makemap` maps the internet.

## Making the internet map
`makemap` reads the prefixes and turns them into a 3D plot in a file called
`denso`. The format of this file is 'first-octet second-octet /24-count'.
Every /16 can contain at most 256 /24s.

Sample use:

```
$ ./makemap sample/prefixes
Have 704562 netmasks
2851277568 IPv4 addresses announced (66.3865%)
Writing data to file 'denso'

$ gnuplot
gnuplot> splot 'denso' u 1:2:3 palette
```

