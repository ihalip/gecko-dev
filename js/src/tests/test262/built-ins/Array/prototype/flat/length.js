// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flat
description: Array.prototype.flat.length value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [Array.prototype.flat]
---*/

assert.sameValue(
  Array.prototype.flat.length, 0,
  'The value of `Array.prototype.flat.length` is `0`'
);

verifyNotEnumerable(Array.prototype.flat, 'length');
verifyNotWritable(Array.prototype.flat, 'length');
verifyConfigurable(Array.prototype.flat, 'length');

reportCompare(0, 0);
