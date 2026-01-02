---
title: '9. Raw Media Interfaces (Part 4 of 4)'
---

> Section 9 from [W3C WebCodecs Specification](https://www.w3.org/TR/webcodecs/)

**Part 4 of 4**

---

dictionary `VideoColorSpaceInit` {
[VideoColorPrimaries](https://www.w3.org/TR/webcodecs/#enumdef-videocolorprimaries)? `primaries` = null;
[VideoTransferCharacteristics](https://www.w3.org/TR/webcodecs/#enumdef-videotransfercharacteristics)? `transfer` = null;
[VideoMatrixCoefficients](https://www.w3.org/TR/webcodecs/#enumdef-videomatrixcoefficients)? `matrix` = null;
[boolean](https://webidl.spec.whatwg.org/#idl-boolean)? `fullRange` = null;
};

````

#### 9.9.1. Internal Slots[](https://www.w3.org/TR/webcodecs/#videocolorspace-internal-slots)

`[[primaries]]`

The color primaries.

`[[transfer]]`

The transfer characteristics.

`[[matrix]]`

The matrix coefficients.

`[[full range]]`

Indicates whether full-range color values are used.

#### 9.9.2. Constructors[](https://www.w3.org/TR/webcodecs/#videocolorspace-constructors)

`VideoColorSpace(init)`

1.  Let c be a new [VideoColorSpace](https://www.w3.org/TR/webcodecs/#videocolorspace) object, initialized as follows:

    1.  Assign `init.primaries` to [[[primaries]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-primaries-slot).

    2.  Assign `init.transfer` to [[[transfer]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-transfer-slot).

    3.  Assign `init.matrix` to [[[matrix]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-matrix-slot).

    4.  Assign `init.fullRange` to [[[full range]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-full-range-slot).

2.  Return c.


#### 9.9.3. Attributes[](https://www.w3.org/TR/webcodecs/#videocolorspace-attributes)

`primaries`, of type [VideoColorPrimaries](https://www.w3.org/TR/webcodecs/#enumdef-videocolorprimaries), readonly, nullable

The [primaries](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-primaries) getter steps are to return the value of [[[primaries]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-primaries-slot).

`transfer`, of type [VideoTransferCharacteristics](https://www.w3.org/TR/webcodecs/#enumdef-videotransfercharacteristics), readonly, nullable

The [transfer](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-transfer) getter steps are to return the value of [[[transfer]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-transfer-slot).

`matrix`, of type [VideoMatrixCoefficients](https://www.w3.org/TR/webcodecs/#enumdef-videomatrixcoefficients), readonly, nullable

The [matrix](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-matrix) getter steps are to return the value of [[[matrix]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-matrix-slot).

`fullRange`, of type [boolean](https://webidl.spec.whatwg.org/#idl-boolean), readonly, nullable

The [fullRange](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-fullrange) getter steps are to return the value of [[[full range]]](https://www.w3.org/TR/webcodecs/#dom-videocolorspace-full-range-slot).

### 9.10. Video Color Primaries[](https://www.w3.org/TR/webcodecs/#videocolorprimaries)

```webidl
enum `VideoColorPrimaries` {
  ["bt709"](https://www.w3.org/TR/webcodecs/#dom-videocolorprimaries-bt709),
  ["bt470bg"](https://www.w3.org/TR/webcodecs/#dom-videocolorprimaries-bt470bg),
  ["smpte170m"](https://www.w3.org/TR/webcodecs/#dom-videocolorprimaries-smpte170m),
  ["bt2020"](https://www.w3.org/TR/webcodecs/#dom-videocolorprimaries-bt2020),
  ["smpte432"](https://www.w3.org/TR/webcodecs/#dom-videocolorprimaries-smpte432),
};
````

`bt709`

Color primaries used by BT.709 and sRGB, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.1 table 2 value 1.

`bt470bg`

Color primaries used by BT.601 PAL, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.1 table 2 value 5.

`smpte170m`

Color primaries used by BT.601 NTSC, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.1 table 2 value 6.

`bt2020`

Color primaries used by BT.2020 and BT.2100, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.1 table 2 value 9.

`smpte432`

Color primaries used by P3 D65, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.1 table 2 value 12.

### 9.11. Video Transfer Characteristics[](https://www.w3.org/TR/webcodecs/#videotransfercharacteristics)

```webidl
enum `VideoTransferCharacteristics` {
  ["bt709"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-bt709),
  ["smpte170m"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-smpte170m),
  ["iec61966-2-1"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-iec61966-2-1),
  ["linear"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-linear),
  ["pq"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-pq),
  ["hlg"](https://www.w3.org/TR/webcodecs/#dom-videotransfercharacteristics-hlg),
};
```

`bt709`

Transfer characteristics used by BT.709, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 1.

`smpte170m`

Transfer characteristics used by BT.601, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 6. (Functionally the same as "bt709".)

`iec61966-2-1`

Transfer characteristics used by sRGB, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 13.

`linear`

Transfer characteristics used by linear RGB, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 8.

`pq`

Transfer characteristics used by BT.2100 PQ, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 16.

`hlg`

Transfer characteristics used by BT.2100 HLG, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.2 table 3 value 18.

### 9.12. Video Matrix Coefficients[](https://www.w3.org/TR/webcodecs/#videomatrixcoefficients)

```webidl
enum `VideoMatrixCoefficients` {
  ["rgb"](https://www.w3.org/TR/webcodecs/#dom-videomatrixcoefficients-rgb),
  ["bt709"](https://www.w3.org/TR/webcodecs/#dom-videomatrixcoefficients-bt709),
  ["bt470bg"](https://www.w3.org/TR/webcodecs/#dom-videomatrixcoefficients-bt470bg),
  ["smpte170m"](https://www.w3.org/TR/webcodecs/#dom-videomatrixcoefficients-smpte170m),
  ["bt2020-ncl"](https://www.w3.org/TR/webcodecs/#dom-videomatrixcoefficients-bt2020-ncl),
};
```

`rgb`

Matrix coefficients used by sRGB, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.3 table 4 value 0.

`bt709`

Matrix coefficients used by BT.709, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.3 table 4 value 1.

`bt470bg`

Matrix coefficients used by BT.601 PAL, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.3 table 4 value 5.

`smpte170m`

Matrix coefficients used by BT.601 NTSC, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.3 table 4 value 6. (Functionally the same as "bt470bg".)

`bt2020-ncl`

Matrix coefficients used by BT.2020 NCL, as described by [\[H.273\]](https://www.w3.org/TR/webcodecs/#biblio-h273) section 8.3 table 4 value 9.
