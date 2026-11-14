Elaro's application icon uses the EFL Elementary default theme's `chat` icon
(`i-lang-blank`). `elaro.svg` is the original artwork from
`data/elementary/themes/img/svg/lang-blank.svg` in EFL:
https://git.enlightenment.org/enlightenment/efl

Copyright EFL developers. Elementary is distributed under the LGPL; the upstream
license is included in `COPYING.elementary`.

Generate the transparent PNGs from the repository root with librsvg:

```sh
rsvg-convert --width=512 --height=512 --output=data/elaro.png data/images/elaro.svg
rsvg-convert --width=128 --height=128 --output=data/images/elaro-128.png data/images/elaro.svg
```

The 512-pixel version is also embedded in Elaro's EFL asset bundle. Both PNG
sizes are installed in the corresponding hicolor application-icon directories.
