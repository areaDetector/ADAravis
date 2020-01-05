ADAravis Releases
===================

The latest untagged master branch can be obtained at
https://github.com/areaDetector/ADAravis.

The versions of EPICS base, asyn, and other synApps modules used for each release can be obtained from 
the EXAMPLE_RELEASE_PATHS.local, EXAMPLE_RELEASE_LIBS.local, and EXAMPLE_RELEASE_PRODS.local
files respectively, in the configure/ directory of the appropriate release of the 
[top-level areaDetector](https://github.com/areaDetector/areaDetector) repository.


Release Notes
=============

### R1-2 (January 5, 2020)
----
* Change arvFeature support for GenICam features from int (32-bit) to epicsInt64 (64-bit)


### R1-1 (October 20, 2019)
----
* Added support for register caching
  * This improves the performance by over a factor of 100 when setting a feature, because ADGenICam reads
    all of the features when any feature was changed.
  * The constructor and aravisConfig have a new enableCaching argument to enable or disable register caching.
    It should be set to 1 except perhaps for some unusual cameras that do not correctly handle register caching.
* This release requires a recent release of the aravis package, ARAVIS_0_7_2 or later.
  ARAVIS_0_7_2 is described as an "unstable" release.  There should be a stable 0.8.x in the near future.
  * This release of aravis adds support for register caching.
  * This release of aravis has changes to the aravis API for several functions.

### R1-0 (August 12, 2019)
----
* Initial release

