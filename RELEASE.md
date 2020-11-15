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
### R2-2 (November 15, 2020)
----
* Added new ConvertPixelFormat mbbo record with choices of Mono16Low and Mono16High.
  This record controls how Mono12Packed and Mono12p pixel formats are decompressed.
  - Mono16Low means that the data is not left-shifted by 4 bits, so bits 12-15 are 0.
  - Mono16High means that the data is left-shifted by 4 bits, so bits 0-3 are 0.
  This requires ADGenICam R1-7 or later because that contains the code to handle the left-shift option.
* Replaced the LeftShift record with ShiftDir and ShiftBits.
  - ShiftDir selects the direction in which to shift UInt16 data.  Choices are None, Left, and Right.
  - ShiftBits controls the number of bits to shift.  Choices are 1-8.  This record is not used if ShiftDir=None.
  - This change allows complete control over how to shift Mono10, Mono12, and Mono16 pixel formats.
  - For example, FLIR cameras always left-shift Mono16 PixelFormat data so the most-significant bit 
    is in bit 15, and the data range is 0-65535, even if the ADC is only 10 or 12 bits.
  - In some cases it may be preferable to right-shift this data so that it only spans the 
    actual range of the ADC. In the case of a 12-bit ADC this would be done with ShiftDir=Right and ShiftBits=4.
    The data will then span the range 0-4095.
  - Note that when decompressing Mono12Packed and Mono12p it may still be desirable to
    use ShiftDir and ShiftBits.  For example on FLIR cameras if AdcBitDepth=10 then the data unpacked
    with ConvertPixelFormat will be 12 bits with a range of 0-4095, with bits 0 and 1 both 0.
    By using ShiftDir=Right and ShiftBits=2 then the data will be truly 10 bits, with a range of 0-1023.

### R2-1 (October 2, 2020)
----
* Added support for PixelFormat=Mono12Packed and Mono12p.
  This requires ADGenICam R1-6 or later because that contains the code to decompress these formats.
  These formats send 2 12-bit pixel values in 3 bytes, rather than 4 bytes required with Mono16.
  This reduces network bandwidth and allows many cameras to run faster.
* Set PINI=1 on the ARLeftShift record.  It was not initializing with the autosaved value.

### R2-0 (September 20, 2020)
----
* Updated from aravis 0.7.2 to 0.8.1.  The aravis API has changed, so sites will need to update their
  local aravis installation to 0.8.1.
* Fixed a problem with GenICam boolean features.  Previously the code was calling the aravis functions
  for integer features, which did not work.  It was changed to call the functions for boolean features,
  and it now works correctly.
* Removed ARHWImageMode. This was presumably used to support cameras that did not support
  the GenICam AcquisitionMode feature.  It seems all cameras now support that, and it
  was not tested that it actually worked.
* Changed printf() calls to asynPrint.  This required adding a new method.
* Added an example of adding the GenICam feature PV $(P)cam1:GC_Gamma to autosave in
  the iocAravis directory.  The auto_settings.req and st.cmd.BlackflyS_13Y3M files were changed.

### R1-3 (September 3, 2020)
----
* Add .bob files for Phoebus Display Manager
* This is the last release that will use ARAVIS_0_7_2.
  The next release will require ARAVIS_0_8_0 which was recently released.

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
* This release requires release ARAVIS_0_7_2 aravis package. ARAVIS_0_7_2 is described as an "unstable" release.  
  Note that more recent releases (e.g ARAVIS_0_7_3) **cannot** be used because the API has changed from ARAVIS_0_7_2.
  Hopefully there will be a stable 0.8.x release in the near future.  
  * This release of aravis adds support for register caching.
  * This release of aravis has changes to the aravis API for several functions.

### R1-0 (August 12, 2019)
----
* Initial release

