# CSI: Crash Scene Investigation

An instrumenting compiler for lightweight program tracing. The mechanisms were originally proposed in

> P. Ohmann and B. Liblit.
“[Lightweight control-flow instrumentation and postmortem analysis in support of debugging](http://pages.cs.wisc.edu/~liblit/ase-2016/).”
Automated Software Engineering Journal (to appear), 2016.  Springer.
DOI: 10.1007/s10515-016-0190-1

Preprint: http://pages.cs.wisc.edu/~liblit/ase-2016/ase-2016.pdf

## Current Release

[`csi-cc` v1.2.0](../../releases/tag/v1.2.0)

## Documentation

Documentation is included in the source download, or you can view the
[online documentation](https://rawgit.com/liblit/csi-cc/master/doc/index.html).

## Changelog

###[v. 1.2.0](../../releases/tag/v1.2.0)

Release associated with the ASE 2016 paper "Optimizing Customized Program
Coverage."  Major changes include:

- Support for various levels of optimization for statement and call-site
coverage instrumentation (see the
[online documentation](https://rawgit.com/liblit/csi-cc/master/doc/running_optimization.html))

###[v. 1.1.0](../../releases/tag/v1.1.0)

Release associated with the article from the Automated Software Engineering
journal (2016).  Major changes include:

- Support for function, call-site, and statement coverage instrumentation
- Support for more customizable tracing (see the
[online documentation](https://rawgit.com/liblit/csi-cc/master/doc/running_schemes.html))

###[v. 1.0.0](../../releases/tag/v1.0.0)

Initial release.
