# Public transport routing algorithms

C++ implementation of the RAPTOR public transport routing algorithm, with more algorithms possibly coming in the future.

This is a project I develop in my spare time as a hobby.
The goal is to create performant and good quality code utilising C++23 features.

## Implementation details

The library works by instantitating all trips and routes of a public transport network for a given time period.
The internal storage is independent of the underlying format of the feed, making it possible to import various feed formats.
An import function for GTFS feeds is provided.

### Transfers

On-foot transfers are currently calculated by approximating the straight line distance between two stops and estimating the walking time.
It is possible to provide another function for calculating the walking distance between two stops (for example by examining OpenStreetMap data).

## Implemented algorithms

### [RAPTOR](https://www.microsoft.com/en-us/research/wp-content/uploads/2012/01/raptor_alenex.pdf)

The original version of RAPTOR has been implemented, as it is presented in the paper.
For this reason, walking transfers between different platforms and stops are not counted as transfers, in the way those are defined in the paper.

## Possible future algorithms

* [Connection Scan Algorithm](https://arxiv.org/pdf/1703.05997)
* [Transfer Patterns](https://link.springer.com/chapter/10.1007/978-3-642-15775-2_25)


## License

All code in this repository is provided under the GPLv3 license.
