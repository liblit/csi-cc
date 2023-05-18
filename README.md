# CSI: Crash Scene Investigation

An instrumenting compiler for lightweight program tracing. The mechanisms were originally proposed in

> P. Ohmann and B. Liblit.
“[Lightweight control-flow instrumentation and postmortem analysis in support of debugging](http://pages.cs.wisc.edu/~liblit/ase-journal-2016/).”
Automated Software Engineering Journal, 2016.  Springer.
DOI: 10.1007/s10515-016-0190-1

Preprint: http://pages.cs.wisc.edu/~liblit/ase-journal-2016/ase-journal-2016.pdf

## Current Release

[`csi-cc` v1.4.0](../../releases/tag/v1.4.0)

## Documentation

Documentation is included in the source download, or you can view the
[online documentation](https://pohmann.github.io/csi-cc).

## Changelog

### [v. 1.4.0](../../releases/tag/v1.4.0)

Release associated with the 2023 INFORMS JOC paper
"A Set Covering Approach to Customized Coverage Instrumentation."
Major changes include:

- Support for more LLVM releases (through 7.0)
- A new, more efficient optimal coverage solver ("LEMON"), as proposed in
the paper
- Support for the GNU Indirect Function attribute for run-time selection
among tracing schemes, as referenced in Peter Ohmann's PhD dissertation

### [v. 1.3.0](../../releases/tag/v1.3.0)

Release associated with the PLDI 2017 paper "Control-Flow Recovery from
Partial Failure Reports."  Major changes include:

- Support for more recent LLVM releases (through 4.0)
- Support for OSX 10.11+
- Tools for extracting annotated control-flow graphs for downstream analyses

### [v. 1.2.0](../../releases/tag/v1.2.0)

Release associated with the ASE 2016 paper "Optimizing Customized Program
Coverage."  Major changes include:

- Support for various levels of optimization for statement and call-site
coverage instrumentation (see the
[online documentation](https://rawgit.com/pohmann/csi-cc/master/doc/running_optimization.html))

### [v. 1.1.0](../../releases/tag/v1.1.0)

Release associated with the article from the Automated Software Engineering
journal (2016).  Major changes include:

- Support for function, call-site, and statement coverage instrumentation
- Support for more customizable tracing (see the
[online documentation](https://rawgit.com/pohmann/csi-cc/master/doc/running_schemes.html))

### [v. 1.0.0](../../releases/tag/v1.0.0)

Initial release.
