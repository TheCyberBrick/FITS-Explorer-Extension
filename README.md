# FITS Explorer Extension

This application enables the Windows Explorer to show thumbnails for .fit/.fits images which are commonly used in astrophotography.

##### Features
- Auto stretch based on the PixInsight algorithm
- Auto debayer if the CFA pattern is specified in the FITS header ("BAYERPAT")
- Exposes FITS header information (e.g. exposure time, aperture, etc.) to file tooltips and the file properties details page
- Color-coded outlines on mono images if the FITS header specifies the (LRGB) filter type

## License

This project is released under the GNU Lesser General Public License v2.1.
Third party libraries and code used in this project:

- [cfitsio](https://github.com/healpy/cfitsio/blob/master/License.txt): Loading/parsing FITS files.
- [QuickLook.Plugin.FitsViewer](https://github.com/siyu6974/QuickLook.Plugin.FitsViewer): Implementation of PixInsight stretch. Licensed under LGPL v2.1.
- [Windows classic samples](https://github.com/microsoft/Windows-classic-samples): Samples for Windows COM servers and shell extensions. Licensed under the MIT License.

## Demo
![Demo](resources/demo.png)